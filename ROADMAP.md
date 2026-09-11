# LF2Vita roadmap

The project keeps version **0.69** as the active hardware-test branch while the current milestone is completed. Milestones are ordered deliberately: gameplay/state determinism comes before networking, because PC-compatible lockstep is not meaningful until the native simulation is stable.

## Current milestone — PC gameplay fidelity / optimization

Goal: make stock Little Fighter 2 2.00a gameplay behave as closely as practical to the PC version on Vita. Work includes fighter state/command fidelity, hidden characters, weapons and consumables, hit/rest/defense rules, catches/throws, AI target behavior, Stage progression/corpses, transformations, projectiles, background behavior, and hardware-log regression testing.

Current fix3 work also includes Stage party setup (P1 + selectable COM companions) and whole-roster special-command audits.

Exit criterion: no known high-impact mismatch in ordinary stock VS/Stage play, and the remaining differences are documented in `PC_FIDELITY_AUDIT.md`.

## Next milestone 1 — Network play

Stabilize deterministic 30 Hz input/state synchronization, checksums and Vita peer transport. Initial networking may target Vita-to-Vita/LAN or Ad-Hoc play before public discovery.

## Next milestone 2 — Server browser

Add session discovery/listing, host metadata, refresh/filter/join flow and clear connection/error states on top of the network transport.

## Next milestone 3 — PC-compatible network play

Reproduce the original LF2 2.00a network framing/synchronization closely enough for Vita-to-PC interoperability. This requires protocol capture/reverse engineering and explicit cross-platform desync testing; it is separate from the Vita-native network path.

## Next milestone 4 — Trophies

Add trophy definitions and unlock tracking only after gameplay and networking behavior are stable. **Any session launched in LF2.NET/cheat mode is trophy-ineligible**, and achievements must not unlock from cheat-mode progress.
