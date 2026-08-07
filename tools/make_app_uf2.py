"""Convert loadable ELF segments into a DISPLAY app UF2.

Unlike picotool's device-oriented conversion, this accepts external PSRAM
segments and ignores NOLOAD/NOBITS memory (p_filesz == 0).  Payload addresses
come from each ELF program header's physical address and are validated by the
same checker used by `fw install-app` immediately after generation.
"""
import argparse
import pathlib
import struct

UF2_MAGIC0 = 0x0A324655
UF2_MAGIC1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
RP2350_ARM_S_FAMILY_ID = 0xE48BFF59
PT_LOAD = 1
PAYLOAD_SIZE = 256


def load_segments(blob):
    if blob[:4] != b"\x7fELF" or blob[4:6] != b"\x01\x01":
        raise ValueError("expected a 32-bit little-endian ELF")
    phoff = struct.unpack_from("<I", blob, 28)[0]
    phentsize, phnum = struct.unpack_from("<HH", blob, 42)
    if phentsize < 32 or phoff + phentsize * phnum > len(blob):
        raise ValueError("ELF program-header table is truncated")
    segments = []
    for index in range(phnum):
        fields = struct.unpack_from("<8I", blob, phoff + index * phentsize)
        kind, offset, _vaddr, paddr, filesz, _memsz, _flags, _align = fields
        if kind != PT_LOAD or filesz == 0:
            continue
        if offset + filesz > len(blob):
            raise ValueError("ELF load segment is truncated")
        segments.append((paddr, blob[offset:offset + filesz]))
    if not segments:
        raise ValueError("ELF contains no loadable bytes")
    return sorted(segments)


def make_uf2(blob):
    chunks = []
    for address, data in load_segments(blob):
        for offset in range(0, len(data), PAYLOAD_SIZE):
            chunks.append((address + offset, data[offset:offset + PAYLOAD_SIZE]))
    blocks = []
    for number, (address, payload) in enumerate(chunks):
        header = struct.pack("<8I", UF2_MAGIC0, UF2_MAGIC1,
                             UF2_FLAG_FAMILY_ID, address, len(payload),
                             number, len(chunks), RP2350_ARM_S_FAMILY_ID)
        blocks.append(header + payload + bytes(476 - len(payload)) +
                      struct.pack("<I", UF2_MAGIC_END))
    return b"".join(blocks)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("elf")
    parser.add_argument("uf2")
    args = parser.parse_args()
    blob = pathlib.Path(args.elf).read_bytes()
    pathlib.Path(args.uf2).write_bytes(make_uf2(blob))


if __name__ == "__main__":
    main()
