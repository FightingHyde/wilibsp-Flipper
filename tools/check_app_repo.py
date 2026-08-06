"""Validate that an external app repo exposes the wilibsp contract to agents."""
import argparse
from pathlib import Path


class SetupError(ValueError):
    pass


def check(root):
    root = Path(root).resolve()
    agents = root / "AGENTS.md"
    bsp_agents = root / "wilibsp" / "AGENTS.md"
    if not bsp_agents.is_file():
        raise SetupError("wilibsp/AGENTS.md is missing; pin wilibsp at repo root")
    if not agents.is_file():
        raise SetupError("root AGENTS.md is missing")

    text = agents.read_text(encoding="utf-8")
    opening = "\n".join(text.splitlines()[:20]).lower().replace("\\", "/")
    if "wilibsp/agents.md" not in opening:
        raise SetupError("AGENTS.md must point to wilibsp/AGENTS.md in its first 20 lines")
    if "eof" not in opening or not any(word in opening for word in ("chunk", "truncat")):
        raise SetupError("AGENTS.md must require reading truncated files in chunks through EOF")

    claude = root / "CLAUDE.md"
    if claude.is_file() and "agents.md" not in claude.read_text(encoding="utf-8").lower():
        raise SetupError("CLAUDE.md must point to root AGENTS.md")
    return root


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".")
    args = parser.parse_args(argv)
    try:
        checked = check(args.root)
    except SetupError as exc:
        parser.exit(1, "app repo setup: %s\n" % exc)
    print("app repo setup: OK", checked)


if __name__ == "__main__":
    main()
