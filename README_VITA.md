# Little Fighter 2 Vita v0.69

Native PS Vita work-in-progress port using Little Fighter 2 2.00a data supplied locally by the user.

## v0.69 hardware-test focus

v0.69 is the active PC-fidelity milestone. Item pickup remains on **L**; **L+□** is an explicit Vita throw command that still runs the stock LF2 throw frames. Milk/Beer now use their authored drink-time units instead of `weapon_hp`, hidden-character shortcuts resolve the actual DAT command families, and Stage slot reuse preserves a persistent corpse snapshot so the next wave can start immediately without visually deleting defeated enemies.

- Original title/menu music now loops through the existing 48 kHz Vita mixer. The local build converts `bgm/main.wma` to PCM WAV ahead of time; no WMA decoder runs on the Vita.
- Melee and projectile contact now trigger LF2-style impact sounds instead of relying only on attack-frame sounds.
- Hit feedback includes a short fighter flash plus normal hit sparks and effect-specific blood, fire or ice particles.
- The in-fight HUD is four columns by two rows, matching the original LF2 layout: small portrait, red HP bar and blue MP bar for up to eight active fighters.
- `itr` knockback now uses `dvx`, `dvy` and accumulated `fall`: light hurt, heavier hurt, dance-of-pain and airborne/falling reactions are separated, with landing frames after a launch.
- CPU fighters now choose valid, affordable DAT-defined special commands on Easy, Normal, Difficult and CRAZY rather than using only basic attacks on lower difficulties.
- Main menu now contains `Network Play`, with separate Vita Ad-Hoc Host/Game Join and PC LF2 2.00a Host/Game Join targets. **This build does not open a network transport yet.**

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

The combat engine is still a native reimplementation rather than the Windows LF2 binary. v0.69 adds first-pass native catch/throw, weapon throwing and rest timing, but exact hitlag/defend-break/armor, dark-red recoverable HP, rare ITR/cpoint cases, CPU item strategy and per-weapon durability/drop rules still need fidelity work. The complete Stage reserve/conditional-directive set and full original `bg.dat` layer/parallax renderer remain incomplete. Rudolf/Louis/Firzen transformations still need hardware edge-case testing. Playback Recording lacks the deterministic `.lfr` event-stream decoder. Network Play remains a menu/protocol scaffold in 0.69; Vita Ad-Hoc discovery/transport and the exact original PC lockstep protocol are not implemented.
