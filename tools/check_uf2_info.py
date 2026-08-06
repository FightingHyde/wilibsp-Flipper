"""Validate the FW2 app-info record in a linked flat binary."""
import argparse
import struct
import sys
import zlib

MAGIC = b"FW2AINFO"
SIZE = 216
FULL = "<8s H B B H H 32s 128s 32s I I"


class RecordError(Exception):
    pass


def text(raw, label):
    if b"\0" not in raw:
        raise RecordError("%s is not NUL-terminated" % label)
    return raw.split(b"\0", 1)[0].decode("ascii", "strict")


def check(blob):
    offsets = []
    start = 0
    while True:
        offset = blob.find(MAGIC, start)
        if offset < 0:
            break
        offsets.append(offset)
        start = offset + 1
    if len(offsets) != 1:
        raise RecordError("expected exactly one FW2AINFO record, found %d" %
                          len(offsets))
    offset = offsets[0]
    chunk = blob[offset:offset + SIZE]
    if len(chunk) != SIZE:
        raise RecordError("record at 0x%X is truncated" % offset)
    fields = struct.unpack(FULL, chunk)
    if fields[1] != 1 or fields[2] != 0:
        raise RecordError("unsupported record version or kind")
    if fields[3] != 0 or fields[5] != 0:
        raise RecordError("reserved record fields are not zero")
    if fields[4] > 999:
        raise RecordError("app version exceeds 999")
    if fields[-1] != (zlib.crc32(chunk[:-4]) & 0xffffffff):
        raise RecordError("record CRC32 is wrong")
    name = text(fields[6], "name")
    description = text(fields[7], "description")
    text(fields[8], "build")
    if not name or not description:
        raise RecordError("name and description must not be empty")
    return name, fields[4], description


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", required=True)
    args = parser.parse_args()
    try:
        with open(args.bin, "rb") as source:
            name, version, _ = check(source.read())
    except (OSError, RecordError, UnicodeDecodeError) as error:
        print("check_uf2_info: %s" % error, file=sys.stderr)
        return 1
    print("UF2 info: %s %03d" % (name, version))
    return 0


if __name__ == "__main__":
    sys.exit(main())
