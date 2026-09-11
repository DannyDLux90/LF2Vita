# LF2Vita 0.70

0.70 begins the **0.7X multiplayer series**. The gameplay base is v0.69 fix3; this release deliberately focuses on Vita-to-Vita AdHoc discovery and lobby synchronization before combat lockstep.

## Loader UI

- Restores a clearly visible progress bar without restoring the large translucent dialog panel.
- Uses a 750×24 px progress bar with a bright border.
- Shows the current resource class: background, sound, character/object data, recording, graphics or generic game file.
- Shows the exact packed path currently being loaded/verified plus entry count and percent.

## Network Play selector

- Adds **Online / AdHoc / Back**.
- `Online` is reserved for the later Internet transport.
- `AdHoc` is active in 0.70.

## Real Vita AdHoc browser

The browser is backed by `SceNetAdhocMatching`, using parent mode for hosts and child mode for browsers/joiners.

Columns:

- Host — Vita system username plus PlayStation profile/avatar cell.
- Players — current / maximum.
- Status — Waiting / Starting / In Game / Results.
- Mode — VS / Stage / Battle / 1v1 / 2v2.
- Connection — measured matching-handshake RTT, shown as milliseconds and five green→red bars.

Below the table is **Create new AdHoc game**.

## Host and join lobby

- 2–8-player lobby.
- Unique peer IDs avoid username collisions.
- Each player has an LF2 character selection and ready state.
- Clients receive complete authoritative lobby state from the host.
- Host changes to mode, max players, Stage and difficulty are broadcast live.
- Character/ready changes are sent to the host and rebroadcast.
- Lobby state is revisioned.
- Client→host PING/PONG keeps a live RTT after join.
- Probe handshakes used by the browser are tagged and never counted as players.
- Sessions that are starting/in game do not accept new browser joins.
- Join timeout and connection loss are surfaced in the UI.
- Network resources are terminated cleanly on application shutdown.

## Avatar behavior

`ur0:user/00/np/myprofile.dat` is fingerprinted for the local PlayStation profile/avatar identity. Because nearby AdHoc must work without Internet and that file does not guarantee locally decoded image pixels, 0.70 renders a deterministic PS-profile fallback when no thumbnail is cached. Remote thumbnail transfer/cache is intentionally kept separate from the first transport test.

## Not yet in 0.70

Pressing START as host currently validates that at least two players are present and everyone is ready, then exercises the lobby `Starting` status. It does **not** launch a network match.

Next 0.7X work:

1. deterministic 30 Hz Vita-to-Vita input lockstep and checksums;
2. match start/results/rematch transitions;
3. Online transport and Internet server directory/browser;
4. PC LF2 2.00a-compatible networking;
5. trophies later, never in cheat/LF2.NET sessions.

## Hardware test

The first two-Vita test should verify:

1. host appears/disappears in the second Vita's browser;
2. displayed ping/bars are stable and refresh works;
3. joining adds exactly one player (browser probes must never appear as players);
4. both Vitas see the same character and ready changes;
5. host mode/player-limit/Stage/difficulty changes update immediately on the client;
6. leave/rejoin works repeatedly without restarting the application;
7. `lf2.log` contains `NET` discovery, probe, join, lobby revision and shutdown diagnostics.

## Server browser compatibility / filters

- Discovery HELLO metadata includes `LF2 2.00a` and `LF2Vita 0.70`.
- Only exact-version matches are admitted to the browser list; the join request repeats the versions and is rejected by the host when mismatched.
- The browser header shows both local versions at all times.
- Added a Counter-Strike-1.6-inspired filter strip for game mode, status, free slots and maximum ping. Version matching is mandatory and cannot be disabled.
- SELECT toggles filter editing; Up/Down selects a filter and Left/Right changes it.
