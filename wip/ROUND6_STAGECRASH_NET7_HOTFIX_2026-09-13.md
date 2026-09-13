# LF2Vita Round 6 — Stage Crash + AdHoc v7 Hotfix — 2026-09-13

## Why this hotfix exists

The Round-6 PC-fidelity VPK crashed immediately after selecting Stage Mode on
real Vita hardware. Both supplied Vita core dumps point to the same Data Abort.
The enlarged Round-6 `lf2_stage_t` was allocated as a local object and pushed
the Vita main thread beyond its 0x40000 (256 KiB) stack before character select.

Local Stage and AdHoc Stage now allocate the large Stage parser/runtime objects
on the heap. The final ARM build reserves only 3644 B in `main()` and 348 B in
`run_stage_major_continuous()`.

The supplied VS log also exposed an independent Round-6 regression: a fighter
could repeatedly re-enter the state-12 bounce after the first dvx2/dvy-2
bounce. A deterministic `fall_bounce_used` state now makes 185/191 the one
bounce before settling into 230/231; it is part of the lockstep state digest.

## Native Vita AdHoc protocol v7

This hotfix also changes the Vita-to-Vita session lifecycle:

- Host can press START immediately with no guest and play solo/against AI.
- The AdHoc Matching/lobby session remains alive and advertised during gameplay.
- Browser allows joining both Waiting and In-Game hosts when a slot is free.
- A brand-new late join is accepted while gameplay is active.
- A guest already connected but unready may press Ready while the host is
  playing solo and join the same way.
- Hot-join uses a controlled synchronized gameplay relaunch. It does not inject
  a new client into an arbitrary existing TU because there is no complete
  cross-machine state-snapshot protocol yet. This keeps RNG/input lockstep
  deterministic.
- If the active guest disconnects, the host continues immediately; the fighter
  falls back to AI, the disconnected player's lobby slot is cleared, and the
  host remains joinable.
- If a reserve Vita is already present, the lowest reserve slot is promoted
  deterministically and the runner relaunches.
- Current gameplay lockstep is still host + one active remote Vita. The lobby
  may hold up to 8 players; additional Vitas are reserves/spectators rather
  than simultaneous 3+ Vita lockstep participants.

Protocol number is now 7, so older native AdHoc builds are intentionally
incompatible with this hotfix.

## Validation

PASS on the final `build-round6-stagecrash-net7` source:

- VitaSDK ELF/SELF/VPK build
- special-command audit: 23 fighters / 71 neutral command roots
- AdHoc v7 deterministic stress: avg 12.48 ms, worst regular 129 ms,
  start recovery worst 1922 ms
- action timing
- CPU AI
- Blink/get-up
- collision/default zwidth
- deterministic state digest
- Firzen fusion
- Rudolf Hide
- item director
- movement/Rowing/weapon drop
- HP/MP resources
- Arest/Vrest
- Stage join/criminal/CRAZY ratio
- state 6/12/18
- Stage stack regression guard

Compiler warnings are the existing legacy/unused/Sony-AdHoc signness warnings;
there are no new compile/link errors.

## Hardware test priority

1. Enter Stage Mode repeatedly; it must reach character select and Stage 1.
2. Play Stage with a crowded COM party and watch memory/load stability.
3. Host an AdHoc game with only one Vita and press START immediately.
4. While the host is in gameplay, find it from a second Vita and use
   `Nachjoinen`; both should automatically relaunch into synchronized gameplay.
5. Disconnect the guest during play. Host should continue with AI and remain
   discoverable; reconnect/rejoin and verify slot reuse.
6. Verify ordinary knocked-down fighters bounce once, not repeatedly.
7. Continue Stage 2/3 visual/background verification.

## Artifacts / SHA-256

- `LF2Vita-round6-stagecrash-net7-hotfix-2026-09-13.vpk`
  `3583d6140c65cbbe1180a8257fab35764ee38dc0d7d7375a6ca716c92d425bb9`
- `LF2Vita-round6-stagecrash-net7-hotfix-eboot-2026-09-13.bin`
  `3b37b36338f60fdc82f9bac82ba07f1dd7913b508a4d5a39d5e0efbfcdf53c67`
- `LF2Vita-round6-stagecrash-net7-hotfix-source-2026-09-13.zip`
  `613fbc849549138d1b52e754c9a0a3654f215483a6563be5f7a4acbff1e2cf61`
- `LF2Vita-round6-stagecrash-net7-hotfix-vs-round6.patch`
  `370086cbc78435a8c851657eeee67a61c66a88809ad3d7edbe489a42b7db9824`
- `LF2Vita-round6-stagecrash-net7-hotfix-validation-2026-09-13.txt`
  `a957c310f0a0b69597d1aac8886a4962b4830f4da09382a04c5a9eda401ce9c8`

## Claim boundary

This is a build-green/audit-green hardware hotfix, not a guarantee of binary
identity with every LF2 2.00a executable edge case. Hardware testing remains
required, especially Stage visual/memory behavior and real two-Vita hot-join.
