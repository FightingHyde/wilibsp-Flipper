import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import fw

def app_uf2(address=0x11000000, block_no=0, num_blocks=1):
    import struct
    data = bytearray(512)
    struct.pack_into("<8I", data, 0, 0x0A324655, 0x9E5D5157, 0,
                     address, 256, block_no, num_blocks, 0xE48BFF59)
    struct.pack_into("<I", data, 508, 0x0AB16F30)
    return bytes(data)

def test_new_app_copies_template(tmp_path):
    root = tmp_path
    (root / "apps" / "template").mkdir(parents=True)
    (root / "apps" / "template" / "CMakeLists.txt").write_text(
        'add_executable(template main.c)\n')
    (root / "apps" / "template" / "main.c").write_text("// template\n")

    dest = fw.new_app("blinky", repo_root=root)

    assert dest == root / "apps" / "blinky"
    assert (dest / "main.c").exists()
    # the app target must be renamed from 'template' to the new name
    assert "add_executable(blinky" in (dest / "CMakeLists.txt").read_text()

def test_new_app_rejects_existing(tmp_path):
    root = tmp_path
    (root / "apps" / "blinky").mkdir(parents=True)
    try:
        fw.new_app("blinky", repo_root=root)
        assert False, "expected FileExistsError"
    except FileExistsError:
        pass

def test_install_app_copies_to_apps_ejects_and_returns_sd(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    volume = tmp_path / "card"
    volume.mkdir()
    calls = []
    monkeypatch.setattr(fw, "_fwfinder_main_port", lambda serial: "COM7")
    monkeypatch.setattr(fw, "_mounted_volumes", lambda: {tmp_path / "old"})
    monkeypatch.setattr(fw, "_wait_for_sd", lambda baseline, timeout: volume)
    monkeypatch.setattr(fw, "_set_sd_host", lambda port, pc: calls.append((port, pc)))
    monkeypatch.setattr(fw, "_eject_volume", lambda path: calls.append(("eject", path)))

    fw.install_app(source, "FW123")

    assert (volume / "apps" / "mesh.uf2").read_bytes() == app_uf2()
    assert calls == [("COM7", True), ("eject", volume), ("COM7", False)]

def test_install_app_copies_to_nested_apps_folder(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    volume = tmp_path / "card"
    volume.mkdir()
    monkeypatch.setattr(fw, "_fwfinder_main_port", lambda serial: "COM7")
    monkeypatch.setattr(fw, "_mounted_volumes", set)
    monkeypatch.setattr(fw, "_wait_for_sd", lambda baseline, timeout: volume)
    monkeypatch.setattr(fw, "_set_sd_host", lambda port, pc: None)
    monkeypatch.setattr(fw, "_eject_volume", lambda path: None)

    fw.install_app(source, folder="beta/radio")

    assert (volume / "apps" / "beta" / "radio" / "mesh.uf2").read_bytes() == app_uf2()

def test_install_app_rejects_escaping_or_ambiguous_subfolders_before_hardware(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    monkeypatch.setattr(fw, "_fwfinder_main_port",
                        lambda serial: (_ for _ in ()).throw(AssertionError("hardware touched")))
    for folder in ("../outside", "/outside", "team//app", "team\\app"):
        try:
            fw.install_app(source, folder=folder)
            assert False, "expected ValueError"
        except ValueError:
            pass

def test_install_app_returns_sd_when_no_volume_ever_mounted(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    calls = []
    monkeypatch.setattr(fw, "_fwfinder_main_port", lambda serial: "COM7")
    monkeypatch.setattr(fw, "_mounted_volumes", set)
    monkeypatch.setattr(fw, "_wait_for_sd", lambda baseline, timeout: (_ for _ in ()).throw(RuntimeError("no card")))
    monkeypatch.setattr(fw, "_set_sd_host", lambda port, pc: calls.append(pc))

    try:
        fw.install_app(source)
        assert False, "expected RuntimeError"
    except RuntimeError as exc:
        assert "no card" in str(exc)
    assert calls == [True, False]

def test_install_app_ejects_then_returns_sd_after_copy_failure(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    volume = tmp_path / "card"
    volume.mkdir()
    calls = []
    monkeypatch.setattr(fw, "_fwfinder_main_port", lambda serial: "COM7")
    monkeypatch.setattr(fw, "_mounted_volumes", set)
    monkeypatch.setattr(fw, "_wait_for_sd", lambda baseline, timeout: volume)
    monkeypatch.setattr(fw, "_set_sd_host", lambda port, pc: calls.append(("host", pc)))
    monkeypatch.setattr(fw.shutil, "copyfile", lambda *args: (_ for _ in ()).throw(OSError("copy failed")))
    monkeypatch.setattr(fw, "_eject_volume", lambda path: calls.append(("eject", path)))

    try:
        fw.install_app(source)
        assert False, "expected OSError"
    except OSError as exc:
        assert "copy failed" in str(exc)
    assert calls == [("host", True), ("eject", volume), ("host", False)]

def test_install_app_never_returns_sd_when_eject_fails(tmp_path, monkeypatch):
    source = tmp_path / "mesh.uf2"
    source.write_bytes(app_uf2())
    volume = tmp_path / "card"
    volume.mkdir()
    calls = []
    monkeypatch.setattr(fw, "_fwfinder_main_port", lambda serial: "COM7")
    monkeypatch.setattr(fw, "_mounted_volumes", set)
    monkeypatch.setattr(fw, "_wait_for_sd", lambda baseline, timeout: volume)
    monkeypatch.setattr(fw, "_set_sd_host", lambda port, pc: calls.append(("host", pc)))
    monkeypatch.setattr(fw, "_eject_volume", lambda path: (_ for _ in ()).throw(RuntimeError("busy")))

    try:
        fw.install_app(source)
        assert False, "expected RuntimeError"
    except RuntimeError as exc:
        assert "remains assigned to the PC" in str(exc)
    assert calls == [("host", True)]

def test_install_app_rejects_flash_before_touching_hardware(tmp_path, monkeypatch):
    source = tmp_path / "bad.uf2"
    source.write_bytes(app_uf2(0x10000000))
    monkeypatch.setattr(fw, "_fwfinder_main_port",
                        lambda serial: (_ for _ in ()).throw(AssertionError("hardware touched")))
    try:
        fw.install_app(source)
        assert False, "expected ValueError"
    except ValueError as exc:
        assert "QSPI flash" in str(exc)

def test_check_app_uf2_rejects_incomplete_declared_block_set(tmp_path):
    source = tmp_path / "truncated.uf2"
    source.write_bytes(app_uf2(num_blocks=2))
    try:
        fw.check_app_uf2(source)
        assert False, "expected ValueError"
    except ValueError as exc:
        assert "incomplete" in str(exc)

def test_check_app_uf2_rejects_duplicate_block_number(tmp_path):
    source = tmp_path / "duplicate.uf2"
    source.write_bytes(app_uf2(block_no=0, num_blocks=2) * 2)
    try:
        fw.check_app_uf2(source)
        assert False, "expected ValueError"
    except ValueError as exc:
        assert "duplicates" in str(exc)

def test_check_app_uf2_rejects_inconsistent_block_totals(tmp_path):
    source = tmp_path / "inconsistent.uf2"
    source.write_bytes(app_uf2(block_no=0, num_blocks=2) +
                       app_uf2(address=0x11000100, block_no=1, num_blocks=3))
    try:
        fw.check_app_uf2(source)
        assert False, "expected ValueError"
    except ValueError as exc:
        assert "inconsistent" in str(exc)

def test_build_command_uses_target_preset():
    cmd = fw.build_command("hello_display")
    assert cmd[:3] == ["cmake", "--build", "--preset"]
    assert "target" in cmd

def test_test_command_configures_and_runs_ctest():
    cmds = fw.test_command()
    # Standalone tests/ tree: configured directly into build-tests/, no
    # Pico-SDK-coupled preset involved (fw test works without the SDK).
    configure = cmds[0]
    assert configure[:2] == ["cmake", "-S"]
    assert str(fw.REPO_ROOT / "tests") in configure
    assert "-B" in configure
    assert str(fw.REPO_ROOT / "build-tests") in configure[configure.index("-B") + 1]

    build = cmds[1]
    assert build[:2] == ["cmake", "--build"]
    assert str(fw.REPO_ROOT / "build-tests") in build

    ctest = cmds[-1]
    assert ctest[0] == "ctest"
    assert "--test-dir" in ctest
    assert str(fw.REPO_ROOT / "build-tests") in ctest[ctest.index("--test-dir") + 1]

def test_flash_command_programs_correct_elf():
    cmd = fw.flash_command("hello_display")
    # cmd[0] is a discovered openocd exe (pico-sdk install path or bare "openocd")
    assert "openocd" in cmd[0].lower()
    assert "-f" in cmd and fw.OPENOCD_CFG in cmd
    program_arg = cmd[-1]
    assert "build/apps/hello_display/hello_display.elf" in program_arg
    assert "verify" in program_arg
    assert "reset" in program_arg

def test_rtt_command_sets_up_and_serves_rtt():
    cmd = fw.rtt_command()
    assert "openocd" in cmd[0].lower()
    assert "-f" in cmd and fw.OPENOCD_CFG in cmd
    assert any("rtt setup" in c for c in cmd)
    assert any(f"rtt server start {fw.RTT_PORT}" in c for c in cmd)

def test_main_print_dispatches_build_command(capsys):
    fw.main(["build", "--print"])
    captured = capsys.readouterr()
    assert "cmake --build --preset target --target hello_display" in captured.out

def test_configure_command_pins_sdk_and_uses_preset():
    cmd = fw.configure_command()
    assert cmd[:3] == ["cmake", "--preset", "target"]
    sdk = [c for c in cmd if c.startswith("-DPICO_SDK_PATH=")]
    # Only asserted when the pinned SDK is actually installed on this machine.
    if fw._pico_sdk_dir("sdk", fw.PICO_SDK_VERSION) is not None:
        assert sdk and sdk[0].endswith(fw.PICO_SDK_VERSION)

def test_configure_command_never_overrides_pico_board():
    # AGENTS.md invariant 1/8: -DPICO_BOARD on the command line overrides the
    # cached value from the top-level CMakeLists and reverts the board config.
    assert not any("PICO_BOARD" in c for c in fw.configure_command())

def test_pico_sdk_dir_falls_back_to_newest_installed(tmp_path, monkeypatch):
    root = tmp_path / ".pico-sdk" / "sdk"
    (root / "2.1.0").mkdir(parents=True)
    (root / "2.4.0").mkdir(parents=True)
    monkeypatch.setattr(fw.pathlib.Path, "home", staticmethod(lambda: tmp_path))
    assert fw._pico_sdk_dir("sdk", "2.1.0").name == "2.1.0"   # pinned wins when present
    assert fw._pico_sdk_dir("sdk", "9.9.9").name == "2.4.0"   # else newest installed

def test_needs_configure_detects_missing_and_stale_trees(tmp_path, monkeypatch):
    monkeypatch.setattr(fw, "BUILD_DIR", tmp_path / "build")
    assert fw.needs_configure() is True                       # no build tree at all

    sdk = fw._pico_sdk_dir("sdk", fw.PICO_SDK_VERSION)
    if sdk is None:
        return                                                # no SDK installed here
    (tmp_path / "build").mkdir()
    cache = tmp_path / "build" / "CMakeCache.txt"
    cache.write_text(f"PICO_SDK_PATH:PATH={sdk}\n")
    # a cache without a generator file means the configure died part-way
    assert fw.needs_configure() is True
    (tmp_path / "build" / "build.ninja").write_text("")
    assert fw.needs_configure() is False                      # already on the pinned SDK
    cache.write_text("PICO_SDK_PATH:PATH=/somewhere/sdk/1.0.0\n")
    assert fw.needs_configure() is True                       # configured against another SDK

def test_packbits_decode_handles_literal_and_repeat_runs():
    # literal run of two units, then a repeat run of three
    data = bytes([1, 0x12, 0x34, 0xAB, 0xCD, 0xFE, 0x07, 0xE0])
    assert fw.packbits_decode(data, 5) == [0x1234, 0xABCD, 0x07E0, 0x07E0, 0x07E0]

def test_packbits_decode_rejects_truncated_input():
    try:
        fw.packbits_decode(bytes([1, 0x12]), 2)
        assert False, "expected ValueError"
    except ValueError:
        pass

def test_png_write_produces_a_valid_signature_and_size(tmp_path):
    out = tmp_path / "x.png"
    # 2x1 image: one red pixel, one blue
    fw.png_write(str(out), 2, 1, [0xF800, 0x001F])
    blob = out.read_bytes()
    assert blob[:8] == b"\x89PNG\r\n\x1a\n"
    assert b"IHDR" in blob and b"IDAT" in blob and b"IEND" in blob
    # IHDR width/height are big-endian at a fixed offset
    import struct
    w, h = struct.unpack(">II", blob[16:24])
    assert (w, h) == (2, 1)

def test_rtt_command_serves_both_channels():
    cmd = fw.rtt_command()
    assert any(f"rtt server start {fw.RTT_PORT} 0" in c for c in cmd)
    assert any(f"rtt server start {fw.AGENTIO_PORT} 1" in c for c in cmd)

def test_main_print_dispatches_screenshot():
    # --print must not touch hardware
    fw.main(["screenshot", "--print"])

def test_agentio_enter_cleans_up_leaked_process_on_timeout(monkeypatch):
    # Regression: if OpenOCD never opens the RTT port within the deadline,
    # __enter__ must still terminate the process it spawned. Python skips
    # __exit__ when __enter__ raises, so without explicit cleanup inside
    # __enter__ this would leak an OpenOCD process holding the debug probe.
    monkeypatch.setattr(fw, "_port_open", lambda port: False)

    class FakeProc:
        def __init__(self):
            self.terminated = False
            self.killed = False
        def poll(self):
            return None  # still "running" the whole time
        def terminate(self):
            self.terminated = True
        def wait(self, timeout=None):
            return 0
        def kill(self):
            self.killed = True

    fake = FakeProc()
    monkeypatch.setattr(fw.subprocess, "Popen", lambda *a, **k: fake)

    # A fake clock that advances past the 10s deadline in a few calls, and a
    # no-op sleep, so the test does not actually wait 10 seconds.
    class FakeClock:
        def __init__(self):
            self.t = 0
        def time(self):
            self.t += 3
            return self.t

    monkeypatch.setattr(fw.time, "time", FakeClock().time)
    monkeypatch.setattr(fw.time, "sleep", lambda s: None)

    try:
        with fw._Agentio():
            assert False, "expected RuntimeError"
    except RuntimeError:
        pass

    assert fake.terminated is True

def test_agentio_capture_surfaces_err_reply_instead_of_hanging(monkeypatch):
    # Regression: every CAP error reply ("ERR rect\n" = 9 bytes, "ERR
    # surface\n" = 12 bytes) is shorter than AGENTIO_HEADER_LEN (18). The old
    # agentio_capture() did an unconditional recv_exact(AGENTIO_HEADER_LEN)
    # first, which can never return for these replies — the CLI would block
    # until the raw socket timeout instead of raising the server's message.
    monkeypatch.setattr(fw, "_port_open", lambda port: True)  # skip spawning OpenOCD

    class FakeSocket:
        """Just enough of the socket API for _Agentio: recv() hands back
        bytes from a fixed buffer, one chunk at a time, like a real socket."""
        def __init__(self, data):
            self.data = data
            self.pos = 0
        def settimeout(self, t):
            pass
        def sendall(self, b):
            pass
        def recv(self, n):
            chunk = self.data[self.pos:self.pos + n]
            self.pos += len(chunk)
            return chunk
        def close(self):
            pass

    fake = FakeSocket(b"ERR rect\n")
    monkeypatch.setattr(fw.socket, "create_connection", lambda *a, **k: fake)

    try:
        fw.agentio_capture("lcd", (0, 0, 0, 0), 1, "unused.png")
        assert False, "expected RuntimeError"
    except RuntimeError as e:
        assert "rect" in str(e)
