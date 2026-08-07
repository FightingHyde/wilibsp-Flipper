#!/usr/bin/env python3
"""Validate that a UF2 is safe for the DISPLAY /apps loader."""

import argparse
import pathlib
import struct

from fw import check_app_uf2
from check_uf2_info import check as check_uf2_info


def load_image(path):
    """Reconstruct loadable bytes in address order, including block joins."""
    data = pathlib.Path(path).read_bytes()
    chunks = []
    for offset in range(0, len(data), 512):
        block = data[offset:offset + 512]
        _m0, _m1, flags, address, size = struct.unpack_from("<5I", block)
        if not (flags & 1) and size:
            chunks.append((address, block[32:32 + size]))
    chunks.sort()
    if not chunks:
        return b""
    base = chunks[0][0]
    end = max(address + len(payload) for address, payload in chunks)
    image = bytearray(end - base)
    for address, payload in chunks:
        image[address - base:address - base + len(payload)] = payload
    return bytes(image)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("uf2")
    args = parser.parse_args()
    target = check_app_uf2(args.uf2)
    name, version, description = check_uf2_info(load_image(args.uf2))
    print(f"app UF2 target: {target}")
    print(f"FW2 app: {name} v{version:03d} - {description}")


if __name__ == "__main__":
    main()
