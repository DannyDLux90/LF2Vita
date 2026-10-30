#!/usr/bin/env python3
"""Inspect an offline VitaSDK ZIP without installing or executing bundled files.

This checks *presence*, not whether the host ABI or dependencies are usable.
Run with: python3 tools/inspect_vitasdk_bundle.py 'Vita SDK(3).zip'
"""
from __future__ import annotations

import argparse
import io
import json
import tarfile
import zipfile
from pathlib import Path, PurePosixPath

TOOLS = ("arm-vita-eabi-gcc", "vita-make-fself", "vita-pack-vpk")
TOOLCHAIN_FILE = "vita.toolchain.cmake"
MAX_NESTED_PACKAGE_BYTES = 256 * 1024 * 1024


def inspect_path(name: str, found: dict[str, list[str]], origin: str) -> None:
    filename = PurePosixPath(name).name
    if filename in (*TOOLS, TOOLCHAIN_FILE):
        found[filename].append(origin + ":" + name)


def scan_tar(blob: bytes, found: dict[str, list[str]], origin: str) -> None:
    with tarfile.open(fileobj=io.BytesIO(blob), mode="r:*") as archive:
        for member in archive:
            if member.isfile():
                inspect_path(member.name, found, origin)


def iter_ar_members(blob: bytes):
    """Read Debian package ar members; no subprocess or filesystem writes."""
    if not blob.startswith(b"!<arch>\n"):
        raise ValueError("Not a Debian ar archive")
    offset = 8
    while offset < len(blob):
        if offset + 60 > len(blob):
            raise ValueError("Truncated ar header")
        header = blob[offset:offset + 60]
        if header[58:60] != b"\x60\n":
            raise ValueError("Invalid ar header")
        try:
            size = int(header[48:58].strip())
        except ValueError as exc:
            raise ValueError("Invalid ar member size") from exc
        offset += 60
        end = offset + size
        if end > len(blob):
            raise ValueError("Truncated ar payload")
        name = header[:16].decode("ascii", errors="replace").strip().rstrip("/")
        yield name, blob[offset:end]
        offset = end + (size % 2)


def scan_deb(blob: bytes, found: dict[str, list[str]], origin: str) -> None:
    for member_name, payload in iter_ar_members(blob):
        if member_name.startswith("data.tar"):
            scan_tar(payload, found, origin + ":" + member_name)


def inspect_sdk_zip(path: Path) -> dict:
    found = {name: [] for name in (*TOOLS, TOOLCHAIN_FILE)}
    packages = 0
    failures = []
    with zipfile.ZipFile(path) as archive:
        for member in archive.infolist():
            if member.is_dir():
                continue
            inspect_path(member.filename, found, "zip")
            name = member.filename.lower()
            if not (name.endswith(".deb") or
                    name.endswith((".tar", ".tar.gz", ".tar.xz", ".tgz"))):
                continue
            packages += 1
            if member.file_size > MAX_NESTED_PACKAGE_BYTES:
                failures.append(member.filename + ": nested package too large to inspect")
                continue
            try:
                contents = archive.read(member)
                if name.endswith(".deb"):
                    scan_deb(contents, found, member.filename)
                else:
                    scan_tar(contents, found, member.filename)
            except (OSError, EOFError, ValueError, tarfile.TarError) as exc:
                failures.append(member.filename + ": " + str(exc))
    return {
        "sdk_bundle": path.name,
        "nested_packages_inspected": packages,
        "found": found,
        "crosscompiler_present": bool(found["arm-vita-eabi-gcc"]),
        "packaging_tools_present": all(found[name] for name in TOOLS[1:]),
        "cmake_toolchain_present": bool(found[TOOLCHAIN_FILE]),
        "inspection_errors": failures,
        "caveat": "Archive inspection cannot prove binary execution or Vita build success.",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip", type=Path, help="offline VitaSDK bundle")
    args = parser.parse_args()
    try:
        report = inspect_sdk_zip(args.zip)
    except (OSError, zipfile.BadZipFile) as exc:
        parser.exit(2, f"Cannot inspect SDK ZIP: {exc}\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if not report["crosscompiler_present"]:
        parser.exit(1, "No usable compiler candidate was located in the inspected contents.\n")


if __name__ == "__main__":
    main()
