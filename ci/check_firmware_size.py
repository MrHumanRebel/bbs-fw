"""Guard the reviewed Adaptive Efficiency flash allowance and zero-RAM-growth baseline.

These are regression budgets, not MCU ratings or measured peak-stack bounds.
"""
import argparse
import json
from pathlib import Path
import re

try:
    from .validate_release import validate_hex
except ImportError:  # Direct script execution.
    from validate_release import validate_hex

BUDGETS = {
    "BBS02": {"flash": 28064, "xram": 861, "stack_available": 156},
    "BBSHD": {"flash": 29648, "xram": 875, "stack_available": 144},
    "TSDZ2": {"code": 16000, "data": 581, "initialized": 61, "image_span": 16736},
}


def require(pattern, text):
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise ValueError("Unrecognized SDCC memory report: " + pattern)
    return int(match[1])


def flash_span(path, start):
    # Include vectors, startup and constants as well as CODE; a large const table
    # must not escape the STM8 regression budget by living in a different section.
    validate_hex(path)
    base = 0
    end = start
    for line in path.read_text().splitlines():
        record = bytes.fromhex(line[1:])
        kind = record[3]
        if kind == 4:
            base = int.from_bytes(record[4:-1], "big") << 16
        elif kind == 2:
            base = int.from_bytes(record[4:-1], "big") << 4
        elif kind == 0:
            address = base + int.from_bytes(record[1:3], "big")
            if address < start:
                raise ValueError("Firmware data appears below the target flash origin")
            end = max(end, address + record[0])
    return end - start


def check(controller, directory):
    if controller == "TSDZ2":
        report = (directory / "bbs-fw.map").read_text()
        values = {name.lower(): require(r"^" + name + r"\s+[0-9A-F]+\s+[0-9A-F]+\s*=\s*(\d+)\. bytes", report)
                  for name in ("CODE", "DATA", "INITIALIZED")}
        values["image_span"] = flash_span(directory / "bbs-fw.hex", 0x8000)
    else:
        report = (directory / "bbs-fw.mem").read_text()
        values = {
            "flash": require(r"ROM/EPROM/FLASH\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)", report),
            "xram": require(r"EXTERNAL RAM\s+0x[0-9a-f]+\s+0x[0-9a-f]+\s+(\d+)", report),
            "stack_available": require(r"with (\d+) bytes available", report),
        }
    for name, limit in BUDGETS[controller].items():
        value = values[name]
        if (value < limit if name == "stack_available" else value > limit):
            raise ValueError(f"{controller} {name}: {value} violates the regression budget {limit}")
    return values


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controller", choices=BUDGETS)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(json.dumps({args.controller: check(args.controller, args.directory)}, sort_keys=True))
