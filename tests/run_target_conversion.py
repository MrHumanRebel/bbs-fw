"""Compare shipped conversion with legacy formulas over their reachable inputs."""
import argparse
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import tempfile
import time


def verify(sdcc, simulator, controller):
    root = Path(__file__).resolve().parents[1]
    source = (root / "src/firmware/app.c").read_text()
    start = source.rindex("uint16_t convert_wheel_speed_kph_to_rpm(")
    function = source[start:]  # This is the last function in app.c.
    harness = r'''
#include <stdint.h>
#include "util.h"
struct { uint8_t wheel_size_inch_x10_u16h, wheel_size_inch_x10_u16l; } g_config;
'''
    harness += "#define " + controller + " 1\n" + function
    harness += r'''
volatile __at(0x100) uint16_t failure[5];
volatile uint8_t result;
void passed(void) { result = 1; for (;;) {} }
void failed(void) { result = 2; for (;;) {} }
void main(void)
{
    for (uint16_t wheel = 100; wheel <= 400; ++wheel)
    {
        g_config.wheel_size_inch_x10_u16h = wheel >> 8;
        g_config.wheel_size_inch_x10_u16l = wheel;
        for (uint16_t speed = 0; speed <= 255; ++speed)
        {
            float radius = wheel * 1.27f;
            uint16_t old_app = (uint16_t)(25000.f / (3 * 3.14159f * radius) * speed);
            uint16_t old_display = (uint16_t)(25000.f / (3 * 3.14159f * 1.27f * wheel) * speed);
            uint16_t actual = convert_wheel_speed_kph_to_rpm(speed);
            /* Display data is at most 127 C, scaled by 161/100 for imperial units: 204.
               Current/SOC percentages are at most 100. Higher values are unreachable. */
            if (actual != old_app || (speed <= 204 && actual != old_display)) {
                failure[0] = wheel; failure[1] = speed; failure[2] = actual;
                failure[3] = old_app; failure[4] = old_display;
                failed();
            }
        }
    }
    passed();
}
'''
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        cfile = directory / "conversion.c"
        cfile.write_text(harness)
        flags = ["-mstm8"] if controller == "TSDZ2" else ["-mmcs51", "--model-large"]
        subprocess.run([sdcc, *flags, "--std-c99", "-I" + str(root / "src/firmware"),
                        str(cfile), "-o", str(directory / "conversion.ihx")], check=True)
        symbols = (directory / "conversion.map").read_text()
        passed = int(re.search(r"([0-9A-F]+)\s+_passed\s", symbols)[1], 16)
        failed = int(re.search(r"([0-9A-F]+)\s+_failed\s", symbols)[1], 16)
        master, slave = pty.openpty()
        process = subprocess.Popen([simulator, "-t", "STM8S105" if controller == "TSDZ2" else "C52", "-q", "-c", "-",
                                    str(directory / "conversion.ihx")],
                                   stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)
        output = b""
        try:
            # uCsim initializes the terminal with tcflush; wait before sending commands.
            startup_deadline = time.monotonic() + 15
            while b"0> " not in output:
                readable, _, _ = select.select([master], [], [], 1)
                if readable: output += os.read(master, 65536)
                if time.monotonic() > startup_deadline:
                    raise TimeoutError("Simulator console did not become ready: " + output.decode(errors="replace"))
            print(f"Executing SDCC {controller} comparison for 77,056 input pairs...", flush=True)
            os.write(master, f"break 0x{passed:x}\nbreak 0x{failed:x}\nrun\n".encode())
            deadline = time.monotonic() + 600
            while time.monotonic() < deadline:
                readable, _, _ = select.select([master], [], [], 1)
                if readable:
                    output += os.read(master, 65536)
                    stop = re.search(rb"Stop at 0x([0-9a-fA-F]+)", output)
                    if stop:
                        if int(stop[1], 16) != passed:
                            os.write(master, b"dump rom 0x100 0x109\n")
                            time.sleep(0.1)
                            while select.select([master], [], [], 0)[0]: output += os.read(master, 65536)
                            raise AssertionError("Target conversion differs from a legacy formula: " + output.decode(errors="replace"))
                        print(f"{controller} SDCC/uCsim: 77,056 app inputs and 61,705 reachable display inputs match legacy formulas: PASS")
                        return
                if process.poll() is not None:
                    raise RuntimeError("Simulator exited before reaching a result")
            raise TimeoutError("Target conversion comparison exceeded 600 seconds: " + output.decode(errors="replace"))
        finally:
            process.terminate()
            process.wait(timeout=10)
            os.close(master)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdcc", default="sdcc")
    parser.add_argument("--simulator")
    parser.add_argument("--controller", choices=("BBS02", "BBSHD", "TSDZ2"), default="TSDZ2")
    arguments = parser.parse_args()
    simulator = arguments.simulator or ("ucsim_stm8" if arguments.controller == "TSDZ2" else "ucsim_51")
    verify(arguments.sdcc, simulator, arguments.controller)
