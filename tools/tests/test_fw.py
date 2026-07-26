import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import fw

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
    assert fw.needs_configure() is False                      # already on the pinned SDK
    cache.write_text("PICO_SDK_PATH:PATH=/somewhere/sdk/1.0.0\n")
    assert fw.needs_configure() is True                       # configured against another SDK
