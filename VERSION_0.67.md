# LF2Vita 0.67

Hardware-test update focused on uninterrupted Stage Mode, dependable item interaction and readable Vita-native UI text.

## Stage Mode

- Removes the remaining fixed 75-tick / 2.5-second pause between cleared Stage waves.
- The next authored phase opens its world bound and populates enemies/items immediately in the same simulation tick after the previous wave has settled.
- Player, camera, background, fighter state and persistent object world remain alive across the transition; no round/result/GO overlay is inserted between waves.
- New `stage:wave_advance` diagnostics record the bound change and spawned enemy/item counts without a wait state.

## Items and controls

- Expands the grounded-item pickup window from 52x32 to 78x46 world pixels.
- Pickup accepts Square while held, not only the single pressed edge.
- Weapons use the normal Square attack path; hold Square with milk/beer to drink.
- The fight HUD shows contextual `Square: Pick up ...` / `Hold Square: Drink ...` hints.
- In-game guide and Vita documentation describe pickup, weapon use and drinking.

## Vita system font

- Legacy menu text is routed through the same `vita2d_load_default_pvf()` Vita system font used by the rest of the UI.
- The PVF outline is reduced to a cleaner 1-pixel four-direction outline for small-menu readability.
- PGF remains only as an emergency fallback if PVF initialization fails.

The supplied v0.67 test build was compiled successfully with VitaSDK 2026.08.1 plus the provided vita2d/freetype/png/jpeg/zlib packages. Hardware validation on a Vita is still required.
