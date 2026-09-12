# LF2Vita Current Handoff

This file is the canonical restart point for a new ChatGPT session.

## Branch and artifact policy
- Work only on `pc-compat-wip`; keep `main` untouched.
- Every user-facing checkpoint is one Source+Workspace ZIP plus one separate VPK.
- `game.lf2pak` is user-provided/proprietary and must never be committed.
- A state is not called verified until the official VitaSDK GitHub Actions job is green.
- The VPK must be repacked with that CI `eboot.bin`, then the embedded SELF hash must match CI exactly.

## Current native Vita AdHoc design
The next consolidated build uses protocol v4 and is the first full optimization pass rather than another small patch.

Transport properties:
- 30 Hz deterministic lockstep with exactly one TU of input delay.
- Neutral frame-0 post-load barrier.
- Every input packet carries an 8-frame recovery window.
- Matching thread moved to Vita user CPU 2 to reduce contention with gameplay/audio.
- Matching retry/retransmit tuned for local-AdHoc RTT (5 retries, 30 ms library rexmit interval).
- While blocked, the current packet is retried every 8 ms; frame 0 retries every 50 ms.
- Regular frame safety deadline is 1.5 s; load barrier deadline is 5 s.
- `MATCH_DATA_CONFIRM` is not queued; `MATCH_DATA_TIMEOUT` is soft and handled by lockstep retry instead of immediately killing the peer.
- Event queue increased to 256 entries with drop/peak diagnostics.
- Host lobby state and client character/ready state refresh every 250 ms so `DATA_BUSY` cannot permanently lose a state transition.
- Periodic simulation digests are exchanged inside input packets. A mismatch logs `NETDESYNC` and aborts instead of silently continuing out of sync.
- During the network critical path, high-volume ANIM/AUDIO trace lines are suppressed and filesystem sync cadence is relaxed; NET/NETSTATE/HIT/GAME/STAGE diagnostics remain.
- `tools/test_adhoc_protocol.py` stress-tests the recovery contract with deterministic packet loss, reordering and 80-360 ms scheduler spikes; CI runs it before compiling.

## Hardware evidence that motivated v4
- Successful v3 host/client runs were simulation-identical through TU 990 (`6468D025` on both devices).
- Browser RTT was about 14 ms.
- v3 still had rare delivery/scheduling stalls: one match disconnected around frames 1435/1437 and another startup around frame 1/3.
- Therefore the remaining problem was transport delivery/jitter, not a proven gameplay-state divergence.

## Native two-Vita game modes now wired
All five lobby modes route through the native Vita lockstep; the current transport supports exactly two physical Vitas.
- VS: host vs client.
- Stage: cooperative Host + Client as the two human party members. The chosen Stage number is the starting major stage; progression continues through Stage 5. Stage enemy/AI generation uses a deterministic shared seed.
- Battle: each human leads two deterministic stock Bandit soldiers.
- 1 vs 1 Championship: two-player championship final (host vs client).
- 2 vs 2 Championship: each human gets one deterministic CPU partner; host/client remain the two network-controlled actors.
- Lobby player-count control is intentionally fixed to `2 Vitas` until multi-peer lockstep is designed and verified.

## Stage-specific fixes already present
- Rudolf clones/summons use dedicated dynamic fighter slots and cannot occupy authored Stage enemy-wave slots.
- Cooperative Stage camera/enemy spawn reference living human players, not COM allies.
- Single-player Stage keeps its original P1-death behavior; the co-op survival rule is only enabled for network Stage.

## PC compatibility boundary
- Native Vita AdHoc must never be routed through the reduced retail-PC input adapter.
- Original LF2 2.00a PC interoperability remains a separate TCP/12345 path with stock 8-bit controls and stock-simulation compatibility work.

## Next action after a chat restart
1. Read this file and `PC_COMPAT_WIP.md` from `pc-compat-wip`.
2. Confirm the branch head and latest CI run.
3. If CI for the optimization/all-modes commit is green, package that exact CI `eboot.bin` into the user-provided VPK base and verify hashes.
4. Only then ask for one consolidated two-Vita hardware test covering a long VS match plus Stage/Battle/1v1/2v2 smoke tests; do not ask the user to test intermediate transport patches.
