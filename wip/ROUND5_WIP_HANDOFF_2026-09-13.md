# LF2Vita Round 5 WIP handoff — 2026-09-13

Continuation of the uploaded Round-4 PC-fidelity snapshot.

## Fixes in this checkpoint
- Fixed Round-4 compile regression in `src/fix3/main_06.inc`: Stage runtime Dark-HP parameters were used by the implementation/callers but missing from the `run_stage_continuous()` signature.
- Fixed GCC 15 compile regression in `src/fix3/game_08.inc`: added the forward declaration for `object_frame_def()` before first use.
- Fixed pickup/vrest corruption in `src/fix3/game_14.inc`: a loose-object slot index was incorrectly written into the fighter victim `vrest_ticks[]` array.
- Fixed authored background draw order in `src/background.c`: preserve `bg.dat` file order instead of sorting layers by parallax width.
- Fixed background camera scaling in `src/background.c`: camera displacement is already in Vita/world coordinates and must not be multiplied by the 794->960 source-coordinate scale a second time.
- Fixed continuous Stage section background reset in `src/fix3/game_23.inc` and `game_24.inc`: section identity remains authored-world based, while the rendering camera origin resets to the actual global camera at the section transition. This removes the long local-camera=0 freeze visible in the supplied hardware log.

## Validation
- VitaSDK: 2026.08.1 core + uploaded libvita2d/libpng/libjpeg-turbo/freetype/zlib packages.
- Full CMake build: PASS (ELF, SELF, VPK).
- `tools/audit_fix3.py --source current_concat.c gamepack/game.lf2pak`: PASS (23 stock fighters, 71 neutral special-command roots).
- `tools/test_adhoc_protocol.py`: PASS (`adhoc protocol v6 stress: OK`, avg 12.48 ms, worst 129 ms, start worst 1922 ms).
- Hardware visual verification is still required, especially vertical/depth alignment of Stage backgrounds.

## Supplied hardware-log diagnosis
The Stage content itself loads and advances. In Stage 2 the supplied log reaches 20/20 phases. Before this fix, every authored section transition reports `local_camera=0.0` because the stored authored world section origin is ahead of the actual camera (for example origin 1660 at global camera 659, and origin 3320 at global camera 2940). That freezes the background at the left edge until the global camera catches the authored offset.

## Next PC-fidelity targets
- Hardware-check Stage 2/3 after the camera reset/order fix; use screenshots or a fresh `lf2.log` if vertical/depth alignment remains wrong.
- Verify `bg.dat` z-boundary mapping against the fighter depth projection before changing global fighter Z limits.
- Continue resource-formula tests: Dark HP/regeneration, John/Sorcerer/Jan healing, Milk/Beer, throw injury.
- Continue Frozen landing/break, John shield kind:9, state 3000/3006 reflect, held-weapon kind:5, and simulation RNG audit.
