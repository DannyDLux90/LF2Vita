# LF2Vita

Native PS Vita reimplementation of **Little Fighter 2 2.00a**, built with VitaSDK and libvita2d.

> This repository contains the port source code, Vita workspace and build tooling. Original Little Fighter 2 game data is **not redistributed in Git**. Provide your own `LittleFighter.zip` and generate `gamepack/game.lf2pak` locally.

Current development version: **0.63**.

## What works

- Vita LiveArea start and LF2.NET cheat launch option
- integrated verification of the packed 2.00a files
- original encrypted DAT decoding/parsing and native vita2d sprite renderer
- high-contrast bitmap UI font and corrected 30 Hz LF2 animation timing
- native 48 kHz sound mixer
- VS Mode with original-style eight-slot setup, teams, backgrounds and difficulty
- **Stage Mode** driven by the stock `data/stage.dat`, including Stage 1-5, Survival, authored enemy order/HP and difficulty scaling
- **1 on 1 Championship** with a 16-fighter elimination bracket
- **2 on 2 Championship** with an 8-team elimination bracket and AI partner
- **Battle Mode** with two configurable armies (hero, follower type/count, background, difficulty)
- **Demo Mode** with random 4-vs-4 CPU teams and no human player
- Playback Recording file browser for the bundled 2.00a `.lfr` demos; deterministic replay decoding is the remaining part of this mode
- native Vita manual under `sce_sys/manual`
- detailed diagnostics in `ux0:data/LF2V00001/lf2.log`

## v0.63 - projectile specials and Championship results

v0.63 adds the missing native **opoint/object layer** used by LF2 special attacks. The 2.00a character DAT parser now reads `opoint:` blocks (`oid`, `action`, launch velocity, position and facing/count). Stock projectile/light-weapon data such as Henry's arrows, Deep/John/Firen/Freeze/Dennis/Woody/Davis energy attacks and blast/wind objects are loaded from the original DAT/BMP resources, animated at the 30 Hz LF2 simulation rate, rendered in stage depth and collide through their own `itr` regions. `injury`, knockback and fire/freeze effect flags are applied to fighters. Henry's `facing: 50` five-arrow shot creates five visible arrows.

The 1-on-1 and 2-on-2 Championship flows now pause after every played round on a dedicated result screen. It shows the winner/defeated fighter or team and, after the other bracket matches are resolved, the next opponent/team before continuing. The bracket screen remains visible before each round.

The object runtime deliberately remains bounded to 64 active spawned objects and only caches stock 2.00a attack object definitions used by the current match. Character-clone/transformation semantics and held-weapon attachment (`opoint kind: 2`, e.g. Freeze's ice sword) still require separate engine work; they are logged instead of being faked.

## Build

Requirements: VitaSDK, libvita2d, libpng, libjpeg-turbo, freetype and zlib.

```sh
export VITASDK=/path/to/vitasdk
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
cmake -S . -B build
cmake --build build -j
```

The local game pack contains the user's legally obtained 2.00a files and is intentionally ignored by Git.

See [README_VITA.md](README_VITA.md) for Vita-specific controls, limitations and diagnostics.
