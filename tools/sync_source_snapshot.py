#!/usr/bin/env python3
"""Compare/apply text-only LF2Vita source from a user-provided next-chat ZIP.

Dry run by default; --apply requires a clean git worktree. This script never
copies the generated LF2 gamepack, VPK, SDK archives or binary game artwork.
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
import zipfile
from pathlib import Path, PurePosixPath

SAFE_SUFFIXES = {".c", ".h", ".inc", ".py", ".md", ".txt", ".cmake",
                 ".xml", ".yml", ".yaml", ".json", ".sh"}
SAFE_NAMES = {".gitignore", "CMakeLists.txt", "Makefile"}
EXCLUDE_TOP = {"gamepack", "build", "vendor", "vitasdk", "sdk_packages"}


def select_prefix(names: list[str]) -> str:
    hits = sorted({
        name.split("/SOURCE/", 1)[0] + "/SOURCE/" +
        name.split("/SOURCE/", 1)[1].split("/", 1)[0] + "/"
        for name in names if "/SOURCE/" in name
        and name.split("/SOURCE/", 1)[1].startswith("LF2Vita-")
        and "/" in name.split("/SOURCE/", 1)[1]
    })
    if len(hits) != 1:
        raise ValueError(f"Expected one SOURCE/LF2Vita-* directory; found {hits}")
    return hits[0]


def is_allowed(rel: PurePosixPath) -> bool:
    if rel.is_absolute() or ".." in rel.parts or not rel.parts:
        return False
    if rel.parts[0] in EXCLUDE_TOP or rel.parts[0].startswith("build-"):
        return False
    if any(part.startswith(".git/") for part in rel.parts):
        return False
    if rel.suffix.lower() not in SAFE_SUFFIXES and rel.name not in SAFE_NAMES:
        return False
    return True


def clean_repo(root: Path) -> bool:
    if not (root / ".git").exists():
        return False
    p = subprocess.run(["git", "-C", str(root), "status", "--porcelain"],
                       capture_output=True, text=True)
    return p.returncode == 0 and not p.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path, help="LF2Vita next-chat source-workspace ZIP")
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--apply", action="store_true", help="Write changed text files")
    args = parser.parse_args()
    root = args.repo_root.resolve()
    if args.apply and not clean_repo(root):
        parser.error("--apply requires a clean git worktree; commit or stash changes first")
    changed = []
    added = []
    unchanged = 0
    with zipfile.ZipFile(args.snapshot) as package:
        prefix = select_prefix(package.namelist())
        for member in sorted(package.infolist(), key=lambda m: m.filename):
            if member.is_dir() or not member.filename.startswith(prefix):
                continue
            relative = PurePosixPath(member.filename[len(prefix):])
            if not is_allowed(relative):
                continue
            path = root.joinpath(*relative.parts)
            if not path.resolve().is_relative_to(root):
                raise ValueError(f"Path escapes repository: {relative}")
            new_bytes = package.read(member)
            existing = path.read_bytes() if path.is_file() else None
            if existing == new_bytes:
                unchanged += 1
                continue
            status = "MOD" if existing is not None else "ADD"
            sha = hashlib.sha256(new_bytes).hexdigest()[:12]
            print(f"{status} {relative.as_posix()} sha256={sha}")
            (changed if existing is not None else added).append(relative.as_posix())
            if args.apply:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(new_bytes)
    print(f"Unchanged: {unchanged}; modified: {len(changed)}; added: {len(added)}")
    if not args.apply and (changed or added):
        print("Dry run only. Rerun with --apply on a clean checkout to sync text source.")
    if args.apply:
        print("Source applied. Review 'git diff' and rerun tests before committing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
