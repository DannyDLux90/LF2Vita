# LF2Vita

Native PS Vita reimplementation of **Little Fighter 2 2.00a**, built with VitaSDK and libvita2d.

> This repository contains the port source code, Vita workspace, UI assets and build tooling. Original Little Fighter 2 game data is **not redistributed in Git**. Provide your own `LittleFighter.zip` and generate `gamepack/game.lf2pak` locally.

Current development version: **0.61**.

## What works

- Vita LiveArea start and LF2.NET cheat launch option
- integrated file verification
- original 2.00a DAT decoding/parsing for character data
- original character sprite sheets rendered natively with vita2d
- VS setup, teams, up to seven CPU fighters and stage selection
- Vita controls, pause/rematch/menu return
- native 48 kHz sound mixer for the supplied WAV effects
- native Vita manual pages under `sce_sys/manual`
- detailed diagnostics in `ux0:data/LF2V00001/lf2.log`

## 0.61 focused fixes

This point release deliberately changes fewer things than 0.60. The goal is to isolate the remaining Vita rendering/animation problems and make hardware feedback easier to interpret.

- Critical menu/HUD text now uses an embedded pre-rasterized, outlined bitmap atlas instead of tiny fractional PGF glyphs. No font file is redistributed.
- Character sprites render at native 1.0x pixel scale on integer screen coordinates to remove nearest-neighbour shimmer caused by the old 1.28x scale.
- The human fighter is no longer force-turned toward the nearest CPU after every logic tick.
- Mirrored `centerx` and mirrored collision-region anchoring have a one-pixel correction.
- Knock-out/death frame chains continue to animate instead of freezing on the first KO frame.
- LF2 sprite-sheet picture numbers are assigned cumulatively from each sheet's `row * col`; the textual `file(a-b)` numbers are not used by LF2 for runtime numbering.
- `lf2.log` is now current-session-only and the previous run is rotated to `lf2_prev.log`, so logs from older VPKs cannot be confused with the current build.
- P1 action frame transitions are logged with `ANIM` entries.

## Build

Requirements: VitaSDK, libvita2d, libpng, libjpeg-turbo, freetype and zlib.

```sh
export VITASDK=/path/to/vitasdk
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
cmake -S . -B build
cmake --build build -j
```

The local game pack contains the user's legally obtained 2.00a files and is intentionally ignored by Git.

See [README_VITA.md](README_VITA.md) for Vita-specific controls, current limitations and diagnostics.
