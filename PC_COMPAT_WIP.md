# LF2Vita PC compatibility WIP

Branch: `pc-compat-wip`

Goal: LF2Vita must interoperate with an **unmodified Little Fighter 2 v2.00a for Windows** over the original TCP network mode. The companion/streaming PC client is diagnostic only and is not the target architecture.

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

The four host keys are bytes 4..7 and the four client keys are bytes 8..11 of the 22-byte packet. Other bytes are deliberately preserved rather than guessed. Diagnostics now count/log changes of opaque bytes 0..3 and 12..21 so their HP/state/checksum semantics can be identified from a later stock-PC capture.

## Network clock

The stock network exchange is **15 Hz**, while the game simulation is **30 Hz**. One 22-byte exchange is latched for two game time units. `lf2_pc_stock_clock_step()` is called once per 30 Hz TU and performs a network exchange only on the first TU of each pair.

This is no longer just an inference from runtime behavior: the stock `.lfr` format independently confirms the same 2-TU cadence (see below).

## Original RNG

`src/lf2_rng.c` implements the reverse-engineered v2.00a random routine:

```text
i = (i + 1) % 1234
j = (j + 1) % 3000
random(range) = (i + table[j]) % range
```

`table` is the first 3000 bytes of the 3001-byte handshake block. Both indices start at zero. Gameplay RNG call sites used by the Vita runtime have been routed through `lf2_rng_mod()` where identified. When stock mode is inactive it falls back to the regular Vita RNG behavior.

## Stock game/menu state

Implemented locally so far:

- stock Game Mode screen driven by the original input clock;
- VS mode join / fighter / team / confirm flow;
- CPU count and CPU fighter/team selection;
- VS options and match launch;
- four local + four remote control-slot mapping into the Vita match runtime.

The branch contains the core PC transport/RNG interfaces. Some larger gameplay/UI integration files still need to be synchronized from the local WIP workspace before the branch is a byte-for-byte build snapshot.

Not complete yet:

- Stage mode state machine;
- 1 on 1 / 2 on 2 championships;
- Battle mode;
- Demo mode;
- exact parity of all combat/state/checksum behavior.

Do not call this test-ready stock compatibility yet.

## Exact `.lfr` decoder breakthrough

The stock `lfr_summary_generator.exe` was disassembled. Its decoder behavior is now reproduced by `tools/lfr_decode.py`.

File format:

1. first 4 bytes = little-endian compressed payload size;
2. first 1345 bytes of the payload are encrypted with a 1345-byte ASCII-digit key from the stock summary utility;
3. decrypt each of those bytes as `plain = encrypted - key_digit + '0'` modulo 256;
4. zlib-decompress the payload.

The tool does **not** embed the key. It extracts it from the user's stock `lfr_summary_generator.exe` at file offset `0xB750` and verifies the key SHA-256 `a1e3e58e52cb7091bff6e1ea7ed8e14274b882f51134af69ca4ce0a72cc8613b`.

All ten bundled LF2 recordings decode to exactly **6,491,672 bytes**.

### Replay input table

Exact table layout identified across all ten stock demos:

- table starts at decoded offset `0x2B98` (11160);
- 17815 records;
- 20 bytes per record;
- one record = one **15 Hz stock network exchange**, i.e. two 30 Hz game TUs;
- local-player key bytes are record offsets `+14..+17`;
- bundled one-human demos use `+14`; `+15..+17` stay zero;
- recording key masks use exactly the same action/direction bits as the network packet, except the network-only baseline bit `0x01` is absent.

The 15 Hz conclusion is verified against the movie-length field at decoded offset `0x144`, which is measured in 30 Hz game TUs. Example: Demo_1on1 has 1527 game TUs (~50.9 s); its last active input packet is #688, corresponding to ~45.9 s after multiplying each packet by two TUs. Survival fills the 17815-packet buffer and reaches ~20 minutes, matching the fixed recording capacity.

### Confirmed replay/player metadata

Global fields:

- `0x000`: difficulty (`-1` CRAZY, `0` Difficult, `1` Normal, `2` Easy)
- `0x004`: Stage progress in tens when mode is Stage (`0,10,20,30,40,50` => stages 1..5 / Survival)
- `0x144`: movie length in 30 Hz game TUs
- `0x148`: mode (`0` VS, `1` Stage, `2` 1-on-1, `3` 2-on-2, `4` Battle)
- `0x1A4`: background id
- `0x8B0..0x8BC`: F6..F9-use flags
- `0x8C0`: stage-cleared flag when in Stage mode
- `0x630BC0`: author name
- `0x630C24`: author info/email field used by the summary utility

Per-player arrays for slots `i=0..7`:

- role/type: `0x14 + i*4` (`-1` absent, `1` human, observed `0` = computer)
- character id: `0x34 + i*4`
- team: `0x54 + i*4`
- kill: `0x74 + i*4`
- attack: `0x94 + i*4`
- HP used: `0xB4 + i*4`
- MP used: `0xD4 + i*4`
- picking: `0xF4 + i*4`
- result/status: `0x114 + i*4` (`-1` lose, `1` win & dead, `2` win & alive)
- human display name: `0x14C + i*11` (11-byte slot string); COMs are displayed as `[com]` by the summary utility.

Character IDs confirmed from the stock summary utility include the normal roster (`deep=1`, `john=2`, `henry=4`, `rudolf=5`, `louis=6`, `firen=7`, `freeze=8`, `dennis=9`, `woody=10`, `davis=11`), Stage/CPU fighters (`bandit=30` through `justin=39`) and bosses (`louisEX=50`, `firzen=51`, `julian=52`).

This is enough to recreate the **exact initial roster, teams, mode, difficulty and background** for the bundled recordings, and to use the final stock result counters as a coarse deterministic oracle before a per-frame checksum is mapped.

Example: `Demo_VS.lfr` decodes as human `starsky` / Dennis on Team 1, COM Louis and Rudolf on Team 1, and COM Henry/Davis/John/Firen/Woody on Team 2. The decoded final statuses identify Team 1 as the surviving winner.

## Next engineering tasks

1. Build an offline replay/reference manifest from decoded LFR metadata and 15 Hz input records.
2. Feed each replay input record into the Vita 30 Hz runtime for exactly two TUs and reproduce the exact roster/background/difficulty from metadata.
3. Compare Vita end-of-replay counters against the stock LFR kill/attack/HP-used/MP-used/picking/status fields; then add finer checkpoints as more state fields are mapped.
4. Identify the opaque 22-byte packet fields from a later stock-PC capture; do not fabricate them.
5. Eliminate deterministic simulation differences, starting with hit/rest/fall/bdefend/RNG call order.
6. Implement the remaining stock network menu/mode state machines.
7. Only then ship a new hardware-test VPK for unmodified LF2 2.00a.

## Build

Current local source builds successfully with VitaSDK. The branch is WIP and should remain separate from `main` until stock interoperability is proven.
