# LF2Vita 0.70 — multiplayer handoff / recovery checkpoint

## Repository and baseline

- Repository: `DannyDLux90/LF2Vita`
- v0.69 fix3 final baseline commit: `a615202874694421b93f5f6b0555dbaa6c2913fd`
- Package version for this workspace: `00.70`
- Current milestone: 0.7X Vita multiplayer.
- First transport: Vita-to-Vita **AdHoc**.
- Online is visible in the selector but intentionally not active yet.
- PC-compatible networking and trophies are later milestones; cheat/LF2.NET sessions must never unlock trophies.

## User-requested 0.70 UX

Network Play -> Online / AdHoc.

AdHoc server browser columns:
- Host: system player name + PlayStation avatar/profile cell
- players current/max
- status
- game mode
- connection: 5 bars green→red based on measured ping
- Create new AdHoc game below the list.

Joined players must see their own and other LF2 characters and all host settings/changes.

## Implemented in this checkpoint

Files:
- `src/net70/adhoc_net.h`
- `src/net70/adhoc_net.c`
- `src/fix3/main_01.inc` loader UI
- `src/fix3/main_09.inc` network browser/lobby UI
- `src/fix3/main_10.inc` 0.70 menu/shutdown
- `CMakeLists.txt` networking libraries and 00.70 SFO
- `NETWORK.md`, `ROADMAP.md`, `VERSION_0.70.md`

Transport:
- `SCE_SYSMODULE_NET`
- `SCE_SYSMODULE_PSPNET_ADHOC`
- `SCE_SYSMODULE_NET_ADHOC_MATCHING`
- title/product ID `LF2V00001`
- host Matching parent, browser/joiner child
- versioned HELLO/session metadata
- transient probe joins for browser RTT; probe peers are never lobby players
- explicit selected-host address for client packets
- unique generated peer ID in addition to username
- full lobby-state packet plus player-update and ping/pong packets
- host-authoritative lobby revision
- clean shutdown and detailed NET logs

Lobby:
- 2–8 slots
- LF2 character per player
- ready per player
- synchronized mode, max players, Stage, difficulty and status
- live client ping to host
- join/leave/timeout handling
- host START currently tests ready/status only; network match launch remains disabled

Avatar:
- Vita username comes from system parameters.
- `ur0:user/00/np/myprofile.dat` is hashed for stable profile/avatar identity.
- actual remote image is not guaranteed offline; first 0.70 renders a PlayStation-profile fallback when no cached thumbnail exists. Implement thumbnail cache/transfer after transport is proven on two Vitas.

Loader:
- larger 750×24 progress bar
- no large translucent frame
- resource category + exact packed path + count/percent

## Build

```sh
export VITASDK=/mnt/data/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build-070 -DCMAKE_BUILD_TYPE=Release
cmake --build build-070 --clean-first -j2
```

Current cross-build reaches ELF -> VELF -> SELF -> VPK.

## Critical next test

Requires two real Vitas with the exact same VPK:
1. Vita A: Network Play -> AdHoc -> Create.
2. Vita B: Network Play -> AdHoc; confirm A appears.
3. Record host row, RTT and bars.
4. Join; verify probes did not inflate player count.
5. Change character/ready on each Vita.
6. Host cycles mode, max players, Stage and difficulty; client must update immediately.
7. Leave/rejoin at least twice.
8. Save `ux0:data/LF2V00001/lf2.log` from BOTH Vitas.

Do not call the gameplay multiplayer complete until deterministic combat lockstep is implemented and hardware-tested.

## Browser compatibility/filter update

- Required discovery versions: `LF2_GAME_VERSION = "2.00a"`, `LF2_VITA_VERSION = "0.70"`.
- `hello_t` and `join_t` carry both version strings. `browser_hello()` discards mismatches before session insertion; `host_join_request()` rejects mismatched joins.
- The browser shows both versions in its header and has CS-1.6-style filters: mode, status, free slots and maximum ping. Version matching is hard-wired and cannot be disabled.
- Browser filter editing: SELECT toggle; Up/Down filter selection; Left/Right value; Circle/SELECT closes.
