# LF2Vita Current Handoff

This file is the canonical restart point for a new ChatGPT session. The branch head of `pc-compat-wip` is authoritative.

## Branch and artifact policy
- Work only on `pc-compat-wip`; keep `main` untouched.
- Every user-facing checkpoint is one Source+Workspace ZIP plus one separate VPK.
- `game.lf2pak` is user-provided/proprietary and must never be committed.
- A state is not called verified until the official VitaSDK GitHub Actions job is green.
- A user VPK must contain that exact CI `eboot.bin`; verify the embedded SELF hash after repacking.
- The user does not want to test every intermediate build. Finish coherent optimization/features internally, then provide one consolidated hardware test build.

## Native Vita AdHoc protocol v6
Protocol v6 is the current optimization checkpoint and is intentionally incompatible with older v2/v3/v4 test VPKs.

Transport properties:
- 30 Hz deterministic lockstep with exactly one TU of input delay.
- Neutral frame-0 post-load barrier.
- Every input packet carries an 8-frame recovery window.
- Matching worker runs on Vita user CPU 2 with a 64 KiB stack.
- Current packet retransmit while blocked: 8 ms; frame-0 resend: 50 ms.
- Regular safety deadline: 1.5 s; load barrier deadline: 5 s.
- `MATCH_DATA_CONFIRM` is not queued; `MATCH_DATA_TIMEOUT` is soft and handled by lockstep retry.
- Event queue capacity is 256 with peak/drop diagnostics.
- Host lobby and client player state refresh every 250 ms while waiting.
- Periodic simulation digests are exchanged. A mismatch logs `NETDESYNC` and aborts rather than continuing silently.
- During network-critical gameplay, high-volume ANIM/AUDIO trace output is suppressed and filesystem sync cadence is relaxed.
- `tools/test_adhoc_protocol.py` stress-tests 6000 frames across deterministic loss, burst loss, reordering, 70–520 ms spikes and lost START transitions. Current model result is about 12.48 ms average wait, 129 ms worst regular recovery and 1922 ms worst start recovery.

### Additional v6 hardening already landed
- Active `PKT_MATCH_INPUT` packets are consumed directly in the Vita matching callback into a dedicated mutex-protected cache.
- Match input is sent before draining non-critical queued events.
- Active peer address is captured at match begin instead of traversing mutable lobby peer tables from the callback.
- Host refreshes the complete `STARTING` state during the frame-0 barrier, so a lost start packet cannot strand the guest in the lobby.
- Hero/partner/Battle configuration changes clear Ready automatically.
- Native post-KO/result settling now stays on the same 30-Hz lockstep instead of using independent Vita wall clocks. KO physics, object updates and periodic NETSTATE digests continue until both peers leave the result phase on deterministic settle-TU thresholds.
- Native result simulation remains canonical for bracket/digest correctness, but the overlay is presentation-only: a participating guest Vita gets its own local `Sieg`/`Niederlage` view without inverting the authoritative match return value.
- After a native match the host returns to `WAITING` through one authoritative settings snapshot that clears Ready on every connected player, avoiding stale guest Ready state.

## Hardware evidence from earlier builds
- Successful host/client runs were simulation-identical through TU 990 (`6468D025` on both devices).
- Browser RTT was about 14 ms.
- Earlier rare disconnects were transport/scheduler stalls, including one around frames 1435/1437 and startup failures around frame 1/3; no deterministic divergence was proven.

## Native AdHoc game modes
The hardware-validation path remains two active players for now. All five lobby modes are wired through the native Vita lockstep:
- VS: host vs active guest.
- Stage: cooperative host + active guest; selected Stage is the starting major stage and progression continues through Stage 5.
- Battle: each human is a team hero with per-Vita follower type (`10..17`) and follower count (`0..3`).
- 1 vs 1 Championship: deterministic 16-entry bracket. Host and active guest enter opposite halves, CPU-only results are deterministic, and every human-involved bout uses native AdHoc lockstep.
- 2 vs 2 Championship: deterministic 8-team bracket with each Vita's selected CPU partner retained through the tournament.

AdHoc Championship round/bracket cards are timed and never wait for a local Continue button.

## 8-slot lobby role model
- Lobby capacity is configurable from 2 through 8 and the matching host accepts that many lobby seats.
- Current hardware-validation/gameplay pair is host slot 0 + the lowest occupied guest slot.
- Additional connected guests are explicit reserve/spectator seats and do not participate in Ready gating or native lockstep.
- Reserve clients do not auto-launch on `STARTING` and reserve disconnects do not abort the active match.
- If the active guest leaves in the lobby, the next occupied guest slot is promoted automatically.
- The user intends to hardware-test with two Vitas even though 2–8 slots are offered.

## Stage/gameplay fixes already present
- Rudolf clones/summons use dedicated dynamic fighter slots and cannot occupy authored Stage enemy-wave slots.
- Cooperative Stage camera/enemy spawn reference living human players, not COM allies.
- Single-player Stage keeps original P1-death behavior; co-op party survival is network-only.
- Native Vita AdHoc keeps Triangle/R/L, pickup, item use and run; it does not use the reduced retail-PC input adapter.
- Passive LF2 `itr kind:6` zones are not treated as active damage hits.
- Direct fighter self-hit (`attacker == victim`) is rejected.

## PC compatibility boundary
- Original LF2 2.00a PC interoperability remains a separate TCP/12345 path with stock 8-bit controls and stock-simulation compatibility work.
- Never route native Vita AdHoc through the PC stock-input adapter.

## Current checkpoint and next actions
1. The current cleanup integration branch contains the CI-green v6 base plus deterministic post-KO settlement, local result-view separation and full post-match Ready reset. Fast-forward it to `pc-compat-wip` and require official VitaSDK CI before calling the combined checkpoint verified.
2. Continue Battle/Stage/Championship fidelity work on the same transport.
3. Keep the lobby 2–8 capable while the active lockstep remains the validated two-Vita path; later extend actual input relay to >2 active Vitas.
4. Do not ask the user for intermediate hardware tests. Produce one consolidated test build only after the next larger coherent block is CI-green.
