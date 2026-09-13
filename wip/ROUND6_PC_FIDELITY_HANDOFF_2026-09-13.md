# LF2Vita Round 6 PC Fidelity Handoff — 2026-09-13

## Claim boundary

This checkpoint is **not** a guarantee that every LF2 2.00a PC executable rule has been reproduced. It is the current build-green/audit-green hardware-test baseline. Known remaining gaps are listed below.

## Build baseline

- Source tree: `LF2Vita-round6-pc-fidelity-2026-09-13`
- Final verified local build directory: `build-round6-collision-final`
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

Full captured output: `LF2Vita-round6-validation-2026-09-13.txt`.

## Main Round 6 compatibility changes

- Background layer order, parallax camera scaling, continuous Stage camera origin and `zboundary` depth handling.
- Stage `ratio`/`times`, reserve lives, boss/soldier relationships, authored/random spawn X, authored items, Survival phase goto, act heal/revive, captive criminals and stock `join:` bosses.
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

## Known remaining non-PC-perfect areas

1. Exact CRAZY Stage `ratio` scaling/lookup remains an approximation; the exact Windows table has not been recovered with enough confidence.
2. Stage/background visual fidelity still needs Vita screenshots and logs, especially Stage 2/3 section transitions and foreground overlap.
3. Large-party Stage memory pressure must be rechecked on hardware; previous logs reached zero free CDRAM and fell back to user memory.
4. Rare DAT state/ITR/cpoint combinations not exercised by the stock regression paths are not exhaustively proven.
5. CPU AI implements documented stock behavior and exceptions but is not claimed binary-identical in every weighting/retarget/random threshold.
6. Original LF2 2.00a TCP interoperability is still a separate path and requires true Windows-PC end-to-end testing.

## Hardware test focus

- Stage 1-5 progression, especially Stage 2/3 visuals.
- Full/COM-heavy Stage party and memory behavior.
- CPU Milk/Beer pickup and drinking.
- Normal punch should no longer repeatedly hit every TU when no `arest` is authored.
- Rudolf Hide; Firzen fusion/separation/refusion.
- Captive releases and Bat/LouisEX joins.
- Falling and Burning bounce behavior.
- Native Vita AdHoc lockstep/desync logging.

## Artifact hashes (SHA-256)

- `LF2Vita-round6-pc-fidelity-2026-09-13.vpk`: `c782a8e0b7f22ed21ea6fb1ad321c84c6c6c994f9b6dc36d317906bf366b4edd`
- `LF2Vita-round6-eboot-2026-09-13.bin`: `f7479503c930afcd50bdbbc75003d3605016c7a540901d75b0e9a3e439930174`
- `LF2Vita-round6-source-2026-09-13.zip`: `02dbd4b5d711576291b78408c5ce589514642e105da19d74649d5d9afcc508b7`
- `LF2Vita-round6-delta-vs-round5-2026-09-13.patch`: `e8ff31c7dd2c0840bb5433ddc1054dc3a2b3ded949b4f2f63e36c3d16bd975e5`
- `LF2Vita-round6-validation-2026-09-13.txt`: `10f95246fda1998ffcc3d558e1ff9dfe2440fe5a04aead93bde5572a713030b8`
