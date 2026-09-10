# Little Fighter 2 Vita v0.60

Native PS Vita work-in-progress port using Little Fighter 2 2.00a data supplied locally by the user.

## v0.60 changes

- Fixed the character sprite-sheet addressing bug: LF2 `row` is the horizontal cell count and `col` is the vertical count. v0.50 had these reversed, so many `pic:` numbers sampled the wrong animation cell.
- Sprite sampling now excludes the one-pixel separator between LF2 cells, uses point filtering and half-texel UVs to prevent neighboring frames from bleeding into the image.
- Mirrored fighters now use the mirrored `centerx` anchor, preventing visible sideways jumps when a fighter turns around.
- Source rectangles are bounds-checked before GXM submission and invalid rectangles are logged as `RENDER` diagnostics rather than sampled outside a bitmap.
- Gameplay simulation now runs in fixed 30 Hz LF2 time units while presentation remains vblank-paced. Frame `wait`, walking/running animation cadence, movement, jumping and gravity now share the original data time base.
- Correct handling for the reserved LF2 velocity value `550` (stop that axis). Negative `next` values flip facing before following the frame target; `999` ends an action safely.
- All PGF UI/HUD text now gets a dark shadow and the smallest menu/HUD labels were enlarged for Vita readability.
- The v0.50 HQ 48 kHz audio mixer is retained unchanged because hardware testing confirmed the sound quality improvement.
- The native Vita manual continues to use graphical Vita button symbols and direction arrows.

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

- `ux0:data/LF2V00001/lf2.log`
- `ux0:data/LF2V00001/last_state.txt`

New renderer diagnostics use the `RENDER` and `ANIM` tags. Texture load records include the DAT layout, cell size and expected bitmap dimensions.

## Current limitations

This is a native reimplementation, not the original Windows executable. VS combat remains the playable focus. Stage, Championship, Battle, Demo and Playback still need their complete original rules. Projectiles, weapons/pickups, catching, transformations and some character-specific states/effects remain incomplete. Backgrounds are still static Vita compositions rather than the complete original multi-layer scrolling implementation.
