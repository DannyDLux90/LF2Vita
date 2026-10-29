# Round 4 PC-fidelity WIP checkpoint — 2026-09-13

This directory records the current Round-4 work-in-progress state before moving the work to a new ChatGPT conversation.

Parent/source baseline before this checkpoint: `150a36272318a1e17495868ad9e315609f94efc1` on `pc-compat-wip`.

The canonical complete backups created in the conversation are:

- `LF2Vita-round4-wip-source-2026-09-13.zip`
  - SHA-256 `123eee8ed632c2ba765a17941f5b3c44c69f44194d37ea938d3760ed182b7b92`
- `LF2Vita-round4-wip-workspace-2026-09-13.zip`
  - SHA-256 `607809def284410562056f46e42b5e621130303664f2124f7b35b6a0286dd9e6`
- `LF2Vita-round4-wip-vs-150a3627.patch`
  - SHA-256 `023a809b87c02cfabd0d31bbad339ebc6f23fa231cb5920026d0e9130ed7ca8e`

`ROUND4_WIP_HANDOFF_2026-09-13.md` describes the gameplay work, validation status and next steps. `ROUND4_WIP_MANIFEST_2026-09-13.txt` records SHA-256 values for the canonical source tree so a restored copy can be verified byte-for-byte.

Important validation note: the latest WIP source passed the static `fix3` audit and AdHoc protocol stress test, but the final tip could not be recompiled in the handoff session because the temporary VitaSDK mount was no longer available. Compile this exact source snapshot first in the next session before calling the tip build-green.

Do not commit `gamepack/game.lf2pak`; it is present only in the workspace backup and remains ignored by the repository.
