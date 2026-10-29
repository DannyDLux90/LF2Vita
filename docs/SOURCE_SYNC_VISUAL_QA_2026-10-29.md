# LF2Vita Round 6 – source-sync and visual review (2026-10-29)

This checkpoint compares the public GitHub project with the **user-supplied 2026-09-13 hotfix bundle** and four Vita screenshots supplied on 2026-10-29. This is a review of the uploaded artifacts, **not a claim of a new hardware test**.

## Provenance and branch boundary

- Repository: `DannyDLux90/LF2Vita`.
- Existing `main` at the time of review: `de63762e319a2d10a2ede5b046501134d7eab86c` ("Start v0.70 AdHoc browser and synchronized lobby").
- This review branch was forked from `pc-compat-round6-pc-fidelity` at `793ae92a0ce4952162179c5afae022720a32cceb` (81 commits ahead of the prior `main`).
- **Important:** the locally uploaded `LF2Vita-next-chat-source-workspace-2026-09-13.zip` is NEWER than this Git branch. Do **not** advertise this PR or branch as byte-identical to the hotfix VPK.

The handoff describes source in `SOURCE/LF2Vita-round6-stagecrash-net7-hotfix-source/` and a matching full `WORKSPACE/project/`. The artifact `LF2Vita-round6-stability-hotfix-2026-09-13.vpk` has SHA-256 `5b562d29e009a96a9e5eb255d7162607a6549d0db774b4ae3786a73a89db274d`. The VitaSDK package, generated gamepack, VPK and LF2-derived art should not be committed as public source.

## Uploaded hotfix changes not yet reconciled with Git

The supplied `CURRENT_HANDOFF.md` identifies the following changes since the older Round 6 code:

- Stage Mode crash fix: `lf2_stage_t` exceeded the 256 KiB Vita main-thread stack; local and network Stage paths must use heap allocation.
- Native AdHoc v7: solo host start, host remains discoverable while in match, deterministic hot-join relaunch, disconnect to AI and reusable network slots.
- One-shot fall/bounce correction: `fall_bounce_used` prevents repeated state-12 bounce loops and is included in the lockstep digest.
- Additional PC 2.00a behavior work: Stage camera/layers/zboundary, 30 Hz combat timings, character states and items, MP/dark HP, active simulation RNG and collision semantics.

Example checks show local `src/fix3/game_14.inc`, `src/fix3/game_24.inc`, `src/net70/adhoc_net_04.inc`, `src/net70/adhoc_net_05.inc`, `src/fix3/main_06.inc` and `src/game.h` have different blob hashes from the Round 6 Git branch. **Before merging**, compare the entire uploaded `SOURCE/` snapshot to this branch and commit ALL required hotfix source changes, avoiding an older code rollback. Do not merge by blindly overwriting the complete source tree with the earlier `main` branch.

## Screenshot review (user-provided screenshots, 960×544)

The screenshots were examined as visual QA evidence. Original screenshots are not embedded here because the repository policy excludes redistribution of original/derived LF2 artwork; hashes below identify the exact supplied inputs for authorized local comparison.

| File (2026-10-29) | SHA-256 prefix | Observation | Verification needed |
| --- | --- | --- | --- |
| `203155.png` | `eae2842f1ef3` | Eight character selection slots, character portraits, Team 1/COM assignments and P1 Henry | Check roster navigation, selected slot/fighter consistency, fonts and team changes |
| `203211.png` | `719f2e348e85` | Stage 1 with eight HUD slots, a crowded mid-screen fighter cluster, projectiles/effects and COM behavior | Hardware-test depth sorting, overlap, AI, hit feedback and crowd stress |
| `203237.png` | `91948e5917e5` | Another Stage 1 frame with multiple knocked-down actors, eight HUD slots, and visible health/mana bars | Verify corpse lifetime, recovery/bounce transitions and bars under large parties |
| `203313.png` | `1008bfdf6baa` | German Network Play selector with PS Vita / PC choices; PC notes original LF2 2.00a / TCP 12345 | Treat PC interoperability as **not yet demonstrated** merely because its UI entry exists |

For future screenshots test Stage 2–5 background layer/camera transitions in addition to Stage 1. Also test two real Vitas to verify AdHoc v7 rather than inferring network correctness from static screens.

## Static regression verification of uploaded source (2026-10-29)

Using the exact source snapshot in the uploaded ZIP and the matching `WORKSPACE/project/gamepack/game.lf2pak`, the following 15 scripts exited successfully:

`test_action_timing_pc_semantics.py`, `test_adhoc_protocol.py`, `test_ai_pc_semantics.py`, `test_blink_pc_semantics.py`, `test_collision_pc_semantics.py`, `test_determinism_semantics.py`, `test_fusion_pc_semantics.py`, `test_hide_pc_semantics.py`, `test_item_pc_semantics.py`, `test_movement_pc_semantics.py`, `test_resource_pc_semantics.py`, `test_rest_pc_semantics.py`, `test_stage_pc_semantics.py`, `test_stage_stack_semantics.py`, `test_state_pc_semantics.py`.

The AdHoc model reported `avg_wait_ms=12.48`, `worst_wait_ms=129`, `start_worst_ms=1922`. These checks do not prove frame-perfect PC equivalence, real-world network reliability or runtime stability on Vita. No fresh cross-build or physical Vita execution has been performed as part of this documentation update.

## Merge checklist

1. Import the *exact* latest hotfix source from the user's 2026-09-13 bundle into this branch, e.g. first run `python3 tools/sync_source_snapshot.py /path/to/LF2Vita-next-chat-source-workspace-2026-09-13.zip` for a dry-run and then repeat with `--apply` on a clean checkout. Review the Git diff. This script only copies allowed text source and deliberately excludes proprietary gamepak/derived art/VitaSDK packages.
2. Compare file hashes and source/asset manifests, including Stage heap allocation, AdHoc v7 and bounce correction.
3. Re-run the regression scripts and perform a clean VitaSDK ELF → VELF → SELF → VPK build.
4. Verify Stage 1–5 on Vita (background/camera, crowd of 8 allies, bounce/AI/MP), plus two-device AdHoc discover/join/host/disconnect/hot-join.
5. Only after these checks promote this branch to `main` and update README/version claims.
