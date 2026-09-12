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

v6 changes over v4:
- Active `PKT_MATCH_INPUT` packets are consumed directly in the Vita matching callback and written into a dedicated mutex-protected lockstep cache. The dedicated input mutex is explicitly created during AdHoc init; match lifecycle and captured peer address are protected by it. They no longer wait behind lobby/browser/system events in the main-thread event queue.
- Match-input cache lifetime/reset is protected against callback races at match begin/end.
- Host repeats the complete `STARTING` lobby state during the frame-0 barrier every 100 ms. Losing the original START packet can therefore no longer leave the client stuck in the lobby while the host waits in-game.
- Diagnostics include `fast_rx` and `start_refresh` counters.
- `tools/test_adhoc_protocol.py` identifies protocol v6, checks that the callback fast-path stays ahead of `queue_event`, and stress-tests 6000 frames across multiple deterministic seeds with independent loss, short burst loss, reorder and 70–520 ms scheduler/radio spikes. Current model result: worst recovery wait remains well below the 1.5 s safety deadline.

### Additional v6 transport hardening
- Match input is sent before draining non-critical queued events, maximizing the peer's 33 ms one-TU delivery budget.
- The active peer address is captured at match begin; the callback no longer traverses mutable lobby peer tables.
- Short waits use a 100 us early cadence before falling back to 300 us, reducing scheduler oversleep on near-ready frames.
- Changing a hero, 2v2 partner, Battle follower/count, or host match rules automatically clears readiness; this prevents stale-ready starts with an older character/config snapshot.

## Hardware evidence from earlier builds
- Successful host/client runs were simulation-identical through TU 990 (`6468D025` on both devices).
- Browser RTT was about 14 ms.
- Earlier rare disconnects were caused by delivery/scheduling stalls, including one around frames 1435/1437 and startup failures around frame 1/3.
- This evidence points to transport delivery/jitter, not a proven deterministic simulation divergence.

## Native AdHoc game modes
The hardware-validation path remains two active players for now. All five lobby modes are wired through the native Vita lockstep:
- VS: host vs active guest.
- Stage: cooperative host + active guest; selected Stage is the starting major stage and progression continues through Stage 5.
- Battle: each human is a team hero. Protocol v6 carries per-Vita follower type (`10..17`) and follower count (`0..3`).
- 1 vs 1 Championship: deterministic 16-entry bracket. Host and active guest enter opposite halves, CPU-only results are deterministic, and every human-involved bout uses native AdHoc lockstep.
- 2 vs 2 Championship: deterministic 8-team bracket with each Vita's selected CPU partner retained through the tournament.

AdHoc Championship round/bracket cards are timed and never wait for a local `Continue` button. This avoids one Vita advancing while the other remains on a local UI screen.

## 8-slot lobby role model
- Lobby capacity is configurable from 2 through 8 and the matching host accepts that many lobby seats.
- Current hardware-validation/gameplay pair is host slot 0 + the lowest occupied guest slot.
- Additional connected guests are explicit reserve/spectator seats and do not participate in Ready gating or native lockstep.
- Reserve clients do not auto-launch on `STARTING`; their periodic player refresh is suppressed while a match is active.
- Host native match peer selection targets the active guest slot instead of requiring exactly one connected peer.
- A reserve disconnect during a match no longer aborts the active match; an active-peer disconnect still does.
- If the active guest leaves in the lobby, the next occupied guest slot is promoted automatically.
- Client local-ready shadow is synchronized from host lobby packets so post-match ready reset remains authoritative.
- The user intends to hardware-test with two Vitas even though 2–8 slots are offered.

## Stage-specific fixes already present
- Rudolf clones/summons use dedicated dynamic fighter slots and cannot occupy authored Stage enemy-wave slots.
- Cooperative Stage camera/enemy spawn reference living human players, not COM allies.
- Single-player Stage keeps original P1-death behavior; co-op party survival is network-only.

## Input/gameplay fixes already present
- Native Vita AdHoc does not use the reduced retail-PC input adapter. Triangle/R/L, pickup, item use and run remain available.
- Passive LF2 `itr kind:6` zones are not treated as active damage hits.
- Direct fighter self-hit (`attacker == victim`) is rejected.

## PC compatibility boundary
- Original LF2 2.00a PC interoperability remains a separate TCP/12345 path with stock 8-bit controls and stock-simulation compatibility work.
- Never route native Vita AdHoc through the PC stock-input adapter.

## Next engineering actions
1. Treat the latest `pc-compat-wip` CI-green head as the base.
2. Synchronize post-KO/result settlement so native matches do not depend on independent local wall clocks after the deterministic fight is over.
3. Correct result perspective/UX for the joining Vita where necessary.
4. Continue Battle/Stage/Championship fidelity work on the same transport.
5. Keep the lobby 2–8 capable while the active lockstep remains the validated two-Vita path; later extend actual input relay to >2 active Vitas.
6. Do not ask the user for intermediate hardware tests. Produce one consolidated test build only after the next coherent block is CI-green.
