# Little Fighter 2 Vita v0.70

Native PS Vita work-in-progress port using Little Fighter 2 2.00a data supplied locally by the user.

## v0.70 hardware-test focus

v0.70 begins the multiplayer series on top of the v0.69 fix3 gameplay base. `Network Play` now offers **Online** and **AdHoc**; AdHoc is active and provides real nearby discovery, host creation, measured connection quality and a synchronized lobby. Network fights are not launched yet: the next 0.7X step connects the existing 30 Hz simulation to deterministic Vita-to-Vita input lockstep.

- Original title/menu music now loops through the existing 48 kHz Vita mixer. The local build converts `bgm/main.wma` to PCM WAV ahead of time; no WMA decoder runs on the Vita.
- Melee and projectile contact now trigger LF2-style impact sounds instead of relying only on attack-frame sounds.
- Hit feedback includes a short fighter flash plus normal hit sparks and effect-specific blood, fire or ice particles.
- The in-fight HUD is four columns by two rows, matching the original LF2 layout: small portrait, red HP bar and blue MP bar for up to eight active fighters.
- `itr` knockback now uses `dvx`, `dvy` and accumulated `fall`: light hurt, heavier hurt, dance-of-pain and airborne/falling reactions are separated, with landing frames after a launch.
- CPU fighters now choose valid, affordable DAT-defined special commands on Easy, Normal, Difficult and CRAZY rather than using only basic attacks on lower difficulties.
- `Network Play` now opens **Online / AdHoc**. AdHoc uses the real Vita matching transport for discovery, host/join and synchronized lobby state; Online remains reserved for a later 0.7X step.

- Stage Mode now advances directly after a cleared wave, and enemy/item textures needed by later waves are preloaded before gameplay. Enemy slots reuse shared read-only fighter assets, eliminating synchronous DAT/texture uploads from the wave transition path. Authored milk/beer entries from `stage.dat` are preserved.
- Stock pickups are active: stick, hoe, knife, baseball, milk, beer, boomerang, stone and wooden box. **L** picks up a nearby grounded item. Square remains attack/use; hold Square with milk/beer to drink. Light-weapon attack poses use the original fighter/weapon `wpoint` + `weaponact` pairing, and loose items play their authored landing sequence before resting.
- Rudolf caught-target transform, Louis -> LouisEX and Firen + Freeze -> Firzen have native first-pass handling. Static special and item audits are included in the workspace.

## Vita controls

- Left stick / D-pad: movement and menu navigation.
- Strong analog-stick deflection: run.
- □: attack / use a held item / confirm in classic-style menus.
- L: pick up a nearby grounded item.
- L + □ while holding an item: explicit throw using the stock LF2 throw-frame chain.
- Hold □ with milk/beer: drink until released, cancelled with ○, or empty.
- ×: jump / alternate confirm.
- ○: defend / combo modifier / back; cancels an active drink.
- △: attack-special shortcut (`hit_Fa`/`hit_Ua`/`hit_Da`, direction-aware).
- R: jump-special shortcut (`hit_Fj`/`hit_Uj`/`hit_Dj`, direction-aware).
- START: pause.
- SELECT in the main menu: in-game guide.

## LiveArea

- Main gate: normal start.
- `LF2.NET` tile: hidden roster and CRAZY! difficulty enabled.
- Retail-style system manual: `sce_sys/manual/001.png` through `009.png`.

## Diagnostics

- `ux0:data/LF2V00001/lf2.log` — current launch only.
- `ux0:data/LF2V00001/lf2_prev.log` — previous launch.
- `ux0:data/LF2V00001/last_state.txt` — last synchronous stage marker.

New 0.69 diagnostics include `HIT` reaction data, `AI ... skill frame=...`, title-music readiness/start/stop, plus the existing `OBJECT` projectile cache/spawn/hit records.

## Current limitations

The combat engine is still a native reimplementation rather than the Windows LF2 binary. v0.69 adds first-pass native catch/throw, weapon throwing and rest timing, but exact hitlag/defend-break/armor, dark-red recoverable HP, rare ITR/cpoint cases, CPU item strategy and per-weapon durability/drop rules still need fidelity work. The complete Stage reserve/conditional-directive set and full original `bg.dat` layer/parallax renderer remain incomplete. Rudolf/Louis/Firzen transformations still need hardware edge-case testing. Playback Recording lacks the deterministic `.lfr` event-stream decoder. Vita AdHoc discovery/host/join and lobby synchronization are implemented in 0.70, but deterministic network combat is not connected yet. Internet Online transport and exact PC LF2 2.00a networking remain later 0.7X work.


### v0.69 fix3 test focus

fix3 locks authored item throw chains through their actual release frame, removes the obsolete fixed-arena projectile cutoff from Stage Mode, audits every stock fighter special-command graph, implements additional type-3/ITR engine rules, and enables **0–7 selectable COM companions in Stage Mode**. The companion slots are Team 1 and persist across authored waves; Stage enemies use a separate seven-actor pool so a full P1+7 COM party still has seven live enemy slots. The startup integrity-check UI is intentionally minimal: no translucent frame/panel, only a thin progress line.


### v0.70 AdHoc test focus

Use two Vitas with the same 0.70 build. On one Vita create an AdHoc game; on the other open the browser. Verify the host row shows the system username/profile badge, player count, status, mode and a measured millisecond value with five-bar quality. Join and verify both screens agree on player slots, LF2 characters, ready states, mode, maximum players, Stage and difficulty while the host changes them.

The PlayStation-avatar cell is protocol-ready but intentionally has an offline fallback in this first build: `myprofile.dat` supplies a stable local profile/avatar fingerprint, while an actual remote image requires a cached/downloaded thumbnail and must not make nearby AdHoc depend on Internet access.

The startup data check is intentionally more visible than fix3: a wide progress bar plus resource category and the concrete packed path being loaded/verified.

### 0.70 AdHoc browser compatibility

The nearby-session browser only lists hosts advertising the same stock data version (**LF2 2.00a**) and the same Vita build (**0.70**). Both are shown in the browser header. SELECT opens a CS-1.6-style filter strip for mode, status, free slots and maximum ping; version compatibility is always enforced.
