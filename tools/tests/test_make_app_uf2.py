import importlib.util
from pathlib import Path
import struct
import sys

import pytest


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


make = load("make_app_uf2")
check_app = load("check_app_uf2")
gen_info = load("gen_uf2_info")


def elf32(segments):
    phoff = 52
    phentsize = 32
    header = bytearray(52 + phentsize * len(segments))
    header[:6] = b"\x7fELF\x01\x01"
    struct.pack_into("<I", header, 28, phoff)
    struct.pack_into("<HH", header, 42, phentsize, len(segments))
    payload = bytearray(header)
    for index, (address, data, memsz) in enumerate(segments):
        offset = len(payload) if data else 0
        payload.extend(data)
        struct.pack_into("<8I", payload, phoff + index * phentsize,
                         1, offset, address, address, len(data), memsz, 5, 4)
    return bytes(payload)


def test_emits_only_file_backed_load_segments():
    source = bytes(range(256)) + b"tail"
    blob = elf32([(0x20000000, source, len(source)),
                  (0x11000000, b"", 614400)])
    uf2 = make.make_uf2(blob)
    assert len(uf2) == 2 * 512
    first = struct.unpack_from("<8I", uf2, 0)
    second = struct.unpack_from("<8I", uf2, 512)
    assert first[3:7] == (0x20000000, 256, 0, 2)
    assert second[3:7] == (0x20000100, 4, 1, 2)
    assert uf2[32:32 + 256] == source[:256]
    assert uf2[512 + 32:512 + 36] == b"tail"


def test_rejects_non_elf_and_empty_load_set():
    with pytest.raises(ValueError, match="ELF"):
        make.make_uf2(b"not an elf")
    with pytest.raises(ValueError, match="no loadable"):
        make.make_uf2(elf32([(0x11000000, b"", 16)]))


def test_final_uf2_metadata_can_span_blocks(tmp_path):
    info = gen_info.build_record(version=12, name="demo", description="Block-spanning metadata")
    source = b"x" * 200 + info
    uf2 = make.make_uf2(elf32([(0x20000000, source, len(source))]))
    path = tmp_path / "demo.uf2"
    path.write_bytes(uf2)
    assert check_app.load_image(path) == source
    assert check_app.check_uf2_info(check_app.load_image(path)) == (
        "demo", 12, "Block-spanning metadata")
