# LF2Vita

Native PS Vita reimplementation of **Little Fighter 2 2.00a**, built with VitaSDK and libvita2d.

Current development version: **0.60**.

This repository contains the Vita port source code, build workspace and tooling. Original Little Fighter 2 game data is not redistributed in Git. Provide your own `LittleFighter.zip` or `LittleFighter/` directory and generate `gamepack/game.lf2pak` locally.

## v0.60 highlights

- fixes LF2 sprite-sheet addressing (`row` = cells across, `col` = cells down)
- excludes the one-pixel separator between animation cells
- point filtering + half-texel UVs to avoid neighbouring-frame bleed
- correct mirrored `centerx` anchor
- bounds checks before GXM texture draws
- fixed 30 Hz LF2 simulation timing while rendering remains vblank-paced
- handles reserved LF2 velocity value `550` as an axis stop
- safer handling of negative/special `next` values
- larger PGF text with dark shadow for better Vita readability
- keeps the improved v0.50 48 kHz audio mixer

## Build

Requirements: VitaSDK, libvita2d, libpng, libjpeg-turbo, freetype and zlib.

```sh
export VITASDK=/path/to/vitasdk
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
cmake -S . -B build
cmake --build build -j
```

See `README_VITA.md` for controls, diagnostics and current limitations.
