"""Compile the production application with mock I/O and execute it on each MCU ISA.

No probes, buffers or statistics are linked into the production firmware.
"""
import argparse
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import tempfile
import time


def verify(controller, sdcc, simulator, baseline=None, timing_sample=False):
    root = Path(__file__).resolve().parents[1]
    fixture = (root / 'tests/pas_voltage_test.c').read_text().split('int main(')[0]
    for header in ('assert.h', 'stdio.h', 'string.h'):
        fixture = fixture.replace(f'#include <{header}>', '')
    fixture = fixture.replace('#define __SDCC 1', '')
    fixture = fixture.replace('"../src/firmware/', '"' + str(root / 'src/firmware') + '/')
    harness = fixture + r'''
volatile uint8_t failure_code;
void passed(void) { failure_code = 0; for (;;) {} }
void failed(void) { for (;;) {} }
#define CHECK(x, code) do { if (!(x)) { failure_code = code; failed(); } } while (0)
void probe_begin(void) {}
uint8_t probe_lvc(bool recover)
{
    now += 130;
    requested_current = 100;
#ifdef BASELINE_PROBE
    apply_low_voltage_limit();
#else
    apply_low_voltage_limit(recover);
#endif
    return requested_current;
}
void main(void)
{
    setup();
    g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE | SYSTEM_EFFICIENCY_COMFORT;
    g_config.use_speed_sensor = true;
    g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY;
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = 25;
    start();
    /* Probe the actual bounded tracker, not a floating-point reimplementation. */
    probe_begin();
    for (uint8_t smooth = 0; smooth <= 100; ++smooth)
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = smooth;
        uint8_t value = 0;
        now = 0xfffffff0ul;
        test_ramp_up(&value, false);
        for (uint16_t step = 1; step <= 200; ++step)
        {
            now += 5;
            uint8_t previous = value;
            value = step <= 80 ? 20 : 100;
            test_ramp_up(&value, true);
            CHECK(value <= previous + 2u && value <= (step <= 80 ? 20 : 100), 1);
        }
        CHECK(value >= 25, 2); /* Includes the 405 ms retarget and clock wrap. */
        value = 0; test_ramp_up(&value, true); CHECK(value == 0, 3);
    }
    /* A one-percent request oscillation cannot postpone a scheduled rise. */
    g_config.current_ramp_amps_s = 100;
    g_config.current_ramp_smoothness_percent = 0;
    uint8_t jitter = 20;
    now = 0; test_ramp_up(&jitter, false);
    now = 5; jitter = 20; test_ramp_up(&jitter, true);
    now = 10; jitter = 21; test_ramp_up(&jitter, true);
    now = 15; jitter = 20; test_ramp_up(&jitter, true);
    now = 20; jitter = 100; test_ramp_up(&jitter, true);
#ifndef BASELINE_PROBE
    CHECK(jitter > 20 && jitter <= 28, 14);
#endif
    g_config.current_ramp_amps_s = 100;
#if defined(BBSHD)
    g_config.max_current_amps = 30;
#elif defined(TSDZ2)
    g_config.max_current_amps = 20;
#else
    g_config.max_current_amps = 25;
#endif
    pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 - 10u;
    wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4u / 5u;
    reload_assist_params(); run(2995);
#ifndef BASELINE_PROBE
    CHECK(assist_level_data.pas.efficiency.scale == 100, 11); /* Preserve launch grace. */
#endif
    run(3005);
#ifndef BASELINE_PROBE
    CHECK(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE, 4);
    CHECK(assist_level_data.pas.efficiency.launch & 0x80u, 12); /* 250 W activates below the 97% speed trim. */
#endif
    uint8_t base = adaptive_pas_current();
    CHECK(base < 40 && motor_current <= base, 5);
    /* 500 W also demonstrates a real command reduction on BBSHD, where the
       five-percent trim at 250 W is smaller than one current-command step. */
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = 50;
    reload_assist_params(); run(6000);
#ifndef BASELINE_PROBE
    CHECK(motor_current < adaptive_pas_current(), 13);
#endif
    brake = true; run(5); CHECK(motor_current == 0, 6);
    brake = false; run(3000);
    voltage = g_config.low_cut_off_v * 10u; run(5); CHECK(motor_current == 0, 7);
    voltage = 480; pedal_cadence = 600; run(3000);
    temperature = 8501; run(5); CHECK(motor_current == 0, 8);
    temperature = 0; run(3000);
    pedaling = false; run(5); CHECK(motor_current == 0, 9);
#if HAS_TORQUE_SENSOR
    pedaling = true; torque_valid = false; run(5); CHECK(motor_current == 0, 10);
#endif
    /* Sample the real soft-LVC filter on the target ISA, including its dwell. */
    voltage = 440;
    uint8_t cap = 100;
    for (uint8_t sample = 0; sample < 150; ++sample) cap = probe_lvc(true);
    CHECK(cap > 20 && cap < 90, 15);
    voltage = 480;
    for (uint8_t sample = 0; sample < 7; ++sample) CHECK(probe_lvc(true) == cap, 16);
#ifndef BASELINE_PROBE
    CHECK(probe_lvc(true) > cap, 17);
#else
    probe_lvc(true);
#endif
    for (uint8_t sample = 0; sample < 100; ++sample) probe_lvc(true);
    voltage = g_config.low_cut_off_v * 10u;
    CHECK(probe_lvc(true) == 0, 18);
    voltage = 480; temperature = 0; torque_valid = true; pedaling = true;
    g_config.assist_levels[0][1].flags |= ASSIST_FLAG_THROTTLE;
    reload_assist_params();
    /* Nonlinear speed shaping is PAS-only; 100% throttle still overrides. */
    wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 - speed_limit_ramp_interval_rpm_x10 / 3u;
    uint8_t eased = 100;
    test_speed_limit(&eased, 0, true, false);
#ifndef BASELINE_PROBE
    CHECK(eased >= 20 && eased <= 23, 19);
#endif
    eased = 100; test_speed_limit(&eased, 100, true, true);
    CHECK(eased >= 32 && eased <= 34, 20);
    wheel_speed = assist_level_data.max_wheel_speed_rpm_x10;
    eased = 100; test_speed_limit(&eased, 0, true, false); CHECK(eased == 0, 21);
    wheel_speed = 0; throttle = 100; run(5);
#ifndef BASELINE_PROBE
    CHECK(motor_current == 100, 22);
#endif
    brake = true; run(5); CHECK(motor_current == 0, 23);
    passed();
}
'''
    if timing_sample:
        harness = harness.replace('++smooth', 'smooth += 50')
    with tempfile.TemporaryDirectory() as work:
        work = Path(work)
        source = work / 'control.c'
        if baseline:
            old_app = work / 'baseline-app.c'
            old_app.write_text(subprocess.check_output(['git', 'show', baseline + ':src/firmware/app.c'], cwd=root, text=True))
            harness = '#define BASELINE_PROBE 1\n' + harness.replace('"' + str(root / 'src/firmware/app.c') + '"', '"' + str(old_app) + '"')
        source.write_text(harness)
        flags = ['-mstm8'] if controller == 'TSDZ2' else ['-mmcs51', '--model-large']
        subprocess.run([sdcc, *flags, '--std-c99', '-D' + controller, '-I' + str(root / 'src/firmware'),
                        str(source), '-o', str(work / 'control.ihx')], check=True, capture_output=True, text=True)
        symbols = (work / 'control.map').read_text()
        addresses = {name: int(re.search(r'([0-9A-F]+)\s+_' + name + r'\s', symbols)[1], 16)
                     for name in ('passed', 'failed', 'failure_code', 'probe_begin')}
        master, slave = pty.openpty()
        process = subprocess.Popen([simulator, '-t', 'STM8S105' if controller == 'TSDZ2' else 'C52', '-q', '-c', '-',
                                    str(work / 'control.ihx')],
                                   stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)
        def receive(timeout=15, startup=False):
            output = b''
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]: output += os.read(master, 65536)
                if (startup and b'0> ' in output) or re.search(rb'0> \s*$', output): return output.decode(errors='replace')
                if process.poll() is not None: raise RuntimeError('Simulator exited without a result')
            raise TimeoutError(output.decode(errors='replace'))

        def command(value, timeout=15):
            os.write(master, (value + '\n').encode())
            return receive(timeout)

        try:
            receive(startup=True)
            if controller == 'TSDZ2':
                command('expression SP=0x7ff')
                command('set memory cpu_0_cfg 0 0x3ff')
            for name in ('probe_begin', 'passed', 'failed'):
                command(f"break 0x{addresses[name]:x}")
            output = command('run', 120)
            stop = re.search(r'Stop at 0x([0-9a-fA-F]+)', output)
            if not stop or int(stop[1], 16) != addresses['probe_begin']:
                raise AssertionError(output)
            sp = int(re.search(r'SP\s*=?\s*0x([0-9a-fA-F]+)', output)[1], 16)
            # uCsim's generic STM8 model starts at 0x17ff with an artificial
            # 0x14ff stack limit. Use the S105's real 2 KiB RAM top and a
            # conservative 0x3ff lower stack boundary; overflow errors remain ON.
            if controller == 'TSDZ2' and sp > 0x7ff:
                raise AssertionError('STM8 probe escaped the physical 2 KiB RAM')
            memory = 'rom' if controller == 'TSDZ2' else 'iram'
            low, high = (0x400, sp) if controller == 'TSDZ2' else (sp + 1, 0xff)
            command(f'fill {memory} 0x{low:x} 0x{high:x} 0xa5')
            cycle_start = int(re.search(r'Total time[^\n]*\((\d+) clks\)', command('state'))[1])
            output = command('run', 120)
            stop = re.search(r'Stop at 0x([0-9a-fA-F]+)', output)
            if not stop or int(stop[1], 16) != addresses['passed']:
                failure_memory = 'rom' if controller == 'TSDZ2' else 'xram'
                output += command(f"dump {failure_memory} 0x{addresses['failure_code']:x} 0x{addresses['failure_code']:x}")
                raise AssertionError(output)
            cycle_end = int(re.search(r'Total time[^\n]*\((\d+) clks\)', command('state'))[1])
            cycles = cycle_end - cycle_start
            if cycles <= 0: raise AssertionError('Invalid simulator clock accounting')
            dump = command(f'dump {memory} 0x{low:x} 0x{high:x}')
            touched = []
            for line in dump.splitlines():
                match = re.match(r'0x([0-9a-fA-F]+)\s+((?:[0-9a-fA-F]{2}\s+)+)', line)
                if match:
                    address = int(match[1], 16)
                    touched.extend(address + offset for offset, value in enumerate(match[2].split()) if value.lower() != 'a5')
            watermark = (sp - min(touched) + 1 if controller == 'TSDZ2' else max(touched) - sp) if touched else 0
            if watermark >= high - low - 16: raise AssertionError('Probe exhausted its stack watermark region')
            label = (baseline or 'working tree') + ('; 3-shape timing sample' if timing_sample else '; all 101 shapes')
            print(f'{controller} ({label}): SDCC/uCsim control checks: PASS; probe stack extension {watermark} bytes; {cycles} simulator cycles (no interrupts)')
            print(output)

        finally:
            process.terminate()
            process.wait(timeout=10)
            os.close(master)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controller', choices=('BBS02', 'BBSHD', 'TSDZ2'), required=True)
    parser.add_argument('--sdcc', default='sdcc')
    parser.add_argument('--simulator')
    parser.add_argument('--baseline-ref', help='Offline comparison against an existing Git revision; no production instrumentation')
    parser.add_argument('--timing-sample', action='store_true', help='Short offline timing comparison at smoothness 0, 50 and 100')
    args = parser.parse_args()
    verify(args.controller, args.sdcc, args.simulator or ('ucsim_stm8' if args.controller == 'TSDZ2' else 'ucsim_51'), args.baseline_ref, args.timing_sample)
