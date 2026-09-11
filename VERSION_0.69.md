# LF2Vita 0.69

PC-fidelity pass built on the v0.68 hardware-test runtime.

## Weapon throwing and held-weapon data

- Item pickup remains a dedicated **L** action; Square is attack/use.
- Held weapons now use the stock type-0 attack groups (20/25 normal, 30 jump, 35 run, 40 dash) and their per-frame `wpoint` data.
- Light throws use the stock fighter frame groups 45-47 and 52-54; heavy throws use 50-51. The item is released only when the authored throw frame supplies `wpoint dvx/dvy/dvz`.
- Throw commands follow the PC hierarchy implemented by the original data: heavy objects/baseball throw directly with Attack; knife/boomerang use direction+Attack; bottle/ordinary light-weapon throws are selected during the corresponding run/jump/dash situation.
- Recoverable thrown weapons return to a loose pickup after impact/landing instead of disappearing permanently.
- `weapon_strength_list` entries are parsed and `injury: 789` is resolved through `wpoint.attacking`, replacing the previous hard-coded held-weapon damage bonus.
- Heavy weapons use their heavy walking/running speeds and stock 12-19 movement frames when those frames are available.

## Repeat-hit / invulnerability correction

The v0.68 victim-wide immunity was already removed, but two replacement shortcuts could still suppress valid hits: fighter `hit_latched` and object `hit_mask`. v0.69 moves the active combat path to LF2-style rest counters:

- `arest` blocks only the attacker for the authored number of ticks.
- `vrest` is tracked per attacker/target pair.
- Stage fighter slot reuse resets all rest/catch state.
- The legitimate PC falling rule remains: a state-12 target ignores weak ordinary hits (`fall < 41`). This must not be confused with the previous global invulnerability bug.

## Catch / throw runtime

- The DAT parser now reads per-frame `cpoint` blocks including `vaction`, `aaction`, `jaction`, `taction`, throw velocity/injury, `hurtable`, `decrease`, `dircontrol`, `cover`, and front/back hurt actions.
- ITR kind 1/3 can establish a catcher/victim pair. The victim follows the current catcher's `cpoint` position/action rather than a fixed Vita timer.
- Attack/Jump/direction+Attack while holding a victim can select the authored catch actions.
- The actual character throw occurs only on a `cpoint` frame carrying throw velocity, and the thrown victim becomes a falling-body hit source for ITR kind 4.
- ITR kind 6 is treated as the passive super-punch trigger rather than a normal damage hit.

This is a substantial first native implementation of the PC catch system, not a claim that every rare `cpoint` edge case is already exact.

## Stage continuity and CPU target selection

- A dead Stage enemy slot is reusable immediately at HP=0. The next wave no longer waits for the defeated model to finish its knock-down/lying animation.
- Stage fighter assets remain preloaded/shared from v0.68; no enemy texture upload is reintroduced into the wave transition.
- A CPU does not select a living opponent in state 14 (lying) as its normal attack target. If that is the only opponent, it backs away until the target is active again.

## Data-coverage audit

The packed LF2 2.00a DAT set used for this build contains 5,868 frames, 5,042 `bdy`, 4,079 `wpoint`, 1,936 `itr`, 521 `cpoint`, 472 `bpoint`, and 115 `opoint` blocks. `PC_FIDELITY_AUDIT.md` records the remaining implementation gaps by subsystem.

## Hardware-test checklist

1. Pick up a knife/boomerang with L, then throw it with direction+Square. Verify the item leaves the hand on the final throw frame and can be recovered after landing.
2. Pick up a heavy object/baseball and throw with Square. Check heavy walking/running poses before throwing.
3. Fight several opponents/projectiles and verify unrelated attacks are no longer rejected after another attacker has hit the same enemy.
4. Check grabs (especially standard stunned grabs and Louis catch/throw) for victim positioning, attack while held, and throw release.
5. Let the player enter the lying state and verify CPUs stop crowding/attacking the body and move away.
6. Clear several Stage waves and verify the next enemies start without waiting for the previous bodies to finish their lying animation.

## Hardware correction pass (same 0.69 milestone)

- Fixes Milk/Beer duration: bottles use the stock 100/125 drink-time units instead of the unrelated `weapon_hp: 450` value.
- ○ now interrupts state-17 drinking without discarding a partially used bottle.
- Adds an unambiguous Vita item throw command: **L+□** while holding an item. It starts the stock light/heavy/air throw frames and releases on authored `wpoint dvx/dvy/dvz`.
- Keeps original PC throw triggers (including knife/boomerang direction+attack) in parallel.
- Splits Vita special shortcuts: △ selects attack-special command roots and R selects jump-special roots, with directional variants and DAT-driven fallbacks. This covers hidden characters such as Firzen and Jan that do not expose `hit_Fa` as their primary command.
- Stage CPU slots may still be reused immediately at HP=0, but a shared-texture corpse snapshot is retained in the world so defeated enemies no longer disappear when the next wave occupies the logical slot.
- Current milestone is PC gameplay fidelity. Network play, server browser, PC-compatible networking, and trophies (disabled for cheat-mode sessions) are tracked as subsequent milestones in `ROADMAP.md`.


## fix3 — hardware correction and full special audit

- Item throw ownership is now locked for fighter frames 45–54 until the authored `wpoint` release frame fires. This prevents bottle drinking or ordinary held-item input from replacing a throw that has already started.
- Stage-mode projectiles use the current scrolling world bounds instead of the old fixed 960-pixel VS arena. Firzen beam/ball objects and every other projectile can therefore remain alive beyond x=1140.
- Type-3 projectile `hit_a`/`hit_d` timing and authored `hit_j` Z movement are handled, including the stock chase/expiry patterns used by Dennis, Woody, Davis, Jan, Firzen and Julian.
- Engine-side stock `hit_Fa` activators used by Jan, Bat, Firzen and Julian are implemented, including Jan Angel regeneration.
- John-style ITR kind 9 projectile reflection and Freeze ITR kind 14 solid blocking have first-pass native runtime behavior.
- Special command links authored on non-neutral/action frames can execute while that action is active. The fix3 audit covers 23 stock fighters, 71 neutral command roots and 85 unique authored command links in total.
- Stage Mode now offers 0–7 selectable COM companions after P1 selection. Companions stay on Team 1, reserve their fighter slots across waves, use the same preloaded asset cache, and carry HP/MP between major Stage sections. Stage enemies use a separate seven-actor pool, so even a full P1+7 COM party still has seven live enemy slots; larger authored waves continue to queue through those enemy slots.
- Startup file verification is visually quieter: the translucent progress panel/frame is removed, leaving only a small two-pixel progress line and compact status text.
- Runtime/log identification is `0.69 fix3`; the Vita package version remains `00.69`.
