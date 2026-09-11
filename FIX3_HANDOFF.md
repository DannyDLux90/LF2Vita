# LF2Vita v0.69 fix3 — chat handoff / recovery checkpoint

This file is intended to make a new ChatGPT conversation resumable without depending on the old chat history.

## Repository / milestone

- Repository: `DannyDLux90/LF2Vita`
- Package/application version: `00.69`
- Runtime test label: `0.69 fix3`
- Current milestone: PC LF2 2.00a gameplay fidelity / optimization.
- Next milestones, in order: native network play; server browser; PC-compatible LF2 2.00a networking; trophies. Cheat/LF2.NET sessions must never unlock trophies.
- Previous public main before fix3 finalization: `37319e789c82330386593f7b8f96485a79d7c22a` (`Fix v0.69 item use, Stage corpses, and hidden specials`).

## User hardware findings that led to fix3

1. L+Square logged `throw start`, but bottles immediately re-entered drinking before the release frame, so nothing actually left the hand.
2. Firzen specials were incomplete from the player's perspective. Disaster could start, but projectile behavior was not faithful.
3. Firzen's beam (`oid=223`) ended very early and caused no useful damage in scrolling Stage sections.
4. Stage dead enemies previously vanished on slot reuse; fix2 introduced persistent corpse snapshots and the hardware log confirmed `corpse preserved`.
5. Stage Mode did not allow configuring additional companions.
6. Startup integrity progress UI was too visually heavy.

## fix3 runtime changes

- Throw frames 45–54 own the held item until authored `wpoint` velocity releases it; no drink/swing path may replace an active throw.
- Milk/Beer remain 100/125 authored use units; Circle cancels an active drink without discarding the remainder.
- Projectiles are culled relative to `g_world_min_x/g_world_max_x`, not fixed x=1140; this fixes Stage-mode projectile disappearance far to the right.
- Type-3 `hit_a` timers transition through `hit_d` once, and `hit_j` supplies authored Z-axis steering where used.
- Hidden/EXE-side stock activators for Jan/Bat/Firzen/Julian are implemented; Jan Angel regeneration is tracked.
- ITR kind 9 projectile reflection and ITR kind 14 solid-blocking have first-pass runtime support.
- Command resolution accepts authored special links on active attack/catch/combo frames rather than only neutral frames.
- Stage corpse snapshots stay visible while fighter slots are reused immediately.
- Stage Mode party selection: P1 plus 0–7 freely selected COM companions on Team 1. Their party slots remain reserved across waves and HP/MP carry between major Stage sections. Stage enemies use a separate seven-actor pool, so a full P1+7 COM party can still fight seven simultaneous enemies and larger waves queue behind those slots.
- Startup check UI has no translucent frame/panel; only a thin two-pixel progress line and compact text remain.

## Automated coverage

`tools/audit_fix3.py` reads `game.lf2pak` directly. Latest result: 23 stock fighters, 71 neutral special-command roots, 85 unique authored special-command links total, 14 links specific to action/combo frames. Stock throw release frames and required runtime markers are checked. Latest output is `SPECIAL_ATTACK_AUDIT_FIX3.txt`.

Important: a static PASS means the command/effect graph and runtime mechanism exist. It does not replace PS Vita hardware validation of timing, visual lifetime, collision feel, damage or exact PC parity.

## Build

Toolchain used in this workspace: `/mnt/data/vitasdk`.

```sh
export VITASDK=/mnt/data/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build-fix3-party7
cmake --build build-fix3-party7 --clean-first -j2
```

A successful build must reach ELF -> VELF -> SELF -> VPK. Release VPKs are repacked with ZIP STORE (method 0) for VitaShell compatibility. Do not claim Vita hardware validation until a new device log is supplied.

## Public-repo asset policy

Do NOT push original LF2 data/music/derived artwork to the public GitHub repository. In particular exclude local `gamepack/game.lf2pak`, title music, game-derived PNGs/manual images and build directories. Public GitHub contains source/docs/tooling only. The local workspace archive may retain the user's local supplied/generated test resources.

## Key files

- `src/fix3/game_08.inc`: item throw ownership; type-3 lifetime/update pieces.
- `src/fix3/game_11.inc` / `game_13.inc`: reflection/collision support.
- `src/fix3/game_21.inc` through `game_24.inc`: Stage asset cache, corpse persistence, ally/enemy slot reservation, continuous Stage runtime.
- `src/fix3/main_01.inc`: subtle startup progress UI.
- `src/fix3/main_05.inc` / `main_06.inc`: Stage party selection/configuration.
- `SPECIAL_ATTACK_AUDIT_FIX3.txt`: whole-roster special audit output.
- `PC_FIDELITY_AUDIT.md`: known parity gaps.
- `ROADMAP.md`: milestone order.

## Hardware test priorities for the next log

- Throw Milk/Beer and ordinary/heavy weapons; confirm an `ITEM ... threw ...` log line and a visible recoverable item.
- Firzen: exercise all direction + Triangle/R shortcut variants and original defend-command inputs; verify beam/balls persist through scrolling Stage coordinates and deal damage.
- Spot-check projectile specials from several other fighters (John, Freeze, Dennis, Woody, Davis, Jan, Bat, Julian), especially reflection, ice column blocking and chase behavior.
- Stage: select 1–7 COM companions, confirm `stage:ally_ready`, verify they survive across waves and are not overwritten by enemy slot reuse.
- Confirm corpse snapshots remain visible after wave advance.
