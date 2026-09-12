# LF2Vita Current Handoff

This file is the canonical restart point for a new ChatGPT session.

## Branch and artifact policy
- Work only on `pc-compat-wip`; keep `main` untouched.
- Integration work may temporarily live on `pc-compat-v6-integration`, but a user-facing checkpoint must be moved back to `pc-compat-wip` and pass CI first.
- Every user-facing checkpoint is one Source+Workspace ZIP plus one separate VPK.
- `game.lf2pak` is user-provided/proprietary and must never be committed.
- A state is not called verified until the official VitaSDK GitHub Actions job is green.
- The VPK must be repacked with that CI `eboot.bin`, then the embedded SELF hash must match CI exactly.
- Do not ask the user to test intermediate transport patches; only consolidated checkpoints.

## Current native Vita AdHoc design
Current integration protocol is v6.

Transport properties:
- 30 Hz deterministic lockstep with exactly one TU of input delay.
- Neutral frame-0 post-load barrier with two-way ACK confirmation.
- Every input packet carries an 8-frame recovery window.
- Match-input packets are consumed directly on the Vita matching callback into a mutex-protected cache; lobby/system traffic remains on the main event queue.
- Matching worker uses Vita user CPU 2 to reduce contention with gameplay/audio.
- Matching retry/retransmit tuned for local AdHoc RTT (5 retries, 30 ms library rexmit interval).
- Current input prefetch retries every 8 ms; frame 0 retries every 50 ms.
- ACKed prefetches suppress redundant retransmits.
- Regular frame safety deadline is 1.5 s; load/start barrier deadline is 5 s.
- STARTING lobby state is refreshed during frame-0 recovery, so a lost start packet cannot strand the guest in the lobby.
- `MATCH_DATA_CONFIRM` is not queued; `MATCH_DATA_TIMEOUT` is soft and handled by lockstep retry instead of immediately killing the peer.
- Event queue capacity is 256 entries with drop/peak diagnostics.
- Host lobby state and client character/ready state refresh every 250 ms so `DATA_BUSY` cannot permanently lose a state transition.
- Periodic simulation digests are exchanged inside input packets. A mismatch logs `NETDESYNC` and aborts instead of silently continuing out of sync.
- During the network critical path, high-volume ANIM/AUDIO trace lines are suppressed and filesystem sync cadence is relaxed; NET/NETSTATE/HIT/GAME/STAGE diagnostics remain.
- `tools/test_adhoc_protocol.py` stress-tests deterministic packet loss, burst loss, reordering, scheduler/radio spikes and lost START transitions; CI runs it before compiling.

## Lobby and active-player model
- Lobby advertises/selects 2 through 8 Vitas.
- All eight lobby slots are synchronized and displayed.
- The currently hardware-validated gameplay path still uses two active physical Vitas: slot 0 host plus the first occupied guest slot.
- Additional connected guest slots remain in the lobby as spectators during that match and do not block Ready/START.
- Spectator disconnect/timeouts do not terminate the active host/guest lockstep.
- The host native match peer is selected by active guest slot instead of requiring exactly one matching peer.
- This preserves an 8-player-capable lobby/UI without pretending that 3-8-way lockstep has already been hardware-validated.

## Native game modes wired
All five lobby modes route through the native Vita path for the active host/guest pair.
- VS: host vs active guest.
- Stage: cooperative host + active guest as the two human party members. The chosen Stage number is the starting major stage; progression continues through Stage 5. Stage enemy/AI generation uses a deterministic shared seed.
- Battle: host and guest each choose their own hero, follower type and follower count.
- 1 vs 1 Championship: deterministic 16-entry tournament path; CPU-only pairings resolve deterministically and human-involved pairings use the network match runner.
- 2 vs 2 Championship: deterministic 8-team tournament path; each Vita chooses its own CPU partner and human-involved pairings use the network match runner.

## Stage-specific fixes already present
- Rudolf clones/summons use dedicated dynamic fighter slots and cannot occupy authored Stage enemy-wave slots.
- Cooperative Stage camera/enemy spawn reference living human players, not COM allies.
- Single-player Stage keeps its original P1-death behavior; the co-op survival rule is only enabled for network Stage.

## Hardware evidence
- Successful earlier host/client runs were simulation-identical through TU 990 (`6468D025` on both devices).
- Browser RTT was about 14 ms.
- Earlier v3 had rare delivery/scheduling stalls around frames 1435/1437 and startup 1/3; this motivated the callback fast path, larger recovery window, ACKs and start barrier hardening.
- No hardware claim should be made for 3-8 simultaneous active players yet; only lobby/spectator handling is implemented for those extra slots.

## PC compatibility boundary
- Native Vita AdHoc must never be routed through the reduced retail-PC input adapter.
- Original LF2 2.00a PC interoperability remains a separate TCP/12345 path with stock 8-bit controls and stock-simulation compatibility work.

## Next action after a chat restart
1. Read this file and `PC_COMPAT_WIP.md`.
2. Confirm `pc-compat-wip` head, `main` head, and latest CI run.
3. If the v6 integration commit has been moved to `pc-compat-wip`, wait for/inspect CI including `Test native AdHoc protocol`, ELF/SELF build and artifact packaging.
4. If CI is green, preserve that checkpoint and continue network-mode fidelity or multi-peer work without requiring an intermediate user hardware test.
5. Only when a larger consolidated block is ready, package the exact CI `eboot.bin` into the separate VPK, verify hashes, and provide Source+Workspace ZIP plus VPK.
