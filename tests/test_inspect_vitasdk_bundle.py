"""Tests for the offline SDK archive inspector (stdlib only)."""
from __future__ import annotations

import importlib.util
import io
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "tools" / "inspect_vitasdk_bundle.py"
SPEC = importlib.util.spec_from_file_location("vitasdk_inspector", MODULE)
assert SPEC and SPEC.loader
inspector = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(inspector)


def make_deb(paths: list[str]) -> bytes:
    tarout = io.BytesIO()
    with tarfile.open(fileobj=tarout, mode="w:gz") as t:
        for path in paths:
            payload = b"stub"  # Presence is checked; never executed.
            item = tarfile.TarInfo(path)
            item.size = len(payload)
            item.mode = 0o755
            t.addfile(item, io.BytesIO(payload))
    data = b"!<arch>\n"
    for name, payload in (
        ("debian-binary", b"2.0\n"),
        ("data.tar.gz", tarout.getvalue()),
    ):
        header = (f"{name + '/':<16}{0:<12}{0:<6}{0:<6}{100644:<8}{len(payload):<10}"
                  + "\x60\n").encode("ascii")
        assert len(header) == 60
        data += header + payload + (b"\n" if len(payload) & 1 else b"")
    return data


class VitaSdkBundleTest(unittest.TestCase):
    def test_nested_deb_and_direct_zip_binary(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sdk.zip"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("packages/compiler.deb", make_deb([
                    "./usr/local/vitasdk/bin/arm-vita-eabi-gcc",
                    "./usr/local/vitasdk/share/vita.toolchain.cmake",
                ]))
                z.writestr("tools/vita-pack-vpk", b"stub")
                z.writestr("tools/vita-make-fself", b"stub")
            result = inspector.inspect_sdk_zip(path)
            self.assertEqual(result["nested_packages_inspected"], 1)
            self.assertTrue(result["crosscompiler_present"])
            self.assertTrue(result["packaging_tools_present"])
            self.assertTrue(result["cmake_toolchain_present"])
            self.assertEqual(result["inspection_errors"], [])

    def test_library_only_bundle_is_not_claimed_buildable(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "library_only.zip"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("lib/mylib.a", b"stub")
            result = inspector.inspect_sdk_zip(path)
            self.assertFalse(result["crosscompiler_present"])
            self.assertFalse(result["packaging_tools_present"])

    def test_bad_deb_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "broken.zip"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("broken.deb", b"not-an-ar")
            result = inspector.inspect_sdk_zip(path)
            self.assertEqual(len(result["inspection_errors"]), 1)
            self.assertFalse(result["crosscompiler_present"])

    def test_malformed_ar_is_rejected(self):
        with self.assertRaises(ValueError):
            list(inspector.iter_ar_members(b"something else"))
        with self.assertRaises(ValueError):
            list(inspector.iter_ar_members(b"!<arch>\n123"))


if __name__ == "__main__":
    unittest.main()
