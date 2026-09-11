# LF2Vita PC compatibility WIP

Branch: `pc-compat-wip`

Goal: LF2Vita must interoperate with an **unmodified Little Fighter 2 v2.00a for Windows** over the original TCP network mode. The companion/streaming PC client is diagnostic only and is not the target architecture.

## Continuity / exact local source snapshot

The branch now contains an exact compressed snapshot of the important local WIP source files at:

`wip/pc-compat-source-snapshot.tar.gz.b64`

Decode it with:

```sh
base64 -d wip/pc-compat-source-snapshot.tar.gz.b64 > /tmp/pccompat.tar.gz
tar -xzf /tmp/pccompat.tar.gz
```

Decoded tar SHA-256:

`66a9c876471122e7ece481f06a021ff5cd906fc7d94ae60f4f9f49a0c1d5f893`

This snapshot exists specifically so a new chat/developer can recover the exact local WIP even while some large split `.inc` integrations are still being normalized into regular branch files.

## Current original-LF2 transport

`src/net70/pc_net.c` implements the stock TCP/12345 path.

Handshake implemented:

1. 14-byte `u can connect\0` banner.
2. 77-byte player/slot identity block in each direction.
3. 3001-byte shared random table from host to client.
4. Repeating 22-byte lockstep packets.

Control ownership follows stock LF2:

- Host / "Waiting for opponent": global control slots 1..4.
- Client / "Connect to opponent": global control slots 5..8.

Known key-byte bits:

- bit 0 (`0x01`) is the stock network baseline bit.
- defend `0x02`
- jump `0x04`
- attack `0x08`
- right `0x10`
- left `0x20`
- up `0x40`
- down `0x80`

The four host keys are bytes 4..7 and the four client keys are bytes 8..11 of the 22-byte packet. Other bytes are deliberately preserved rather than guessed. Diagnostics count/log changes of opaque bytes 0..3 and 12..21.

Public reverse-engineering discussion says one undocumented field, possibly packet byte 14 depending on indexing, is a **health/state checksum** and is what raises the out-of-sync error. Its algorithm is still unknown. Do **not** freeze or fabricate it as a final compatibility solution; preserve it until it is mapped from a real stock-PC capture.

## Network clock

The stock network exchange is **15 Hz**, while the game simulation is **30 Hz**. One 22-byte exchange is latched for two game time units. `lf2_pc_stock_clock_step()` is called once per 30 Hz TU and performs a network exchange only on the first TU of each pair.

This is independently confirmed by the stock `.lfr` input table: one 20-byte recording input record corresponds exactly to two 30 Hz game TUs.

## Original RNG

`src/lf2_rng.c` implements the reverse-engineered v2.00a random routine:

```text
i = (i + 1) % 1234
j = (j + 1) % 3000
random(range) = (i + table[j]) % range
```

`table` is the first 3000 bytes of the 3001-byte handshake block. Both indices start at zero. Gameplay RNG call sites used by the Vita runtime have been routed through `lf2_rng_mod()` where identified. When stock mode is inactive it falls back to regular Vita RNG behavior.

Client mode therefore uses the exact table received from Windows LF2. Host mode currently generates a deterministic shared table but the exact Windows table-generator algorithm is not yet proven.

## Stock game/menu state

Implemented locally so far:

- stock Game Mode screen driven by the original input clock;
- VS mode join / fighter / team / confirm flow;
- CPU count and CPU fighter/team selection;
- VS options and match launch;
- four local + four remote control-slot mapping into the Vita match runtime.

Still incomplete:

- Stage mode network state machine;
- 1 on 1 / 2 on 2 championships;
- Battle mode;
- Demo mode;
- exact parity of all combat/state/checksum behavior.

Do not call this test-ready stock compatibility yet.

## Exact `.lfr` decoder

`tools/lfr_decode.py` reproduces the stock `lfr_summary_generator.exe` decoder.

File format:

1. first 4 bytes = little-endian compressed payload size;
2. first 1345 bytes of payload use a 1345-byte ASCII-digit key from the stock utility;
3. decrypt as `plain = encrypted - key_digit + '0'` modulo 256;
4. zlib-decompress.

The decoder does not embed the proprietary key. It extracts it from the user's stock `lfr_summary_generator.exe` at file offset `0xB750` and verifies key SHA-256 `a1e3e58e52cb7091bff6e1ea7ed8e14274b882f51134af69ca4ce0a72cc8613b`.

All ten bundled LF2 recordings decode to exactly **6,491,672 bytes**.

### Confirmed recording fields

Global:

- `0x000`: difficulty (`-1` CRAZY, `0` Difficult, `1` Normal, `2` Easy)
- `0x004`: Stage progress (`0,10,20,30,40,50`)
- `0x144`: movie length in 30 Hz game TUs
- `0x148`: mode (`0` VS, `1` Stage, `2` 1-on-1, `3` 2-on-2, `4` Battle)
- `0x1A4`: background id
- `0x8B0..0x8BC`: F6..F9 flags
- `0x8C0`: Stage-cleared flag
- **`0x8C8`: stock 3000-byte RNG table**
- `0x630BC0`: author name
- `0x630C24`: author info/email

Per player, `i=0..7`:

- role: `0x14 + i*4` (`-1` absent, `0` COM, `1` human)
- character id: `0x34 + i*4`
- team: `0x54 + i*4`
- kill: `0x74 + i*4`
- attack: `0x94 + i*4`
- HP used/lost statistic: `0xB4 + i*4`
- MP used: `0xD4 + i*4`
- picking: `0xF4 + i*4`
- result/status: `0x114 + i*4` (`-1` lose, `1` win & dead, `2` win & alive)
- human display name: `0x14C + i*11`

### Input table

- starts at `0x2B98`
- 17815 records
- 20 bytes per record
- one record = one **15 Hz** stock input/network sample = two 30 Hz game TUs
- local-player key bytes at record offsets `+14..+17`
- bundled one-human demos use `+14`; `+15..+17` are normally zero
- key bits match stock network action/direction bits except baseline bit `0x01` is absent.

Within the active movie span of the bundled one-human demos, the other recording-record bytes are constant zero, so `.lfr` itself does not expose the network health/checksum field.

## Compact deterministic replay fixture (`.l2rf`)

`tools/lfr_decode.py --fixture-dir <dir>` exports a compact fixture consumed by `src/replay_ref.c`.

Layout:

1. 40-byte header (`L2RF`, version, mode, difficulty, background, stage, movie TUs, packet count, RNG length, flags);
2. eight 48-byte player records containing role/character/team and stock result counters;
3. original 3000-byte RNG table;
4. `packet_count * 4` local stock input bytes.

`src/replay_ref.c/.h` can load the fixture, install its exact stock RNG table, map difficulty/background, and expose a lockstep callback which holds each recording sample for exactly two game TUs.

The replay path is designed for deterministic offline comparison, not for shipping recording files in the final VPK.

## Determinism diagnostics

The local gameplay WIP now logs a scalar stock-state digest at TU 1 and every 30 game TUs. The digest includes deterministic fighter/object scalar state plus original RNG indices/call count and deliberately excludes pointers/padding.

Example diagnostic form:

```text
STOCK tu=300 packet=150 digest=... rng_i=... rng_j=... rng_calls=...
```

This gives us a first-divergence locator once a Windows reference/capture exposes comparable checkpoints.

The runtime also accumulates per-fighter diagnostic result counters:

- attack dealt
- HP lost
- kills
- MP used
- item picking

and logs them as `STOCKSTAT` at stock-match cleanup. These are being matched against the corresponding final `.lfr` fields as a coarse end-to-end deterministic oracle.

Known accounting gaps still to resolve include some sustained/status/self-damage paths such as Sonata-style damage and exact original HP/MP accounting semantics.

## Combat-parity changes already made for stock mode

Vita-only convenience/balance behavior is disabled when stock compatibility is active where identified:

- the previous victim-wide "one direct fighter hit per TU" latch is not used in stock mode;
- Vita-only AI damage scaling by difficulty is not used in stock mode;
- original LF2 `arest` / per-victim `vrest` remain the repeat-hit mechanism.

Offline Vita play keeps its existing compatibility fixes for now.

Still to reconcile against the reverse-engineered Windows attack routine:

- frame/state-specific damage reductions (including the current Vita frame-110 rule);
- exact `fall` / `bdefend` ordering;
- grab/throw accounting;
- sustained effects;
- owner attribution and all result-stat edge cases;
- RNG call order in AI/object paths.

## Build status

Current local WIP builds successfully with VitaSDK after linking the replay module. Large runtime/network translation units are compiled with `-Os` to retain enough Vita ELF metadata headroom.

Latest internal WIP build at the time of this handoff passed VPK integrity checks. It is **not** a user hardware-test build yet.

## Next engineering tasks

1. Add a hidden developer replay runner which consumes `.l2rf`, recreates the exact roster/teams/background/difficulty, runs for exactly the recorded movie TUs, and compares `STOCKSTAT` with expected result fields.
2. Fix the first reproducible replay mismatches rather than guessing globally.
3. Finish exact result/stat accounting for unusual damage/status paths.
4. Continue mapping Windows attack/rest/fall/bdefend behavior and RNG call order.
5. Identify the stock packet health/state checksum from a real Windows capture without fabricating it.
6. Finish Stage / championship / Battle / Demo stock network menu state machines.
7. Only then ship the next hardware-test VPK for an unmodified Windows LF2 2.00a peer.
