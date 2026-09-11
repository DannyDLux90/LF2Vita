#!/usr/bin/env python3
"""Decode original Little Fighter 2 .lfr recordings.

The stock lfr_summary_generator.exe decrypts only the first 1345 bytes of the
compressed payload.  For each encrypted byte e and ASCII digit k it computes
    plain = (e - k + ord('0')) & 0xff
and then feeds the payload to zlib.

This tool intentionally does not contain the LF2 key.  Supply the stock
lfr_summary_generator.exe and the key is extracted from its .data image at the
known file offset used by the 2008 utility.

It can also print the 20-byte input records found in the stock recordings.
Each record is one original LF2 network exchange at 15 Hz and is held for two
30 Hz game time units.  The four local key bytes are at record offsets +14..+17;
the bundled demos use +14, while +15..+17 are normally zero.  Key bits match
LF2 network key bytes except recordings do not carry the network baseline bit
0x01.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

KEY_FILE_OFFSET = 0xB750
KEY_LEN = 1345
EXPECTED_KEY_SHA256 = "a1e3e58e52cb7091bff6e1ea7ed8e14274b882f51134af69ca4ce0a72cc8613b"
DECODED_SIZE = 6_491_672
INPUT_START = 0x2B98
INPUT_STRIDE = 20
INPUT_CAPACITY = 17_815
INPUT_KEY_OFFSETS = (14, 15, 16, 17)
GAME_HZ = 30.0
NETWORK_TUS = 2
MODE_NAMES = {0: "VS", 1: "Stage", 2: "1 on 1", 3: "2 on 2", 4: "Battle"}
DIFFICULTY_NAMES = {-1: "CRAZY", 0: "Difficult", 1: "Normal", 2: "Easy"}
BACKGROUND_NAMES = {
    0: "HK Colliseum", 1: "Lion Forest", 2: "Stanley Prison", 3: "Great Wall",
    4: "Queen Island", 5: "Forbidden Tower", 6: "Brokeback Cliff",
    7: "CUHK", 8: "TaiHom Village", 99: "Lee On Road",
}
CHARACTER_NAMES = {
    0: "template", 1: "deep", 2: "john", 4: "henry", 5: "rudolf",
    6: "louis", 7: "firen", 8: "freeze", 9: "dennis", 10: "woody",
    11: "davis", 30: "bandit", 31: "hunter", 32: "mark", 33: "jack",
    34: "sorcerer", 35: "monk", 36: "jan", 37: "knight", 38: "bat",
    39: "justin", 50: "louisEX", 51: "firzen", 52: "julian",
}
PLAYER_STATUS_NAMES = {-1: "lose", 1: "win & dead", 2: "win & alive"}

KEY_NAMES = (
    (0x80, "down"),
    (0x40, "up"),
    (0x20, "left"),
    (0x10, "right"),
    (0x08, "attack"),
    (0x04, "jump"),
    (0x02, "defend"),
)


def extract_key(exe: Path) -> bytes:
    data = exe.read_bytes()
    end = KEY_FILE_OFFSET + KEY_LEN
    if len(data) < end:
        raise ValueError(f"{exe}: too small to contain key at 0x{KEY_FILE_OFFSET:X}")
    key = data[KEY_FILE_OFFSET:end]
    digest = hashlib.sha256(key).hexdigest()
    if not all(0x30 <= b <= 0x39 for b in key):
        raise ValueError("extracted key is not all ASCII digits; unsupported summary-generator build")
    if digest != EXPECTED_KEY_SHA256:
        raise ValueError(
            "unexpected key SHA-256; this tool currently supports the stock 2008 "
            f"summary generator only (got {digest})"
        )
    return key


def decode_lfr(path: Path, key: bytes) -> bytes:
    raw = path.read_bytes()
    if len(raw) < 5:
        raise ValueError(f"{path}: truncated LFR")
    (packed_size,) = struct.unpack_from("<I", raw, 0)
    if packed_size <= 0 or packed_size > len(raw) - 4:
        raise ValueError(
            f"{path}: invalid compressed size {packed_size} (available {len(raw)-4})"
        )
    payload = bytearray(raw[4 : 4 + packed_size])
    n = min(KEY_LEN, len(payload))
    for i in range(n):
        payload[i] = (payload[i] - key[i] + 0x30) & 0xFF
    try:
        decoded = zlib.decompress(payload)
    except zlib.error as exc:
        raise ValueError(f"{path}: zlib decompression failed: {exc}") from exc
    if len(decoded) != DECODED_SIZE:
        raise ValueError(
            f"{path}: decoded size {len(decoded)} != expected stock LF2 size {DECODED_SIZE}"
        )
    return decoded


def key_text(v: int) -> str:
    names = [name for bit, name in KEY_NAMES if v & bit]
    unknown = v & ~0xFE
    if unknown:
        names.append(f"unknown=0x{unknown:02X}")
    return "+".join(names) if names else "idle"


def iter_input_records(decoded: bytes):
    end = INPUT_START + INPUT_STRIDE * INPUT_CAPACITY
    if len(decoded) < end:
        raise ValueError("decoded recording is too small for known input table")
    for frame in range(INPUT_CAPACITY):
        off = INPUT_START + frame * INPUT_STRIDE
        rec = decoded[off : off + INPUT_STRIDE]
        keys = tuple(rec[i] for i in INPUT_KEY_OFFSETS)
        yield frame, off, rec, keys


def i32(decoded: bytes, off: int) -> int:
    return struct.unpack_from("<i", decoded, off)[0]


def player_summary(decoded: bytes, index: int) -> dict | None:
    role = i32(decoded, 0x14 + index * 4)
    if role == -1:
        return None
    char_id = i32(decoded, 0x34 + index * 4)
    raw_name = decoded[0x14C + index * 11 : 0x14C + index * 11 + 11]
    name = raw_name.split(b"\0", 1)[0].decode("latin1", "replace")
    if role != 1:
        name = "[com]"
    status = i32(decoded, 0x114 + index * 4)
    return {
        "slot": index + 1,
        "role": "human" if role == 1 else "computer",
        "role_raw": role,
        "name": name,
        "character": char_id,
        "character_name": CHARACTER_NAMES.get(char_id, f"Unknown({char_id})"),
        "team": i32(decoded, 0x54 + index * 4),
        "kill": i32(decoded, 0x74 + index * 4),
        "attack": i32(decoded, 0x94 + index * 4),
        "hp_used": i32(decoded, 0xB4 + index * 4),
        "mp_used": i32(decoded, 0xD4 + index * 4),
        "picking": i32(decoded, 0xF4 + index * 4),
        "status": status,
        "status_name": PLAYER_STATUS_NAMES.get(status, ""),
    }


def recording_summary(path: Path, decoded: bytes) -> dict:
    nonzero = []
    used_slots = [0, 0, 0, 0]
    last_nonzero = -1
    for frame, off, rec, keys in iter_input_records(decoded):
        if any(keys):
            last_nonzero = frame
            nonzero.append((frame, keys))
        for i, v in enumerate(keys):
            if v:
                used_slots[i] += 1
    mode = i32(decoded, 0x148)
    difficulty = i32(decoded, 0x000)
    background = i32(decoded, 0x1A4)
    movie_tus = i32(decoded, 0x144)
    stage_raw = i32(decoded, 0x004)
    stage_index = stage_raw // 10 if mode == 1 and stage_raw >= 0 else None
    stage_name = None
    if stage_index is not None:
        stage_name = "Survival" if stage_index >= 5 else f"Stage {stage_index + 1}"
    players = [p for i in range(8) if (p := player_summary(decoded, i)) is not None]
    return {
        "file": path.name,
        "decoded_size": len(decoded),
        "decoded_sha256": hashlib.sha256(decoded).hexdigest(),
        "mode": mode,
        "mode_name": MODE_NAMES.get(mode, f"Unknown({mode})"),
        "difficulty": difficulty,
        "difficulty_name": DIFFICULTY_NAMES.get(difficulty, f"Unknown({difficulty})"),
        "background": background,
        "background_name": BACKGROUND_NAMES.get(background, f"Unknown({background})"),
        "stage_raw": stage_raw,
        "stage_name": stage_name,
        "movie_time_units_30hz": movie_tus,
        "movie_seconds": round(movie_tus / GAME_HZ, 3),
        "stage_cleared": bool(i32(decoded, 0x8C0)) if mode == 1 else None,
        "f6_f9_used": any(i32(decoded, off) for off in (0x8B0, 0x8B4, 0x8B8, 0x8BC)),
        "players": players,
        "input_start": INPUT_START,
        "input_stride": INPUT_STRIDE,
        "input_rate_hz": GAME_HZ / NETWORK_TUS,
        "input_capacity": INPUT_CAPACITY,
        "last_nonzero_packet": last_nonzero,
        "last_nonzero_game_tu": None if last_nonzero < 0 else last_nonzero * NETWORK_TUS,
        "last_nonzero_seconds": None if last_nonzero < 0 else round(last_nonzero * NETWORK_TUS / GAME_HZ, 3),
        "nonzero_packets": len(nonzero),
        "slot_nonzero_packets": used_slots,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("lfr", type=Path, nargs="+", help="stock .lfr recording(s)")
    ap.add_argument(
        "--summary-exe",
        type=Path,
        required=True,
        help="stock lfr_summary_generator.exe used to extract the 1345-byte digit key",
    )
    ap.add_argument("--out-dir", type=Path, help="write decoded .bin files here")
    ap.add_argument("--inputs", action="store_true", help="print non-idle 20-byte input records")
    ap.add_argument("--all-inputs", action="store_true", help="print every input record, including idle")
    ap.add_argument("--json", action="store_true", help="emit machine-readable JSON summaries")
    args = ap.parse_args()

    key = extract_key(args.summary_exe)
    summaries = []
    if args.out_dir:
        args.out_dir.mkdir(parents=True, exist_ok=True)

    for path in args.lfr:
        decoded = decode_lfr(path, key)
        summary = recording_summary(path, decoded)
        summaries.append(summary)
        if args.out_dir:
            out = args.out_dir / (path.stem + ".decoded.bin")
            out.write_bytes(decoded)
        if args.inputs or args.all_inputs:
            print(f"# {path}")
            for frame, off, rec, keys in iter_input_records(decoded):
                if not args.all_inputs and not any(keys):
                    continue
                desc = ", ".join(f"P{i+1}=0x{v:02X}({key_text(v)})" for i, v in enumerate(keys))
                print(f"{frame:5d}  t={frame*NETWORK_TUS/GAME_HZ:8.3f}s  off=0x{off:06X}  {desc}")

    if args.json:
        print(json.dumps(summaries, indent=2))
    elif not (args.inputs or args.all_inputs):
        for s in summaries:
            print(
                f"{s['file']}: {s['mode_name']} {s['stage_name'] or ''} diff={s['difficulty_name']} "
                f"bg={s['background_name']} movie={s['movie_seconds']}s decoded={s['decoded_size']} "
                f"sha256={s['decoded_sha256'][:16]}... last_input_packet={s['last_nonzero_packet']} "
                f"({s['last_nonzero_seconds']}s) slot_activity={s['slot_nonzero_packets']}"
            )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
