# Native Vita AdHoc input fix

- Vita-to-Vita lockstep now transports the complete Vita gameplay action mask instead of the retail PC 8-bit key byte.
- Triangle/R/L/analog-run remain available in native AdHoc matches.
- Native Vita matches no longer enable the unfinished stock-PC simulation compatibility branch.
- Direct fighter hit application rejects identical attacker/victim indices.
- Diagnostic logs are synced to storage at least every 250 ms so hard-exit test sessions remain recoverable.
