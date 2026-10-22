"""Keep firmware within its pre-hardening footprint using the pinned SDCC toolchain.

These are regression budgets, not MCU ratings or measured peak-stack bounds.
"""
import argparse
import json
from pathlib import Path
import re

BUDGETS = {
    "BBS02": {"flash": 25805, "xram": 904, "stack_available": 150},
    "BBSHD": {"flash": 27361, "xram": 918, "stack_available": 138},
    "TSDZ2": {"code": 15655, "data": 587, "initialized": 61},
}


def require(pattern, text):
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise ValueError("Unrecognized SDCC memory report: " + pattern)
    return int(match[1])


def check(controller, directory):
    if controller == "TSDZ2":
        report = (directory / "bbs-fw.map").read_text()
        values = {name.lower(): require(r"^" + name + r"\s+[0-9A-F]+\s+[0-9A-F]+\s*=\s*(\d+)\. bytes", report)
                  for name in ("CODE", "DATA", "INITIALIZED")}
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
