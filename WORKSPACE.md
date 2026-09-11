# LF2Vita development workspace

This repository tracks the current source/build workspace for the PS Vita port. Hardware-test version: **0.70**.

## Directory layout

- `src/` — Vita runtime, renderer, DAT/stage parsers, fighter/object/projectile/item world, transformations, combat physics, AI, audio and diagnostics.
- `tools/` — helpers to build `game.lf2pak`, prepare the original title music, and run static special/item coverage audits against a locally owned LF2 2.00a copy.
- `gamepack/` — local generated game pack; `game.lf2pak` is intentionally ignored.
- `assets/` — Vita UI/stage artwork in the local hardware-test snapshot. `main_bgm.wav` is locally generated and ignored.
- `sce_sys/` — LiveArea and native Vita manual resources.

## Reproducing a local test workspace

```sh
export VITASDK=/path/to/vitasdk
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
python3 tools/prepare_music.py /path/to/LittleFighter.zip assets/main_bgm.wav
cmake -S . -B build
cmake --build build -j
```

Supply the non-redistributable Vita artwork described by the asset README(s), or copy it from the matching private/local workspace snapshot.

## Publication policy

Every hardware-test increment is synchronized to the public GitHub repository with its current **source and reproducible workspace text/tooling**. Original LF2 2.00a data, converted title music, and copyrighted game artwork are not committed publicly. The downloadable local workspace snapshot can contain the user's supplied/generated resources for hardware testing.
