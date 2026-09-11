# LF2Vita PC compatibility WIP

Branch: `pc-compat-wip`

Goal: LF2Vita must interoperate with an **unmodified Little Fighter 2 v2.00a for Windows** over the original TCP network mode. The companion/streaming PC client is diagnostic only and is not the target architecture.

`main` stays stable. Continue PC compatibility work on this branch.

## Current stock transport

`src/net70/pc_net.c` implements the original TCP/12345 path:

1. 14-byte `u can connect\0` banner.
2. 77-byte player/slot identity block in each direction.
3. 3001-byte shared random block from host to client.
4. Repeating 22-byte lockstep packets.

Control ownership follows stock LF2:

- host / `Waiting for opponent`: global controls 1..4;
- client / `Connect to opponent`: global controls 5..8.

Known key bits: baseline `0x01`, defend `0x02`, jump `0x04`, attack `0x08`, right `0x10`, left `0x20`, up `0x40`, down `0x80`.

Host keys are packet bytes 4..7 and client keys are 8..11. Opaque bytes 0..3 and 12..21 are preserved instead of fabricated and are logged when they change. Public reverse-engineering discussion indicates that one undocumented field, possibly byte 14 depending on indexing, is a health/state checksum involved in LF2's out-of-sync detection. Its algorithm is still unknown and must be mapped from a real stock-PC capture rather than guessed.

## Clock and RNG

The original network exchange is **15 Hz** while the game simulation is **30 Hz**. One network sample is held for two game TUs. The stock `.lfr` recordings independently confirm exactly the same 2-TU cadence.

`src/lf2_rng.c` implements the reverse-engineered v2.00a random routine:

```text
i = (i + 1) % 1234
j = (j + 1) % 3000
random(range) = (i + table[j]) % range
```

The table is the first 3000 bytes of the shared random block. Client mode therefore uses the exact table supplied by Windows LF2. Host-side generation of the 3001-byte block is deterministic but the exact Windows generator for that block is not yet proven.

## Stock menu state

Implemented:

- original-clock Game Mode selection;
- VS join / fighter / team / confirm flow;
- CPU count and CPU fighter/team selection;
- VS options and match launch;
- four local + four remote stock control slots mapped into actor order.

Still required:

- Stage network state machine;
- 1-on-1 Championship state machine;
- 2-on-2 Championship state machine;
- Battle network state machine;
- Demo network state machine;
- exact combat/state/checksum parity.

The official game description confirms that Championship is a tournament mode and supports solo or partner play; this is useful for semantics but not enough to assume exact menu timing/state transitions. Those must remain synchronized with the Windows executable.

## `.lfr` decoder and reference data

`tools/lfr_decode.py` reproduces the stock `lfr_summary_generator.exe` decoder without embedding its key. It extracts the 1345-byte digit key from the user's stock utility and verifies its expected SHA-256.

All ten bundled recordings decompress to exactly **6,491,672 bytes**.

Confirmed global offsets:

- `0x000`: difficulty (`-1` CRAZY, `0` Difficult, `1` Normal, `2` Easy)
- `0x004`: Stage progress
- `0x144`: movie length in 30-Hz TUs
- `0x148`: mode (`0` VS, `1` Stage, `2` 1-on-1, `3` 2-on-2, `4` Battle)
- `0x1A4`: background id
- `0x8B0..0x8BC`: F6..F9 flags
- `0x8C0`: Stage-cleared flag
- `0x8C8`: 3000-byte stock RNG table
- `0x630BC0`: author name
- `0x630C24`: author info/email

Per-player arrays for `i=0..7`:

- role `0x14+i*4` (`-1` absent, `0` COM, `1` human)
- character `0x34+i*4`
- team `0x54+i*4`
- kills `0x74+i*4`
- attack `0x94+i*4`
- HP used/lost `0xB4+i*4`
- MP used `0xD4+i*4`
- picking `0xF4+i*4`
- result/status `0x114+i*4`
- name `0x14C+i*11`

Input table:

- starts at `0x2B98`;
- 17,815 records;
- 20 bytes/record;
- each record is one 15-Hz sample / two 30-Hz TUs;
- local keys are offsets `+14..+17`;
- the bundled one-human demos use `+14` and normally leave `+15..+17` zero;
- key masks match network action/direction bits except network baseline bit `0x01` is absent.

`Demo_Survival` outlives the fixed 17,815-record input table by 100 game TUs, so its final tail cannot currently be treated as a complete exact-input oracle. Do not silently synthesize missing input data.

## Compact `.l2rf` reference fixture

`tools/lfr_decode.py --fixture-dir <dir>` exports compact fixtures consumed by `src/replay_ref.c/.h`:

1. 40-byte header;
2. eight 48-byte player records;
3. original 3000-byte RNG table;
4. `packet_count * 4` input bytes.

The local internal build has fixtures for all ten stock demos. Packaging is conditional so the branch remains buildable without redistributing those game-derived files.

### Championship team code finding

`Demo_1on1` stores teams as **10** and **11**, unlike normal VS teams 1..4. In the bundled reference corpus these are confirmed to be the two 1-on-1 championship sides. `replay_ref.c` now normalizes only this proven case:

- mode 2 + raw team 10 -> Vita team 1
- mode 2 + raw team 11 -> Vita team 2

Other unknown/out-of-range team codes are rejected instead of guessed. The stat comparator compares against the normalized match plan, not the raw championship side id.

All ten fixtures were validated for known character IDs and valid normalized Vita team values after this change.

## Deterministic reference runner

The hidden/internal replay path is now implemented for non-Stage fixtures:

- loads `.l2rf` from `app0:/ref/` with `ux0:data/LF2V00001/ref/` fallback;
- recreates roster, teams, difficulty and background;
- installs the exact recorded stock RNG table;
- applies each recorded 15-Hz input sample for exactly two 30-Hz TUs;
- continues to the exact recorded TU limit even if a side has already reached KO;
- can fast-forward up to 32 simulation TUs per rendered frame;
- returns structured fighter counters;
- compares `roster`, normalized `team`, `kills`, `attack`, `hp_lost`, `mp_used` and `picking` against Windows-LF2 recording values;
- logs mismatches as `REFCMP`.

Stage replay execution is deliberately deferred until the Stage runtime can reproduce the stock recording state without inventing missing semantics.

The stock runtime logs a deterministic scalar state digest at TU 1 and every 30 TUs, including RNG indices/call count. `src/fix3/game_stock_diag.inc` hashes explicit scalar fields only (not pointers/padding) and includes the separate `bdefend` counter. The stock-only hitlag layer extends this digest with each fighter's signed shaking value. This will be used to locate the first divergence once comparable Windows checkpoints are available.

## Combat parity already changed in stock mode

Where identified, Vita-only behavior is disabled while stock compatibility is active:

- no victim-wide one-direct-hit-per-TU convenience latch;
- no Vita-only CPU difficulty damage multiplier;
- stock `arest` / per-victim `vrest` remain the repeat-hit mechanism;
- normal landed hits use authored `injury` rather than the Vita fallback/minimum-damage rules;
- fighters carry a distinct `bdefend` counter which decays by one point per 30-Hz TU;
- a block is based on authored `state: 7`, not merely frame 110;
- a successful stock block adds the ITR/weapon-strength `bdefend`, applies one tenth of authored `injury` with integer truncation, and uses frame 111 when appropriate;
- grounded defense breaks to frame 112 once accumulated `bdefend` exceeds 30;
- authored `bdefend > 60` bypasses normal defense;
- an unblocked stock hit sets the victim `bdefend` counter to 45;
- `fall < 0` suppresses the normal injury-frame transition while retaining authored push/vertical velocity;
- stock fall reactions use the 20/40/60 meter categories, including directional 222/224 injury2 frames and directional 180/186 falling entry;
- airborne hits at or below the low-fall category enter 222/224, while an airborne victim whose accumulated fall exceeds 20 enters the falling sequence;
- type-0 falling frames are trajectory-driven in stock mode instead of cycling 180..191 as an animation: the already selected 180-series or 186-series is mapped from vertical velocity using `< -10`, `< 0`, `< 6`, and `>= 6` bands;
- stock local hitlag/shaking is represented independently of `fighter_t`: direct fighter/held-weapon attackers freeze for 3 TU, normally hit victims for 3 TU, and defended victims for 5 TU;
- a fighter in stock hitlag does not advance normal input, AI decisions, frame animation/physics, hard-coded frame-state processing, catch/item commands, or fighter opoint emission;
- attacker-side `arest` pauses during attacker hitlag, while per-victim `vrest` continues to count down;
- `fall` recovery pauses during hitlag, while the confirmed stock `bdefend` decay remains one point per TU;
- projectile/object contact applies victim hitlag without freezing the owning fighter; this avoids incorrectly giving ordinary type-3 projectiles fighter-style attacker hitlag.

The back-facing exception for negative `dvx` is included in the block test. Airborne or caught state-7 frames do not enter broken-defend even when their counter is above 30.

The stock fall/hitlag code is intentionally limited to lockstep/reference stock mode. Normal Vita/Stage gameplay keeps its previous reaction path.

Still unresolved inside the hit/fall/state-12 path:

- exact hitlag momentum behavior: Windows accumulates attacker/victim momentum during normal hitlag and appears to decelerate it during defend hitlag; this is deliberately not approximated yet;
- exact object-local hitshake/frame transitions for every non-character object class;
- exact ground-bounce behavior and velocity constants for falling frames 185/191;
- `fall: 80` and other hard-coded/special fall cases;
- effect/weapon special cases that reinterpret negative fall values.

Result counters are tracked for attack dealt, HP lost, kills, MP used and item picking.

Other important remaining combat mismatches include armor/special defensive characters, the hard-coded `bdefend: 100` weapon/armor behavior, grab/throw accounting, sustained effects, owner attribution, AI/RNG call order, and exact item/object processing order. Do not guess the armor thresholds or the `bdefend: 100` weapon-destruction path without Windows evidence.

## Build status

The last full clean VitaSDK build was the pre-bdefend tree after the 1-on-1 team normalization. That internal VPK passed `unzip -t` with all ten optional local reference fixtures included.

While continuing from that handoff, the GitHub split was found to contain two source-sync defects that the prior local tree did not expose: `game_21.inc` passed an extra `stock_compat` argument to the existing hit functions, and it called `stock_state_digest()` without a definition in the branch. The bdefend change restores the existing hit-function signatures, scopes stock behavior through the lockstep hit pass, and adds the missing deterministic digest implementation.

The bdefend/digest helper path, stock fall reactions and stock hitlag timing have been exercised with strict host-side C syntax/semantic harnesses (`-std=c11 -Wall -Wextra -Werror`). The hitlag harness covers 3-TU attacker freeze, 3-TU normal-victim freeze, 5-TU defended-victim freeze, attacker `arest` pause, continuing `vrest`/`bdefend` decay, paused `fall` recovery and the KO path without double-decrementing shaking.

A **fresh full VitaSDK compile plus deterministic fixture run is still required** for the current tree. The uploaded SDK packages are available, but this session does not have a complete local repository checkout to link the application from. Therefore this remains an engineering branch, **not** the next user hardware-test VPK.

## Next engineering tasks

1. Clean-build the current GitHub tree and run the deterministic non-Stage fixtures; use `REFCMP`/`STOCK` to identify the first reproducible simulation divergence.
2. Finish exact hitlag momentum accumulation/deceleration and State-12 185/191 bounce behavior.
3. Resolve armor/special defensive characters and the `bdefend: 100` bypass/weapon behavior from Windows evidence rather than approximation.
4. Finish exact result/stat accounting for status/self/grab/throw paths.
5. Implement the stock 1-on-1 and 2-on-2 Championship menu/bracket state machines without assuming unverified timing.
6. Map the opaque packet health/state checksum from a real Windows capture.
7. Implement Stage/Battle/Demo stock network state machines.
8. Only then issue the next hardware-test VPK for an unmodified Windows LF2 2.00a peer.
