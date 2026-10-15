"""Validate the complete release payload and generate its download checksums."""
import argparse
import hashlib
import re
import struct
from pathlib import Path


def validate_hex(path):
    lines = path.read_text(encoding="ascii").splitlines()
    if not lines or lines[-1] != ":00000001FF":
        raise ValueError(f"{path}: missing Intel HEX EOF")
    data_size = 0
    for index, line in enumerate(lines):
        if not line.startswith(":"):
            raise ValueError(f"{path}: invalid record prefix")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) % 256:
            raise ValueError(f"{path}: invalid Intel HEX length/checksum")
        if record[3] == 1 and index != len(lines) - 1:
            raise ValueError(f"{path}: data after EOF")
        if record[3] == 0:
            data_size += record[0]
    if not data_size:
        raise ValueError(f"{path}: no firmware data")


def validate_exe(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError(f"{path}: not a Windows executable")
    offset = struct.unpack_from("<I", data, 60)[0]
    if offset + 6 > len(data) or data[offset:offset + 4] != b"PE\0\0":
        raise ValueError(f"{path}: missing PE signature")
    if struct.unpack_from("<H", data, offset + 4)[0] != 0x8664:
        raise ValueError(f"{path}: expected Windows x64 executable")


def validate(directory, version):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Invalid release version")
    expected = [f"bbs-fw-v{version}-{controller}.hex" for controller in ("BBS02", "BBSHD", "TSDZ2")]
    expected.append(f"BBSFWTool-v{version}-win-x64.exe")
    actual = {p.name for p in directory.iterdir() if p.is_file() and p.name != "SHA256SUMS"}
    if actual != set(expected):
        raise ValueError(f"Incomplete/unexpected release assets: {actual ^ set(expected)}")
    for name in expected:
        path = directory / name
        (validate_hex if name.endswith(".hex") else validate_exe)(path)
    checksums = "".join(f"{hashlib.sha256((directory / name).read_bytes()).hexdigest()}  {name}\n"
                        for name in sorted(expected))
    (directory / "SHA256SUMS").write_text(checksums, encoding="ascii")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("version")
    args = parser.parse_args()
    validate(args.directory, args.version)
