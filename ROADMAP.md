# LF2Vita roadmap

## Completed foundation — 0.69 / fix3

The 0.69 line concentrated on PC LF2 2.00a gameplay fidelity: combat/rest rules, weapon use/throwing, catches/throws, special-command coverage, Stage corpse behavior, scrolling projectiles and one human player plus up to seven Stage COM allies. `PC_FIDELITY_AUDIT.md` remains the backlog for remaining gameplay mismatches.

The user's fix3 hardware log confirms the seven-allies Stage configuration reaches the live Stage simulation; hardware testing continues whenever gameplay regressions are reported.

## Current milestone — 0.7X Vita multiplayer

### 0.70 — Vita-to-Vita AdHoc discovery and lobby

Build the transport and lobby independently of combat synchronization:

- Network Play selector: **Online / AdHoc**.
- Real Vita AdHoc session discovery.
- Server browser with host/profile avatar cell, current/max players, status, game mode and five-bar measured connection quality.
- Host creation flow.
- Join/leave handling.
- Up to eight lobby players.
- Per-player LF2 character and ready state.
- Host settings synchronized live to every client.
- Useful `NET` diagnostics for two-device testing.

The first 0.70 test build intentionally does not start a network match yet.

### Next 0.7X step — AdHoc gameplay lockstep

Synchronize the existing 30 Hz native simulation between Vitas: common seed/start tick, input frames, state checksums, disconnect handling and lobby→match→results→lobby transitions. This is the point at which Vita-to-Vita becomes actually playable.

### Following 0.7X step — Online transport and Internet server browser

Reuse the lobby/game protocol over an Internet-capable discovery/connection layer. Add public/private session metadata, refresh/filter/join and robust NAT/relay/error handling as needed.

### Later 0.7X step — PC-compatible network play

Reverse engineer and reproduce LF2 2.00a's original networking closely enough for Vita-to-PC interoperability, with explicit cross-platform desync tests.

## Later milestone — Trophies

Add trophy definitions and unlock tracking only after gameplay/network behavior is stable. **LF2.NET / cheat-mode sessions are trophy-ineligible** and must never unlock achievements.
