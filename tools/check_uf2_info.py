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


def text(raw, label, allow_empty=False):
    if b"\0" not in raw:
        raise RecordError("%s is not NUL-terminated" % label)
    value = raw.split(b"\0", 1)[0]
    if not allow_empty and not value:
        raise RecordError("%s must not be empty" % label)
    if any(byte < 0x20 or byte > 0x7e for byte in value):
        raise RecordError("%s must contain printable ASCII only" % label)
    return value.decode("ascii")


def parse_record(blob, offset):
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
    text(fields[8], "build", allow_empty=True)
    return name, fields[4], description


def check(blob):
    offsets = []
    start = 0
    while True:
        offset = blob.find(MAGIC, start)
        if offset < 0:
            break
        offsets.append(offset)
        start = offset + 1
    valid = []
    invalid = []
    for offset in offsets:
        try:
            valid.append((offset, parse_record(blob, offset)))
        except (RecordError, UnicodeDecodeError) as error:
            invalid.append((offset, error))
    # A valid record may contain the magic in a text field. Ignore only those
    # nested occurrences; malformed top-level records still fail closed.
    spans = [(offset, offset + SIZE) for offset, _record in valid]
    for offset, error in invalid:
        if not any(start < offset < stop for start, stop in spans):
            raise RecordError("record at 0x%X is invalid: %s" % (offset, error))
    if len(valid) == 1:
        return valid[0][1]
    if len(valid) > 1:
        raise RecordError("expected exactly one valid FW2AINFO record, found %d" % len(valid))
    raise RecordError("expected exactly one valid FW2AINFO record, found 0")


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
