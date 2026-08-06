from pathlib import Path
import importlib.util
import pytest


TOOLS = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_app_repo", TOOLS / "check_app_repo.py")
check_app_repo = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_app_repo)


def valid_repo(tmp_path):
    (tmp_path / "wilibsp").mkdir()
    (tmp_path / "wilibsp" / "AGENTS.md").write_text("contract\n", encoding="utf-8")
    (tmp_path / "AGENTS.md").write_text(
        "Read wilibsp/AGENTS.md. If truncated, continue in chunks through EOF.\n",
        encoding="utf-8")
    return tmp_path


def test_accepts_required_root_pointer(tmp_path):
    assert check_app_repo.check(valid_repo(tmp_path)) == tmp_path.resolve()


@pytest.mark.parametrize("text", [
    "No BSP pointer. Read in chunks through EOF.\n",
    "Read wilibsp/AGENTS.md.\n",
])
def test_rejects_missing_pointer_or_complete_read_instruction(tmp_path, text):
    valid_repo(tmp_path)
    (tmp_path / "AGENTS.md").write_text(text, encoding="utf-8")
    with pytest.raises(check_app_repo.SetupError):
        check_app_repo.check(tmp_path)


def test_rejects_claude_file_that_bypasses_agents(tmp_path):
    valid_repo(tmp_path)
    (tmp_path / "CLAUDE.md").write_text("Read README.md\n", encoding="utf-8")
    with pytest.raises(check_app_repo.SetupError, match="CLAUDE"):
        check_app_repo.check(tmp_path)
