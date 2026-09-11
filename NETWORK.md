# Network Play — 0.7X multiplayer series

Status in **0.70**: the first Vita-to-Vita AdHoc transport/lobby milestone is active. Online discovery and synchronized gameplay simulation are intentionally not enabled yet.

## 0.70 user flow

`Network Play` now opens a transport selector:

1. **Online** — reserved for the later Internet/server-browser transport.
2. **AdHoc** — active in 0.70.

AdHoc opens a real nearby-session browser backed by Vita/PSP-compatible `SceNetAdhocMatching`.

The browser shows:

- **Host** — Vita system username plus a fixed PlayStation-avatar cell. The protocol carries a stable local profile/avatar fingerprint; if no actual cached thumbnail is available offline the UI uses a deterministic PS-profile fallback.
- **Players** — current / maximum players (2–8).
- **Status** — waiting, starting, in game, results.
- **Mode** — VS, Stage, Battle, 1v1 or 2v2.
- **Connection** — five bars derived from a transient AdHoc matching round-trip probe and displayed with the measured milliseconds.
- **Create new AdHoc game** below the discovered sessions.

## Lobby synchronization

A joined Vita receives the host's complete lobby state. The lobby synchronizes:

- player join/leave and a unique peer identity (not username-only);
- each player's selected LF2 character;
- ready state;
- host game mode;
- maximum players;
- Stage/background selection;
- difficulty;
- lobby status/revision;
- periodic client-to-host RTT ping.

Host setting changes are broadcast immediately, so clients see the same options without leaving the lobby. Character and ready changes are sent back to the host and then rebroadcast as authoritative lobby state.

## AdHoc transport design

The host uses AdHoc Matching **parent** mode. Browsers/joiners use **child** mode. Host metadata is advertised through the matching HELLO payload, so a browser can populate rows before becoming a player.

Connection quality is measured with a short matching handshake. Probe connections are tagged separately and are never inserted into the lobby's player list.

After joining, lightweight PING/PONG packets provide a continuing RTT for the lobby display.

The AdHoc product ID is the LF2 Vita title ID (`LF2V00001`), and the application protocol has an independent version field so incompatible future lobby/gameplay revisions can be rejected cleanly.

## Gameplay lockstep — next 0.7X step

0.70 deliberately stops at the synchronized lobby boundary. Pressing START as host verifies that at least two players are present and all are ready, changes the advertised status to `Starting`, but **does not launch a network fight yet**.

The next step is deterministic Vita-to-Vita gameplay:

- fixed 30 Hz input frames;
- host-defined start tick and RNG seed;
- per-tick player input packets with a small prediction/rollback-or-delay window;
- deterministic object IDs/spawn order, AI and Stage state;
- periodic state checksums/desync logging;
- pause/disconnect/rejoin policy;
- result/rematch transition back into the same lobby.

This separation is intentional: discovery/lobby failures must be diagnosable independently from simulation desyncs.

## Online and PC compatibility

**Online** is the next transport after Vita AdHoc gameplay is stable. It can reuse the same browser/lobby model while replacing nearby HELLO discovery with an Internet directory/relay path.

**PC-compatible LF2 2.00a networking** remains a separate later milestone. It requires reproducing the original PC framing/synchronization behavior rather than simply reusing the Vita-native protocol.

## Diagnostics

Network activity is written to `ux0:data/LF2V00001/lf2.log`, including:

- AdHoc initialization and local peer identity;
- scan start and host discovery/revision changes;
- browser probe RTT and bar rating;
- host creation;
- join request / accept / peer slot;
- lobby revision and host settings;
- character/ready updates;
- status transitions;
- disconnect/timeouts;
- clean AdHoc shutdown.

## Compatibility gate and browser filters

AdHoc discovery now advertises both compatibility versions in every HELLO packet: stock game data **LF2 2.00a** and port build **LF2Vita 0.70**. The browser rejects a host before adding it to the session list unless both strings exactly match the local build. Join requests repeat both version fields and the host rejects mismatched clients as a second safety check.

The browser header permanently shows the local game and Vita versions. The CS-1.6-style filter strip supports game mode, lobby status, free slots and maximum measured ping. Version compatibility is deliberately not an optional filter: incompatible sessions are never exposed to the UI. SELECT enters/leaves filter editing; Up/Down selects a filter and Left/Right changes its value.
