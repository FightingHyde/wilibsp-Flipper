"""Wrap a contiguous binary as RP2350 Arm Secure UF2 blocks in PSRAM."""

from __future__ import annotations

import pathlib
import struct
import sys

BASE = 0x11000000
PAYLOAD = 256
MAGIC0 = 0x0A324655
MAGIC1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
FLAG_FAMILY_ID = 0x00002000
RP2350_ARM_S_FAMILY_ID = 0xE48BFF59


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: make_uf2.py INPUT.bin OUTPUT.uf2")
    data = pathlib.Path(sys.argv[1]).read_bytes()
    if not data:
        raise SystemExit("refusing to create an empty UF2")
    count = (len(data) + PAYLOAD - 1) // PAYLOAD
    blocks = []
    for number in range(count):
        payload = data[number * PAYLOAD : (number + 1) * PAYLOAD]
        payload += bytes(PAYLOAD - len(payload))
        header = struct.pack(
            "<8I",
            MAGIC0,
            MAGIC1,
            FLAG_FAMILY_ID,
            BASE + number * PAYLOAD,
            PAYLOAD,
            number,
            count,
            RP2350_ARM_S_FAMILY_ID,
        )
        blocks.append(header + payload + bytes(476 - PAYLOAD) + struct.pack("<I", MAGIC_END))
    pathlib.Path(sys.argv[2]).write_bytes(b"".join(blocks))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
