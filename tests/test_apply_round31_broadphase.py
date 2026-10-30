"""Source-level Stage broad-phase adapter installation safety tests.

This copies the historical GitHub engine snippets as fixtures. The *user's*
current Round-27 source must separately pass these checks and a Vita build.
"""
from __future__ import annotations
import importlib.util
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "stage_patcher", REPO / "tools" / "apply_round31_broadphase.py")
assert SPEC and SPEC.loader
patcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(patcher)


class StagePatcherTest(unittest.TestCase):
    def make_workspace(self, tmp):
        root = Path(tmp)
        (root / "src/fix3").mkdir(parents=True, exist_ok=True)
        for relative in ("src/game.c", "src/fix3/game_24.inc"):
            (root / relative).write_bytes((REPO / relative).read_bytes())
        # GitHub main is older than the local Round-27 split; its game.c
        # deliberately lacks the top-level stock wrapper boundary.
        # This fixture emulates the later #undef established in Round 6+.
        gamec = root / "src/game.c"
        source = gamec.read_text()
        source = source.replace(
            '#include "fix3/game_15.inc"\n',
            '#include "fix3/game_15.inc"\n#undef apply_hits\n', 1)
        gamec.write_text(source)
        return root

    def test_dry_run_does_not_touch_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self.make_workspace(tmp)
            old = (root / "src/game.c").read_bytes()
            stage = (root / "src/fix3/game_24.inc").read_bytes()
            changes = patcher.prepare(root)
            self.assertEqual(len(changes), 4)
            self.assertIn(patcher.MARKER, changes[str(root / "src/game.c")].decode())
            self.assertIn(patcher.GUARDED,
                          changes[str(root / "src/fix3/game_24.inc")].decode())
            self.assertNotIn("#define LF2_STAGE_BROADPHASE_ENABLED 1",
                             changes[str(root / "src/game.c")].decode())
            self.assertEqual((root / "src/game.c").read_bytes(), old)
            self.assertEqual((root / "src/fix3/game_24.inc").read_bytes(), stage)

    def test_explicit_enable_preserves_existing_stage_loop(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self.make_workspace(tmp)
            original = (root / "src/fix3/game_24.inc").read_text()
            result = patcher.prepare(root, enable=True)
            stage = result[str(root / "src/fix3/game_24.inc")].decode()
            self.assertEqual(stage.replace(patcher.GUARDED,patcher.REFERENCE),
                             original)
            self.assertIn("#define LF2_STAGE_BROADPHASE_ENABLED 1",
                          result[str(root / "src/game.c")].decode())
            for path, data in result.items():
                Path(path).write_bytes(data)
            self.assertEqual(result, patcher.prepare(root, enable=True))

    def test_abort_missing_source_or_foreign_guard(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                patcher.prepare(Path(tmp))
            root = self.make_workspace(tmp)
            stage = root / "src/fix3/game_24.inc"
            stage.write_text(stage.read_text().replace(
                patcher.REFERENCE, "if(alternative_check())"+patcher.REFERENCE))
            with self.assertRaises(ValueError):
                patcher.prepare(root)

    def test_existing_conflicting_header_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self.make_workspace(tmp)
            (root / "src/fix3/lf2_stage_pair_runtime.inc").write_text("conflict")
            with self.assertRaises(ValueError):
                patcher.prepare(root)


if __name__ == "__main__":
    unittest.main()
