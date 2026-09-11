# LF2Vita

Native PS Vita reimplementation of **Little Fighter 2 2.00a**, built with VitaSDK and libvita2d.

> The public repository contains the port source code and reproducible workspace tooling. Original Little Fighter 2 game data, music and derived game artwork are not redistributed in Git. Supply your own LF2 2.00a files to make the local game pack and title BGM.

Current hardware-test version: **0.70**. Current milestone: **Vita-to-Vita multiplayer — AdHoc discovery and synchronized lobby**.

## Current state

- Vita LiveArea start plus LF2.NET/CRAZY launch option and native Vita manual.
- Packed/CRC checked 2.00a game data with fast VitaShell installation layout.
- Native DAT parser, 30 Hz gameplay simulation and vita2d renderer.
- VS, Stage, 1-on-1 Championship, 2-on-2 Championship, Battle and Demo mode foundations.
- Native projectile/opoint runtime for stock 2.00a arrows, balls and other attack objects, including recursive child opoints.
- 48 kHz effect mixer plus looping original title/menu music prepared from the user's `bgm/main.wma`.
- Contact hit sounds, hit flashes and effect-dependent blood/fire/ice feedback.
- LF2-style eight-slot in-fight status board using the original small character portraits.
- Hit `dvx`/`dvy` and `fall` reactions now drive knockback, launch/falling and landing states.
- CPU fighters can choose and execute their own DAT-defined specials at all difficulties.
- `Network Play` now has an **Online / AdHoc** transport selector. 0.70 enables real Vita AdHoc discovery, measured-ping server browser, host/join and synchronized character/ready/game-setting lobby state; network combat lockstep is the next 0.7X step.
- Stock pickup runtime for stick, hoe, knife, baseball, milk, beer, boomerang, stone and wooden box; VS/Battle drops and Stage Mode-authored consumables are supported.
- Rudolf caught-target transformation, Louis→LouisEX and Firen+Freeze→Firzen have first-pass native state handling.
- Detailed current-session log in `ux0:data/LF2V00001/lf2.log`.

See [VERSION_0.70.md](VERSION_0.70.md) and [NETWORK.md](NETWORK.md) for the current multiplayer work, [PC_FIDELITY_AUDIT.md](PC_FIDELITY_AUDIT.md) for remaining gameplay-fidelity gaps, and [ROADMAP.md](ROADMAP.md) for milestone order.

## Build

Requirements: VitaSDK, libvita2d, libpng, libjpeg-turbo, freetype, zlib, Python 3; `ffmpeg` is needed only to prepare the original menu music.

```sh
export VITASDK=/path/to/vitasdk
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
python3 tools/prepare_music.py /path/to/LittleFighter.zip assets/main_bgm.wav
cmake -S . -B build
cmake --build build -j
```

`gamepack/game.lf2pak` and `assets/main_bgm.wav` are local generated inputs and intentionally ignored by Git. The executable still builds without the music file, but title/menu music will then be disabled at runtime.

See [README_VITA.md](README_VITA.md) for controls, diagnostics and remaining engine gaps.

## 0.69 PC-fidelity combat pass

v0.69 adds PC-style weapon throw frame/release behavior, parsed `weapon_strength_list`, LF2-style `arest`/per-target `vrest`, first-pass `cpoint` catch/throw and thrown-body ITR kind 4, heavy-weapon movement, immediate dead Stage-slot reuse, and CPU retreat from a state-14 lying opponent. See `VERSION_0.69.md` and `PC_FIDELITY_AUDIT.md`.


## 0.69 fix3

The current hardware-test correction pass fixes item-throw state ownership, scrolling-Stage projectile lifetime, broad stock special-command/runtime coverage, Stage corpse persistence, and Stage party setup. Stage Mode supports one Vita-controlled P1 plus up to seven selectable Team-1 COM companions, with a separate seven-actor enemy pool. `SPECIAL_ATTACK_AUDIT_FIX3.txt` records the static stock-special coverage audit.


## 0.70 Vita-to-Vita AdHoc milestone

0.70 starts the 0.7X multiplayer series. The Vita AdHoc path now performs real nearby host discovery and exposes a server browser with host profile badge, player count, lobby status, game mode and five-bar RTT quality. Hosts can create 2–8-player lobbies; joined players see all selected LF2 characters and ready states, while mode, max players, Stage and difficulty changes are broadcast live by the host.

This first 0.70 build intentionally stops before network gameplay: the deterministic 30 Hz fight lockstep is the next 0.7X step. Separating lobby transport from simulation synchronization keeps the first two-Vita hardware test focused and diagnosable.
