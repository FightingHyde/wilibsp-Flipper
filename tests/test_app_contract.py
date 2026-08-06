from pathlib import Path


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
