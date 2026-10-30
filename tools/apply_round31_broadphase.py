#!/usr/bin/env python3
"""Prepare a conservative Stage broad-phase candidate in an existing LF2 Vita source tree.

This script requires the *full actual* Round-27-or-later workspace supplied by
the project owner. It NEVER substitutes older GitHub main for a newer game
tree, never touches assets and never builds a VPK.

Default: dry run (read-only). --apply installs an OFF-by-default adapter.
--enable requires --apply and is for controlled native/Vita parity experiments,
NOT for release builds without further verification.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
INC = "fix3/lf2_stage_pair_runtime.inc"
MARKER = '#include "fix3/lf2_stage_pair_runtime.inc"'
REFERENCE = "apply_hits(world,fighters,total,a,b,difficulty);"
GUARDED = "if(LF2_STAGE_PAIR_RUNTIME_ALLOWED(&fighters[a],&fighters[b]))" + REFERENCE
TOP_LEVEL_ANCHOR = '#undef apply_hits\n'


def prepare(root: Path, enable: bool = False) -> dict[str, bytes]:
    root = root.resolve()
    gamec = root / "src/game.c"
    stage = root / "src/fix3/game_24.inc"
    if not gamec.is_file() or not stage.is_file():
        raise ValueError("Missing src/game.c or src/fix3/game_24.inc; supply full current source tree")
    # This is a structural check, not an assertion that a given old/new version
    # is fully compatible with any future LF2 Vita game split.
    c = gamec.read_text(encoding="utf-8")
    g = stage.read_text(encoding="utf-8")
    if TOP_LEVEL_ANCHOR not in c or 'game_15.inc' not in c:
        raise ValueError("Top-level after-game_15 boundary missing; abort")
    if "LF2_STAGE_PAIR_RUNTIME_ALLOWED" not in g:
        if g.count(REFERENCE) != 1:
            raise ValueError("Expected exactly one Stage apply_hits call; abort")
        g = g.replace(REFERENCE, GUARDED)
    elif g.count(GUARDED) != 1:
        raise ValueError("Existing Stage guard differs from expected version; abort")

    lines = '#define LF2_STAGE_BROADPHASE_ENABLED 1\n' if enable else ""
    if MARKER not in c:
        c = c.replace(TOP_LEVEL_ANCHOR, TOP_LEVEL_ANCHOR + lines + MARKER + "\n", 1)
    elif enable and "#define LF2_STAGE_BROADPHASE_ENABLED 1" not in c:
        c = c.replace(MARKER, lines + MARKER, 1)
    elif not enable and "#define LF2_STAGE_BROADPHASE_ENABLED 1" in c:
        # Never silently disable an existing explicitly enabled experiment.
        raise ValueError("Already enabled: must explicitly pass --enable or revert manually")

    pending = {
        gamec: c.encode("utf-8"),
        stage: g.encode("utf-8"),
        root / "src/fix3/lf2_stage_pair_runtime.inc":
            (HERE / "lf2_stage_pair_runtime.inc").read_bytes(),
        root / "src/fix3/lf2_stage_pair_bounds.h":
            (HERE / "lf2_stage_pair_bounds.h").read_bytes(),
    }
    for path, content in pending.items():
        if path in (gamec, stage):
            continue
        if path.exists() and path.read_bytes() != content:
            raise ValueError(f"Conflicting existing file, refusing to overwrite: {path}")
    return {str(p): content for p, content in pending.items()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("workspace", type=Path, help="full Round-27-or-later LF2 Vita source folder")
    parser.add_argument("--apply", action="store_true", help="write guarded files (default: dry run)")
    parser.add_argument("--enable", action="store_true", help="enable experimental filter, requires --apply")
    args = parser.parse_args()
    if args.enable and not args.apply:
        parser.error("--enable requires --apply")
    try:
        updates = prepare(args.workspace, enable=args.enable)
    except (OSError, UnicodeError, ValueError) as exc:
        parser.exit(2, f"Source integration refused: {exc}\n")
    changed = [(Path(path), payload) for path, payload in updates.items()
               if not Path(path).is_file() or Path(path).read_bytes() != payload]
    for path, payload in changed:
        digest = hashlib.sha256(payload).hexdigest()[:16]
        print(f"{'WRITE' if args.apply else 'WOULD WRITE'} {path} bytes={len(payload)} sha256-prefix={digest}")
    print(f"Broad-phase compile-time state: {'ENABLED (experimental)' if args.enable else 'DISABLED (safe default)'}")
    if not changed:
        print("No changes needed; already installed.")
    if args.apply:
        for path, payload in changed:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)


if __name__ == "__main__":
    main()
