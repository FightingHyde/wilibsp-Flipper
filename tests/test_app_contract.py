from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
APPS = ROOT / "apps"


def app_dirs():
    return sorted(path.parent for path in APPS.glob("*/CMakeLists.txt")
                  if "add_executable(" in path.read_text(encoding="utf-8"))


def test_every_app_uses_contract_wrapper():
    missing = []
    for app in app_dirs():
        cmake = (app / "CMakeLists.txt").read_text(encoding="utf-8")
        if "fw2_display_app(" not in cmake:
            missing.append(app.name)
    assert not missing, "apps missing fw2_display_app(): " + ", ".join(missing)


def test_every_app_initializes_and_services_recovery():
    missing = []
    for app in app_dirs():
        main = app / "main.c"
        if not main.exists():
            continue
        source = main.read_text(encoding="utf-8")
        required = ("fw2_app_recovery_init()", "fw2_app_recovery_task()")
        absent = [call for call in required if call not in source]
        if absent:
            missing.append("%s: %s" % (app.name, ", ".join(absent)))
    assert not missing, "apps missing recovery calls: " + "; ".join(missing)


def _matching_brace(source, opening):
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return offset
    raise AssertionError("unclosed loop body")


def test_every_terminal_loop_services_recovery():
    """A token elsewhere in main.c must not make an unserviceable loop pass."""
    missing = []
    terminal = re.compile(r"(?:while\s*\(\s*(?:true|1)\s*\)|for\s*\(\s*;\s*;\s*\))")
    for app in app_dirs():
        main = app / "main.c"
        if not main.exists():
            continue
        source = main.read_text(encoding="utf-8")
        for match in terminal.finditer(source):
            opening = source.find("{", match.end())
            semicolon = source.find(";", match.end())
            if opening < 0 or (semicolon >= 0 and semicolon < opening):
                missing.append(f"{app.name}:{source.count(chr(10), 0, match.start()) + 1}")
                continue
            body = source[opening + 1:_matching_brace(source, opening)]
            if "fw2_app_recovery_task()" not in body:
                missing.append(f"{app.name}:{source.count(chr(10), 0, match.start()) + 1}")
    assert not missing, "terminal loops missing recovery service: " + ", ".join(missing)


def test_apps_do_not_use_unserviceable_sleep():
    offenders = []
    direct_sleep = re.compile(r"(?<!fw2_app_recovery_)sleep_ms\s*\(")
    for app in app_dirs():
        main = app / "main.c"
        if main.exists() and direct_sleep.search(main.read_text(encoding="utf-8")):
            offenders.append(app.name)
    assert not offenders, (
        "apps must use fw2_app_recovery_sleep_ms(): " + ", ".join(offenders)
    )


def test_apps_do_not_use_blocking_pdm_capture():
    offenders = []
    for app in app_dirs():
        main = app / "main.c"
        if main.exists() and "pdm_capture_block(" in main.read_text(encoding="utf-8"):
            offenders.append(app.name)
    assert not offenders, "apps must service recovery while capturing PDM: " + ", ".join(offenders)
