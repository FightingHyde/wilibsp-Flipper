import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import check_public_docs


def test_clean_public_docs_pass(tmp_path):
    (tmp_path / "README.md").write_text("# Public API\n", encoding="utf-8")
    assert list(check_public_docs.violations(tmp_path)) == []


def test_private_source_reference_fails(tmp_path):
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "bad.md").write_text(
        "Copied from freewili-firmware/freewilimain/rmpLib.\n",
        encoding="utf-8",
    )
    found = list(check_public_docs.violations(tmp_path))
    assert found
    assert found[0][0] == pathlib.Path("docs/bad.md")


def test_non_markdown_and_agent_guidance_are_not_scanned(tmp_path):
    (tmp_path / "AGENTS.md").write_text("freewili-firmware\n", encoding="utf-8")
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "notes.txt").write_text("freewilimain\n", encoding="utf-8")
    assert list(check_public_docs.violations(tmp_path)) == []
