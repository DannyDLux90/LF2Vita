# Little Fighter 2 Vita v0.63

Native PS Vita work-in-progress port using Little Fighter 2 2.00a data supplied locally by the user.

## v0.63 changes

- Adds native LF2 `opoint` spawning from the original character DAT files.
- Renders and simulates stock arrows, balls, chase balls, wind/blast and flame/column objects from their own 2.00a DAT/BMP definitions.
- Projectile `itr` damage now changes HP and applies authored knockback/effect values.
- Henry's five-arrow command spawns five separately visible arrows.
- Projectile spawning/hits are logged with `OBJECT` entries for hardware diagnosis.
- 1-on-1 Championship now shows the round winner, defeated fighter and next opponent before the next round.
- 2-on-2 Championship shows the winning team, defeated team and next team.
- The stable v0.61 renderer/font and v0.50 audio path are otherwise intentionally unchanged.

## Vita controls

- Left stick / D-pad: movement and menu navigation.
- Strong analog-stick deflection: run.
- □: attack / confirm in classic-style menus.
- ×: jump / alternate confirm.
- ○: defend / combo modifier / back.
- L: alternate defend/combo modifier.
- △ or R: primary special shortcut.
- START: pause.
- SELECT in the main menu: in-game guide.

## LiveArea

- Main gate: normal start.
- `LF2.NET` tile: hidden roster and CRAZY! difficulty enabled.
- Retail-style system manual: `sce_sys/manual/001.png` through `009.png`.

## Diagnostics

- `ux0:data/LF2V00001/lf2.log` (current launch only)
- `ux0:data/LF2V00001/lf2_prev.log` (previous launch)
- `ux0:data/LF2V00001/last_state.txt`

Mode transitions are logged with `stage_mode`, `stage:phase`, `championship1`, `championship2`, `battle` and `demo` tags. Renderer/action diagnostics continue to use `RENDER` and `ANIM`.

## Current limitations

The combat engine is still being rebuilt from the original 2.00a data rather than executing the Windows binary. Stage weapons and consumables, captive criminals, boss/soldier reserve spawning, multi-human local input, complete background scrolling/layers, Rudolf clone/transform semantics and held-weapon attachment (such as Freeze ice sword) remain incomplete. The Vita runtime currently supports eight active fighters; large Stage waves are therefore streamed in batches. Playback Recording can browse the stock files but does not yet decode their deterministic input stream.
