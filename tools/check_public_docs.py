#!/usr/bin/env python3
"""Reject private upstream references in wilibsp's public Markdown."""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
PUBLIC_ROOTS = ("README.md", "docs", "apps", "libs")
FORBIDDEN = (
    (re.compile(r"freewili-firmware", re.IGNORECASE), "private repository name"),
    (re.compile(r"\bfreewilimain\b", re.IGNORECASE), "private source tree"),
    (re.compile(r"\brmpLib\b", re.IGNORECASE), "private source directory"),
    (re.compile(r"agents[/\\]hardware", re.IGNORECASE), "private documentation path"),
    (re.compile(r"panels[/\\]fw[A-Za-z0-9_]+", re.IGNORECASE), "private panel source path"),
)


def markdown_files(root: Path = REPO_ROOT):
    for entry in PUBLIC_ROOTS:
        path = root / entry
        if path.is_file():
            yield path
        elif path.is_dir():
            yield from sorted(path.rglob("*.md"))


def violations(root: Path = REPO_ROOT):
    for path in markdown_files(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            for pattern, description in FORBIDDEN:
                match = pattern.search(line)
                if match:
                    yield path.relative_to(root), line_number, description, match.group(0)


def main() -> int:
    found = list(violations())
    for path, line, description, match in found:
        print(f"FAIL {path}:{line}: {description}: {match}")
    if found:
        print(f"FAILED {len(found)} private documentation reference(s)")
        return 1
    print("OK: public documentation contains no private upstream references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
