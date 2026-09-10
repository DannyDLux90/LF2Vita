# Little Fighter 2 Vita v0.61

Native PS Vita work-in-progress port using Little Fighter 2 2.00a data supplied locally by the user.

## v0.61 changes

- New high-contrast embedded bitmap UI font for the main menu, VS selection cards and combat HUD.
- Native 1.0x integer-position sprite rendering to eliminate non-integer nearest-neighbour shimmer.
- Player facing is controlled by player input; it is no longer overwritten by nearest-enemy auto-facing.
- One-pixel mirror-anchor correction for sprite and collision coordinates.
- KO/death `wait`/`next` animation chains now continue correctly.
- Sprite sheets use cumulative LF2 picture numbering (`row * col`); the numbers written in `file(a-b)` are not used for runtime indexing.
- Fresh diagnostic log per launch: current run is `lf2.log`, previous run becomes `lf2_prev.log`.
- P1 non-movement frame transitions are logged as `ANIM` records.
- Audio engine is unchanged from the good v0.50/v0.60 hardware-tested path.

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

New renderer diagnostics use the `RENDER` and `ANIM` tags. Texture load records include the DAT layout, cell size and expected bitmap dimensions.

## Current limitations

This is a native reimplementation, not the original Windows executable. VS combat remains the playable focus. Stage, Championship, Battle, Demo and Playback still need their complete original rules. Projectiles, weapons/pickups, catching, transformations and some character-specific states/effects remain incomplete. Backgrounds are still static Vita compositions rather than the complete original multi-layer scrolling implementation.
