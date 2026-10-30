# Stage-Kollisionen: sicherer Integrationsplan für Round 27+

**Status:** Nur Host-C-Prototyp, **nicht** in der Vita-Engine eingebaut.
Diese Datei ersetzt keinen PC-2.00a-Vergleich und keinen Vita-Hardwaretest.

## Warum dieser Ansatz

Das bestehende Stage-Spiel durchläuft für jeden aktiven angreifenden Fighter
potenziell alle aktiven Ziele. Round 27 filtert bereits untaugliche Angriffsträger
mit `src/lf2_stage_attack_gate.h` heraus. Die hier erprobte **zweite Stufe**
würde erst bei **vollständig bekannter Weltgeometrie** ein Paar überspringen,
wenn die Rechteckvereinigungen sicher disjunkt sind.

## Verhalten, das niemals verändert werden darf

- Angreifer- und Zielreihenfolge bleiben `a=0..total-1`, `b=0..total-1`.
- `apply_hits(world,fighters,total,a,b,difficulty)` bleibt für jedes
  mögliche Trefferpaar unverändert, einschließlich arest/vrest, Waffentreffern,
  CATCH, DEFEND, Hitlag, Spezialangriffen, RNG und späteren Mutationen.
- Die konservativen Bounds verwenden **genau die von `region_world` und
  `object_region_world` gelieferten ganzzahligen Weltkoordinaten**,
  nicht bloß die Standposition der Figuren oder geschätzte Reichweiten.
- Es müssen alle Fighter-ITRs berücksichtigt werden, die `apply_hits`
  als Kontakt verarbeitet (laut Round-27-Audit: 0,4,5,10,11,15,16 sowie
  1/3 nur bei authored Catch-Action). Der Sonderfall einer **gehaltenen
  Waffe** muss seinen tatsächlichen ausgerichteten world-`wpoint` und
  alle für den Treffer relevanten Waffenregionen in den Bereich aufnehmen.
- Ziel-Bounds vereinen **alle aktuellen BDYs**. **Neu überprüfter
  PC-Sonderfall:** Ein normaler Schlag in Frame 60/65 kann eine passive
  **ITR kind 6** berühren und dadurch auf Frame 70 (*Super-Punch*)
  umschalten, **ohne dass sich ITR und normale BDY überlappen**.
  Daher müssen für diesen Angreifer auch sämtliche kind-6-Rechtecke
  des Zieles als zusätzliche Kontaktmöglichkeit berücksichtigt werden
  (API: `lf2_pair_may_contact_with_passive`). Der Kind-6-Treffer
  bleibt trotzdem passiv und darf nicht selbst als Angriff wirken.
- Gehaltene Waffen haben eine eigene, früher ausgeführte Kollision
  `apply_held_weapon_hit`; wenn deren world-space-`wpoint`-Transformation
  nicht vollständig eingerechnet ist, **Angreifer-Bounds als
  `complete=0` markieren**, sodass der Originalcode ungekürzt läuft
  (`lf2_pair_bounds_uncertain`).
- Sonderzustände, in denen die Trefferwirkung nicht auf den errechneten
  Bereichen beruht, müssen ebenfalls konservativ `complete=0` setzen.
- Position, Facing, Frame, held_oid, weaponact, Klonstatus und ähnliche
  für Weltregionen relevante Daten **können während der Paarfolge wechseln**.
  Deshalb müssen die Bounds vor ihrer Verwendung aktuell sein.
  Caching ohne nachgewiesene Invalidation ist verboten.
- Unvollständige, leer errechnete oder ungewöhnliche Regionen werden mit
  `complete=0` behandelt und vom bestehenden `apply_hits` vollständig
  geprüft (**fail-open**, nie ein fraglicher False Negative).
- Keine Änderung der Simulation von 30 TUs/s, Animation-`wait`-Daten,
  Boss-/Spawn-Mengen oder der AdHoc-Digest-Reihenfolge.

## Sichere Reihenfolge der späteren Integration

1. **Erst auf die vollständig vorliegende Round-27-Quelle anwenden.**
   Das öffentliche GitHub-`main` ist älter und darf nicht die neuesten
   Stage-/Steuerungs-/Trinkkorrekturen überschreiben.
2. Vor jedem `apply_hits`-Aufruf den konservativen Bereich bestimmen.
   Zuerst ohne dauerhaften Cache; erst nach der korrekten Version
   gegebenenfalls gezielt invalidierende Bounds-Caches ergänzen.
3. **Orakeltest auf dem wirklichen Runtime-Code**, nicht nur auf synthetischen
   Rechtecken: Quellstand vor/nach der Optimierung mit gleichen Eingaben
   ausführen, sämtliche Trefferereignisse, Reihenfolgen, Statuswerte,
   Spawns und Zufallszustände vergleichen. Teste u. a. verwandte
   Team-Angriffe, Waffenausrichtung, Spezialangriffe, geworfene Körper,
   Griffe, Zustand 12/16, große/negative Bounds, Spiegelung und Klone.
4. AdHoc-Stage-Digest und VS Mode kontrollieren; andere Modi dürfen nicht
   unabsichtlich verändert werden.
5. VitaSDK-Crossbuild und echte 8-/40-Gegner-Hardwaremessungen durchführen.
   Der Host-Test ist **keine** FPS-Messung.

## Bereits vorhandene Prüfmöglichkeiten

- `tools/lf2_stage_pair_bounds.h` – konservative Bounds, niemals in
  Produktionscode inkludiert.
- `tests/native/test_stage_pair_bounds.c` – 1.800 synthetische Szenen
  (8, 40, 72 Figuren), inklusive passiver kind-6-Super-Punch-Ereignisse,
  unvollständiger Waffenbereiche, Statusmutationen, gleicher Reihenfolge,
  identischen Ereignishashes und Actor-Zuständen.
- `tests/native/test_stage_world_regions.c` – 200.000 zusätzliche
  Fälle mit der historischen `region_world`-Spiegelungsformel,
  negativen Positionen, kind-6-Reaktionszonen und unsicherer
  Waffen-Geometrie. Ergebnis im Host-Test: **0 False Negatives**,
  89.581 aussortierte Fälle. Das Testmodell ersetzt **nicht**
  die Round-27-`region_world`- und `apply_hits`-Laufzeitfunktionen.
- GitHub Actions: normale GCC-Kompilierung und UBSan-Ausführung
  auch für den 200.000-Fälle-Geometrietest.
- Der ursprüngliche Round-27-Filter für passive ITRs ist weiterhin die
  separate produktive Änderung und wird durch diesen Prototyp nicht ersetzt.

**Gemessener Host-Testbefund nach Ergänzung der Sonderfälle:**
1.903.983 von 3.399.592 potenziellen Narrow-Phase-Paarprüfungen
werden in synthetischen Szenen
ausgespart, mit identischen modellierten Treffern. Dieser Wert ist
**nicht** auf LF2-Originaldaten und **nicht** auf Vita-FPS übertragbar.
