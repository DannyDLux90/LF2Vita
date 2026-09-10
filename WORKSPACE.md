# LF2Vita development workspace

This repository tracks the current source/build workspace for the PS Vita port.

## Directory layout

- `src/` — native Vita runtime, renderer, DAT parser, audio mixer and diagnostics.
- `tools/` — helpers that build the indexed `game.lf2pak` from a locally owned LF2 2.00a archive/directory.
- `gamepack/` — local generated game pack; `game.lf2pak` is intentionally ignored.
- `sce_sys/` — LiveArea/manual layout. Binary artwork used by the packaged test build is kept in the downloadable workspace snapshot rather than redistributed from the original game in this public repository.
- `assets/` — packaged Vita UI/stage artwork in the local test workspace. See `assets/README.md`.

## Reproducing the test workspace

1. Install VitaSDK plus libvita2d, libpng, libjpeg-turbo, freetype and zlib.
2. Put a legally obtained LF2 2.00a archive somewhere outside the repository.
3. Generate the pack:

```sh
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
```

4. Supply the Vita artwork described in `assets/README.md` / `sce_sys/README.md` or copy it from a matching downloadable workspace snapshot.
5. Build:

```sh
export VITASDK=/path/to/vitasdk
cmake -S . -B build
cmake --build build -j
```

## Versioning policy

Starting with v0.60, the current source and build-workspace text files are published here whenever a new hardware-test VPK is produced. Original LF2 game data is not committed; the local `game.lf2pak` remains reproducible with the included tool.
