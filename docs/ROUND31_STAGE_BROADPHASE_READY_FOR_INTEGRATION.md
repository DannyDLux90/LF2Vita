# Round 31 – Stage-Broadphase: vorbereitete Integration (noch kein Vita-Release)

**Status am 30.10.2026:** Die aktuelle vollständige lokale Engine ist weiterhin **Round 27**, nicht GitHub-`main`. Die lokale Container-/Python-Ausführung ist ausgefallen, daher wurde der Einbau nicht gegen den tatsächlichen Benutzer-Quellcode/VitaSDK ausgeführt. Der Host-C-Adapter ist unabhängig getestet; **keine neue VPK und keine bestätigte FPS-Verbesserung**.

## Neue Quelldateien

- `tools/lf2_stage_pair_runtime.inc`: Echtzeit-Adapter für den bestehenden Stage-Paarloop. Es wird **vor jedem Paar die aktuelle Geometrie** von Angreifer-ITRs und Opfer-BDY aus `frame_def()` / `region_world()` ermittelt. Keine riskanten, veraltenden Bounds-Caches.
- `tools/lf2_stage_pair_bounds.h`: konservative Rechteck-Vereinigung mit 64-Bit-Endkoordinaten.
- `tools/apply_round31_broadphase.py`: Installationshelfer für den **vollständigen aktuellen** lokalen Spielcode, kontrolliert die bekannte Top-Level-Grenze nach `game_15.inc` und das exakte innere Paarloop-Muster in `game_24.inc`. Er verändert nur `src/game.c`, `src/fix3/game_24.inc` und erzeugt die zwei Quell-Includes; keine Assets, Stage-Daten, Steuerung oder Build-Konfiguration.
- `tests/native/test_stage_pair_runtime_adapter.c`: kompiliert den realen Kandidaten-Adapter mit kompatiblen Mock-Fighter-/DAT-Typen und prüft **100.000 Szenen jeweils vor/nach Zustandänderung** gegen eine ungefilterte Geometrieprüfung, also 200.000 Sicherheitsvergleiche.
- `tests/test_apply_round31_broadphase.py`: vier Tests zu Dry-Run, absichtlicher Aktivierung, Idempotenz und Ablehnung unbekannter/konfliktträchtiger Source-Layouts.

## Offline-Integration (erst nach Wiederherstellung der Arbeitsumgebung)

```sh
# Aus der GitHub-Tooling-Branch, im Verzeichnis mit tools/:
python3 tools/apply_round31_broadphase.py /pfad/zum/vollstaendigen/lf2_round27
# Zuerst nur die geplanten Änderungen anzeigen, noch nichts schreiben.

python3 tools/apply_round31_broadphase.py /pfad/zum/vollstaendigen/lf2_round27 --apply
# Auslieferungsschutz: Filter bleibt damit AUS, Originalverhalten bleibt erhalten.

# NUR für separaten gezielten Quell-/SDK-/Hardware-Vergleich:
python3 tools/apply_round31_broadphase.py /pfad/zum/vollstaendigen/lf2_round27 --apply --enable

# Falls ein Test Abweichungen zeigt: zuerst trocken prüfen, dann vollständig zurücknehmen:
python3 tools/apply_round31_broadphase.py /pfad/zum/vollstaendigen/lf2_round27 --undo
python3 tools/apply_round31_broadphase.py /pfad/zum/vollstaendigen/lf2_round27 --undo --apply
```

**Wichtig:** Die lokale Round-27-Quelle enthält bereits den Round-26-Stage-D-Pad-Umbau, den Trink-Fix, Round-27-Angriffsfilter, Rendering-Anpassungen und Stage-Hardware-Diagnosen. Der Host-Test im GitHub-CI verwendet als Dateivorlage eine **synthetisch um den neueren Include-Abschluss ergänzte ältere Haupt-Branch**, **nicht** die echte Round-27-Datei. Deshalb folgt aus dem erfolgreichen Host-Test **keine** erfolgreiche reale Source-Integration.

## Erforderliche vollständige Validierung vor Aktivierung

1. Git-/Datei-Diff gegen **wirkliche** Round-27-Quellen; es darf ausschließlich die eine Stage-Paarloop-Zeile konditionieren. Sonstige Modi und Steuerung bleiben unverändert.
2. Einen **VitaSDK-Crossbuild** mit Filter ausgeschaltet und einen mit Filter eingeschaltet durchführen. Alle 46 Round-27-Regressionen müssen weiterhin erfolgreich sein, dazu native Szenentests.
3. **Laufzeit-Paritätsvergleich** mit deterministischen Eingaben: gleiche Treffer-Ereignisse und Reihenfolge, alle P1/CPU/Enemy-HP-Werte, Flaschen- und Objektzustände, Catch, kind-6/Super-Punch, Spezialattacken, held weapons, Freeze/Effekt, Hitlag, RNG und netzwerkrelevanten Zustandshash prüfen.
4. Mit **gleichen 8-/40-Gegner-Szenarien** und ohne synchrone Nachladungen messen, ob die zusätzlichen Bounds-Berechnungen überhaupt Zeit sparen. Der Filter könnte in bestimmten Szenen auch langsamer sein.
5. Stage-Abschnittswechsel und CDRAM-Verbrauch separat optimieren. Der räumliche Filter löst **keine** Texturlade-/Speicherprobleme.

## Sicherheitsprinzipien

- Unbekannte/ungültige Positionen, fehlende Frames, leere/ungewöhnliche Rechteckdaten oder `held_oid != 0` werden **nicht herausgefiltert**. Der alte `apply_hits`-Pfad bleibt erreichbar.
- Zusätzlich werden alle **Zwischenergebnisse** der originalen Ganzzahlkoordinaten-Berechnung überprüft, nicht nur die endgültigen Weltkoordinaten. Sonst könnte ein int-Überlauf vor dem anschließenden Ausgleich schon undefiniertes C-Verhalten erzeugen.
- Der Rücknahmebefehl akzeptiert nur den exakt erwarteten eingebauten Code und **bytegenau unveränderte** generierte Include-Dateien. Fremde lokale Modifikationen werden niemals überschrieben bzw. gelöscht. Nach erfolgreicher Rücknahme stimmen `src/game.c` und `game_24.inc` bytegenau mit dem Original vor dem Einbau überein.
- Ein normaler Schlag kann eine passive `ITR kind 6` am Ziel treffen und dadurch in Super-Punch Frame 70 umschalten, auch bei räumlich disjunkten Ziel-BDYs. Der Adapter berücksichtigt das ausdrücklich.
- Keine neue Gegnerbegrenzung, kein schnelleres/abweichendes Kampf-`wait`, keine veränderte Simulationsfrequenz.
- Die Round-27-Kompatibilität muss im echten Quellstand erst bestätigt werden; bei unbekannter Stage-Paarloop stoppt der Einbauhelfer absichtlich.

## Bisherige Host-Nachweise

GitHub Actions **41120193869** auf Commit `b2b6bd79`: **SUCCESS**.
8 Performance-Analyzer-Tests + 4 SDK-Scanner-Tests + **7 Installations-/Rücknahme-Sicherheitstests** sowie native GCC/UBSan-Kollisionsszenen und 200.000 Adapter-Vergleiche. Ein zusätzlicher CI-Test holt den **echten historischen** Zweig `pc-compat-round6-pc-fidelity`, installiert/aktiviert den Filter und nimmt ihn wieder zurück; anschließend verifiziert er die Bytegleichheit der ursprünglichen Haupt- und Stage-Dateien. Diese historische Quellenprobe ist **kein** Round-27-Build. Der Native-C-Adaptertest nutzt Mock-Strukturen, nicht die vollständige Vita-Engine.

**Keinen öffentlich verfügbaren PR auf GitHub mit dem aktuellsten Spielquellstand verwechseln:** PR #3 ist eine Tooling-/Prototyp-Branch.
