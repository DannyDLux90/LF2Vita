# LF2Vita Round 4 WIP handoff — 2026-09-13

## Goal
Make LF2Vita reproduce PC Little Fighter 2 gameplay as closely as possible in both singleplayer and multiplayer. Gameplay rules must be shared between local play, Vita↔Vita AdHoc and PC lockstep; only input/network transport may differ.

## GitHub baseline before this handoff
- Repository: `DannyDLux90/LF2Vita`
- Branch: `pc-compat-wip`
- Previous head: `150a36272318a1e17495868ad9e315609f94efc1`
- Previous tree: `383079251f5fb41df8e5f009b860178d38e41305`
- Previous commit message: `fix: render authored backgrounds and weapon item hits`

The Round-4 files in this snapshot are WIP work after that commit. A new GitHub commit created from this handoff should be treated as a continuation checkpoint, not a finished release.

## Current validation status
- `tools/audit_fix3.py --source current_concat.c gamepack/game.lf2pak`: PASS on the current source after regenerating `current_concat.c`.
  - 23 stock fighters covered.
  - 71 neutral stock special-command roots have a runtime effect path.
  - authored throw-release checks pass.
- `tools/test_adhoc_protocol.py`: PASS (`adhoc protocol v6 stress: OK`, avg 12.48 ms, worst 129 ms, start worst 1922 ms).
- Latest source could NOT be recompiled in this final handoff session because the temporary VitaSDK mount (`/mnt/data/vitasdk`) was no longer available. Earlier Round-4 checkpoints compiled to SELF, but edits made after those checkpoints must be compiled again in the next chat before calling the tip build-green.

## Important gameplay work already present in this snapshot
- Authored `bg.dat` renderer and continuous-stage background section origin handling from Round 3.
- Item/weapon durability and pickup work, including Louis transformation armour OID 217/218 as recoverable heavy weapons.
- Henry wind and general weapon/object hit handling reworked so ground drinks/weapons use PC-style body/hit paths instead of a Henry-specific hack.
- Fighter resource model now contains `dark_hp`, `max_hp`, HP/MP regen counters, delayed spell heal, Jan heal timers and caught throw-injury state.
- Normal damage and special paths are being routed through common Red/Dark-HP accounting rather than independent HP-only subtraction.
- `cpoint throwinjury` is stored and applied on landing instead of immediately on throw.
- Stage runtime now contains player/ally Dark-HP input/output fields; `game_23.inc` and `game_24.inc` read/write them.
- Multiplayer stock-state digest (`game_stock_diag.inc`) now hashes Dark HP, regen/heal timers, throw injury, input direction/attack state and existing fighter/object simulation state.
- Natural HP/MP regeneration and Milk/Beer healing have been moved toward PC timing; the old duplicate MP-regeneration path was removed during Round-4 reconstruction.
- Running/jump/dash attack-state transitions and heavy-weapon restrictions were moved toward the PC common state machine.
- Falling/burning landing physics were investigated against PC references; the target PC constants are `sqrt(vx^2+vy^2) > 13.4` or `vy > 11`, bounce `vy=-4.25`, with lookup-table X/Z absorption.
- General airborne mechanics were identified as needing PC gravity 1.7 without Vita-style per-TU X/Z air damping; current snapshot contains the in-progress implementation.
- Gameplay RNG work routes several simulation decisions through `lf2_rng_mod()`; remaining raw `rand()` calls in active `fix3` should be audited by whether they affect simulation (menu/visual-only calls may remain outside the deterministic state).

## Henry / drinks conclusion
Do not restore the old assumption that Milk/Beer lying on the ground are immune to normal attacks. The PC reverse-engineering path allows weapons/drinks to be attacked even on the same team, and Henry wind is a normal damaging type-3 object with a kind-0 ITR. The intended behavior is implemented through the generic weapon/drink victim rules, not by special-casing Henry.

## Known next work / verify first in the next chat
1. **Compile this exact tip first.** The latest edits have only static-audit coverage because VitaSDK disappeared before this handoff.
2. Inspect the current pickup/vrest logic around `game_14.inc` before adding more collision changes; an earlier note flagged the pickup/vrest indexing path for cleanup.
3. Re-run all resource formula tests (normal damage, block/armor, caught throw injury, natural regen, John/Sorcerer heal, Jan heal, Milk, Beer) after a successful build.
4. Continue PC-fidelity verification for Frozen break/landing, John shield (`itr kind:9`), projectile reflect/state 3000/3006 and held-weapon `kind:5`.
5. Audit active `fix3` gameplay random calls and ensure simulation-affecting randomness uses the shared LF2 RNG in multiplayer.
6. Hardware-test Stage 2 background transitions and vertical/depth alignment; horizontal local-section camera origin/parallax scaling is already implemented, but vertical projection may still need correction.
7. Hardware-test Henry wind vs Milk/Beer, Box/Stone, Louis armour pickup, falling/burning bounce, Dark-HP regeneration and multiplayer desync logging.

## Files changed from GitHub commit 150a3627
- `src/background.c`
- `src/game.h`
- `src/fix3/game_stock_armor.inc`
- `src/fix3/game_10.inc`
- `src/fix3/game_17.inc`
- `src/fix3/game_03.inc`
- `src/fix3/game_14.inc`
- `src/fix3/game_08.inc`
- `src/fix3/game_11.inc`
- `src/fix3/game_04.inc`
- `src/fix3/game_13.inc`
- `src/fix3/game_18.inc`
- `src/fix3/game_15.inc`
- `src/fix3/game_23.inc`
- `src/fix3/game_06.inc`
- `src/fix3/game_stock_hitlag.inc`
- `src/fix3/game_12.inc`
- `src/fix3/game_02_prefix.inc`
- `src/fix3/game_stock_diag.inc`
- `src/fix3/game_05.inc`
- `src/fix3/game_20.inc`
- `src/fix3/game_24.inc`
- `src/fix3/game_21.inc`
- `src/fix3/game_07.inc`
- `src/fix3/game_09.inc`
- `src/fix3/game_19.inc`
- `src/fix3/game_01.inc`
- `src/fix3/main_06.inc`
- `src/fix3/game_22.inc`

## Reconstructing a build workspace
The source ZIP is intended to be the canonical code snapshot. The workspace ZIP additionally includes `gamepack/game.lf2pak`, local fallback assets, regenerated `current_concat.c`, audit output and this handoff. Do not commit `gamepack/game.lf2pak` to GitHub; it is ignored by the repository.

Typical build commands once VitaSDK is available again:

```sh
export VITASDK=/mnt/data/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build
cmake --build build -j2
```

Then rerun:

```sh
cat src/game.c $(find src/fix3 -maxdepth 1 -type f -name '*.inc' | sort) > current_concat.c
python3 tools/audit_fix3.py --source current_concat.c gamepack/game.lf2pak
python3 tools/test_adhoc_protocol.py
```
