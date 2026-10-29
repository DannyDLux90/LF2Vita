# Rebuilt uploaded 2026-09-13 source with VitaSDK (2026-10-29)

The **uploaded local source snapshot**, not the current pull-request tip, was configured and cross-compiled with the user-supplied VitaSDK package set on Linux.

```sh
export VITASDK=/mnt/data/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build-github-sync -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-github-sync -j2
```

Result: **PASS**, full VPK path `build-github-sync/lf2vita.vpk` produced after `elf -> velf -> eboot.bin -> param.sfo -> vpk`. Compiler warnings did not block linking.

Fresh local VPK SHA-256:
`b2f92ee813d7efe4044de8b0c382713eef7fff492ccf429dc39ae23c31774c69`

User-provided 2026-09-13 hotfix VPK SHA-256:
`5b562d29e009a96a9e5eb255d7162607a6549d0db774b4ae3786a73a89db274d`

The two VPK archives are **not byte-identical**. The rebuilt package includes an extra `assets/main_bgm.wav` and has a different `eboot.bin`. The game data archive and game art were supplied locally and must not be added to public Git history.

The 15 static/regression scripts passed when run against the uploaded source and its matching local `gamepack/game.lf2pak`. No hardware execution and no Vita-to-Vita network validation was performed in this step.

**Follow-up gate:** import exact source changes into the PR, then rebuild and test the PR tip separately before merging.
