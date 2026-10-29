# LF2Vita Round 6 PC Fidelity Handoff — 2026-09-13

## Claim boundary

This checkpoint is **not** a guarantee that every LF2 2.00a PC executable rule has been reproduced. It is the current build-green/audit-green hardware-test baseline. Remaining uncertainty is concentrated in hardware-only behavior and rare/unexercised edge cases.

## Build baseline

- Source tree: `LF2Vita-round6-pc-fidelity-2026-09-13`
- Final verified local build directory: `build-round6-crazyfinal`
- Toolchain: uploaded VitaSDK 2026.08.1 package set used in this session
- Final local build result: ELF/SELF/VPK PASS
- Compiler warnings: legacy/unused helper warnings and Sony AdHoc signedness warnings remain; no new Round-6 compile/link errors.

## Validation matrix

PASS:
- `tools/audit_fix3.py`: 23 stock fighters, 71 neutral command roots; audited neutral stock special commands have runtime effect paths.
- `tools/test_adhoc_protocol.py`: avg wait 12.48 ms, worst regular 129 ms, start worst 1922 ms in deterministic stress model.
- action timing PC semantics
- CPU AI PC semantics
- Blink/get-up semantics
- collision/default zwidth semantics
- determinism/lockstep digest semantics
- Firzen fusion semantics
- Rudolf Hide semantics
- item-director semantics
- movement/Rowing/weapon-drop semantics
- HP/MP/resource semantics
- Arest/Vrest semantics
- Stage join/criminal semantics
- state 6/12/18 semantics
- exact CRAZY Stage ratio multiplier table rule (`floor(1.5*N)+1` before authored-ratio truncation)

Full captured output: `LF2Vita-round6-validation-2026-09-13.txt`.

## Main Round 6 compatibility changes

- Background layer order, parallax camera scaling, continuous Stage camera origin and `zboundary` depth handling.
- Stage `ratio`/`times`, exact CRAZY ratio multiplier, reserve lives, boss/soldier relationships, authored/random spawn X, authored items, Survival phase goto, act heal/revive, captive criminals and stock `join:` bosses.
- Larger Stage actor and queue capacities.
- CPU weapon/drink pickup, drink retreat/use, healing decisions and defensive reaction paths.
- Default character Arest, Vrest cap/reset semantics and elimination of repeated same-punch TU hits.
- PC default ITR `zwidth=15` across fighter/object/held-weapon paths.
- PC-style movement oscillation/diagonal scaling, dash constants, Rowing, facing changes and held-weapon drop rules.
- Dark-HP/resource/MP boundary fixes, clone MP/carry/form behavior.
- Rudolf Hide, Blink/get-up invulnerability, Firzen fusion controller/refusion rules.
- Falling/Burning landing behavior and bounce rules.
- Item-director total weapon counting and background-aware drop depth.
- Expanded lockstep digest coverage and deterministic simulation RNG audit.

## Remaining non-guaranteed areas

1. Stage/background visual fidelity still needs Vita screenshots and logs, especially Stage 2/3 section transitions and foreground overlap.
2. Large-party Stage memory pressure must be rechecked on hardware; previous logs reached zero free CDRAM and fell back to user memory.
3. Rare DAT state/ITR/cpoint combinations not exercised by the stock regression paths are not exhaustively proven.
4. CPU AI implements documented stock behavior and exceptions but is not claimed binary-identical in every weighting/retarget/random threshold.
5. Original LF2 2.00a TCP interoperability is still a separate path and requires true Windows-PC end-to-end testing.

## Hardware test focus

- Stage 1-5 progression, especially Stage 2/3 visuals.
- Full/COM-heavy Stage party and memory behavior.
- CPU Milk/Beer pickup and drinking.
- Normal punch should no longer repeatedly hit every TU when no `arest` is authored.
- CRAZY Stage enemy counts with 1–8 normal/Firzen/Julian party weighting.
- Rudolf Hide; Firzen fusion/separation/refusion.
- Captive releases and Bat/LouisEX joins.
- Falling and Burning bounce behavior.
- Native Vita AdHoc lockstep/desync logging.

## Artifact hashes (SHA-256)

- `LF2Vita-round6-pc-fidelity-2026-09-13.vpk`: `e72ad2fe07cf136aa62ee1dfb2d569486a27e4243c3985b6eee49a3ab0a720dd`
- `LF2Vita-round6-eboot-2026-09-13.bin`: `c32051c28801c7cdbae9cfa364cd2ef82df5f3c8daf3784c6adb72504fd3d9ec`
- `LF2Vita-round6-source-2026-09-13.zip`: `9b973199396994b1f6e778faa184df831b1cb950113c39bb89b715f4741c478e`
- `LF2Vita-round6-delta-vs-round5-2026-09-13.patch`: `ec4c5a436a916a0dfc0f55555efe309f0a18bca4abe2e754e51fd78395f2f229`
- `LF2Vita-round6-validation-2026-09-13.txt`: `2b3cd4df6a15de38ab6326edf12848ff82d58fa5f3d6fef67fc904545247c868`
