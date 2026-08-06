import importlib.util
from pathlib import Path

import pytest


TOOLS = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


gen = load("gen_uf2_info")
check = load("check_uf2_info")


def record(**overrides):
    values = dict(version=7, name="demo", description="Demo application")
    values.update(overrides)
    return gen.build_record(**values)


def test_round_trip_and_sparse_offset():
    name, version, description = check.check(b"prefix" + record() + b"suffix")
    assert (name, version, description) == ("demo", 7, "Demo application")


@pytest.mark.parametrize("blob", [b"", b"FW2AINFO", record()[:-1]])
def test_missing_or_truncated_record_is_rejected(blob):
    with pytest.raises(check.RecordError):
        check.check(blob)


def test_duplicate_record_is_rejected():
    with pytest.raises(check.RecordError, match="exactly one"):
        check.check(record() + record(name="second"))


def test_valid_plus_corrupt_structural_record_is_rejected():
    damaged = bytearray(record(name="second"))
    damaged[40] ^= 1
    with pytest.raises(check.RecordError, match="invalid.*CRC32"):
        check.check(record() + damaged)


def test_magic_in_description_is_not_a_second_record():
    result = check.check(record(description="Contains FW2AINFO as text"))
    assert result[0] == "demo"


def test_corrupt_crc_is_rejected():
    damaged = bytearray(record())
    damaged[40] ^= 1
    with pytest.raises(check.RecordError, match="CRC32"):
        check.check(damaged)


def test_nonzero_reserved_field_is_rejected():
    damaged = bytearray(record())
    damaged[11] = 1
    import zlib
    damaged[-4:] = (zlib.crc32(damaged[:-4]) & 0xffffffff).to_bytes(4, "little")
    with pytest.raises(check.RecordError):
        check.check(damaged)


def test_unterminated_string_is_rejected_even_with_valid_crc():
    damaged = bytearray(record())
    damaged[16:48] = b"x" * 32
    import zlib
    damaged[-4:] = (zlib.crc32(damaged[:-4]) & 0xffffffff).to_bytes(4, "little")
    with pytest.raises(check.RecordError, match="NUL-terminated"):
        check.check(damaged)


def test_generator_refuses_truncation_and_non_ascii():
    with pytest.raises(SystemExit, match="too long"):
        record(name="x" * 32)
    with pytest.raises(UnicodeEncodeError):
        record(description="not ASCII: \N{SNOWMAN}")


def test_generator_refuses_invalid_version():
    with pytest.raises(ValueError, match="000-999"):
        record(version=1000)
