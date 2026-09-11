# LF2Vita PC 2.00a fidelity audit

This audit tracks native Vita runtime coverage against the LF2 2.00a DAT data present in the locally generated `game.lf2pak`. It describes implementation coverage, not binary compatibility with the Windows executable.

## Data inventory

The current local 2.00a pack contains:

| Block / object | Count |
| --- | ---: |
| DAT files | 79 |
| frames | 5,868 |
| `bdy` | 5,042 |
| `wpoint` | 4,079 |
| `itr` | 1,936 |
| `cpoint` | 521 |
| `bpoint` | 472 |
| `opoint` | 115 |

ITR kinds observed in the pack: kind 0 (897), 4 (286), 5 (245), 6 (162), 7 (127), 1 (96), 2 (49), 14 (34), 10 (15), 9 (6), 15 (5), 11 (5), 8 (5), 3 (3), and 16 (1).

## Implemented / active in 0.69

- Type-0 fighter frame state, `bdy`, ordinary ITR kind 0 damage/knockback, authored `dvx/dvy/fall`, elemental first-pass states, opoint/projectile spawning, and recursive child objects.
- `wpoint` position/weaponact/attacking/cover and throw velocities, plus parsed `weapon_strength_list` for held weapon hit strength.
- Light/heavy pickup, use, authored landing chains, PC-style throw frame groups, recoverable thrown weapons, and heavy movement speeds/frames.
- Per-attacker `arest` and per-attacker/per-victim `vrest`; victim-wide post-hit immunity is not used by the active hit path.
- ITR kind 1/3 catch initiation, cpoint-driven victim positioning/actions, cpoint throw release, and ITR kind 4 falling/thrown-body contact first pass.
- ITR kind 6 passive super-punch activation first pass.
- Existing first-pass support for ITR kinds 5, 8, 9, 10, 11, 15 and 16 in the object/fighter paths where currently used.
- State-12 weak-hit protection (`fall < 41`) and state-14 CPU target de-prioritisation.
- Continuous Stage runtime with preloaded fighter/item assets and immediate dead-slot reuse.

### Hardware correction pass

The current 0.69 correction pass fixes four hardware-confirmed regressions: bottle duration no longer uses `weapon_hp`, L+□ provides an explicit Vita throw path through the authored throw frames, Stage dead-slot reuse preserves persistent corpse visuals, and hidden-character shortcuts resolve attack/jump command families instead of assuming every fighter has `hit_Fa`.

## High-priority fidelity gaps

### Combat / defense

- Exact PC hit-stop/hitlag timing and all `bdefend`/defend-break interactions.
- Full armor/super-armor semantics for relevant states and special characters.
- Exact fall-point/dance-of-pain thresholds and every state-specific reaction exception.
- Dark-red recoverable HP, exact healing limits, and all regeneration rules.
- Full projectile-vs-projectile/object-vs-object collision, reflection and ownership-transfer behavior.
- Exact thrown-body kind-4 damage/rest/ownership behavior for every catch/throw special.

### Catch / cpoint

- Complete `decrease` semantics and all rare cpoint kind/state combinations.
- Exact catcher/victim facing/cover rules in every negative-action/turning case.
- Character-specific scripted grabs beyond the generic cpoint runtime still need hardware comparison.

### ITR coverage

- ITR kinds 2, 7 and 14 require dedicated PC-faithful semantics rather than broad generic handling.
- Rare kinds 8/9/10/11/15/16 have only the behavior needed by the currently exercised stock objects/specials and need exhaustive validation.

### Weapons/items

- Exact durability / `drop_hurt` / broken-weapon rules and all weapon-specific hit/break sounds.
- CPU weapon/item pickup, selection, drinking and throw strategy.
- Exhaustive dash-weapon and airborne throw edge cases, including interruption/catch interactions.

### AI / movement

- Original PC decision weighting/timing is not yet replicated; current CPU special selection and navigation are native approximations.
- Exact roll/dash/stop-running and obstacle/ally spacing behavior needs comparison.
- State-14 retreat behavior is implemented, but original spacing/retarget timing still needs hardware observation.

### Stage / backgrounds / modes

- Full `stage.dat` directives such as reserve/join/act/y/music and all conditional phase transitions (`when_clear_goto_phase`) are not complete.
- Full `bg.dat` multi-layer/parallax renderer remains incomplete.
- Recording playback does not yet implement the deterministic `.lfr` event stream.
- Network Play remains a UX/protocol scaffold; Vita Ad-Hoc transport and PC 2.00a interoperability are not implemented.

## Validation rule

A feature moves from "first pass" to "verified" only after: (1) the stock DAT path is exercised, (2) Vita hardware log/visual behavior is checked, and (3) no regression is seen in Stage/VS basic combat. This avoids treating a successful cross-build as proof of PC-perfect runtime behavior.


## fix3 coverage update

The fix3 static audit reads the packed stock 2.00a DAT graph directly. It currently covers all 23 stock playable fighters, 71 special-command roots reachable from neutral/movement frames and 85 unique authored special-command links after including action/catch/combo frames. Runtime support added in this pass includes scrolling-world projectile bounds, type-3 timer/Z semantics, stock hidden-character activators, ITR kind 9 reflection, ITR kind 14 solid blocking, and current-frame combo command dispatch. This is static/runtime coverage, not a claim that every move has already been frame-perfect validated on Vita hardware.

Stage Mode now supports one Vita-controlled P1 plus up to seven selectable Team-1 COM companions. Stage uses a separate seven-actor enemy pool, so a full P1+7 COM party can still face seven simultaneous enemies. True additional human input remains part of the network/local-controller work rather than being simulated in this milestone.
