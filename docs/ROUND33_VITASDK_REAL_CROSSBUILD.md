# Round 33 – echter VitaSDK-Crossbuild und reproduzierbarer Stage-Adapter-Einbau

**Stand: 30. Oktober 2026.** Hier wurden keine neuen VPKs erstellt und keine proprietären Original-Spielassets übertragen.

## Neu nachgewiesen

Der bisherige konservative `lf2_stage_pair_runtime.inc` war nur mit nativen Mock-DAT-Strukturen getestet. In Round 33 gelang erstmals eine **vollständige echte ARM-Vita-Engine-Kompilierung und ELF-Linkung** mit dem aktivierten Filter.

Ein früher CI-Versuch scheiterte an einem bislang übersehenen Fragment-Sonderfall:
`src/fix3/game_15.inc` endet **mitten in einer offenen Zeichenfunktion**.
Eine Funktionsdefinition unmittelbar nach `#undef apply_hits` in `src/game.c` ist damit **nicht** auf C-Top-Level, selbst wenn sie beim Lesen der Include-Liste so aussieht. GCC meldete `invalid storage class for function`.

Die korrigierte Struktur lautet daher:

1. **An einem nachweislich freien Top-Level-Punkt nach `game_02_prefix.inc` / `#undef in_action`** eine frühe Funktionsdeklaration und das Stage-Kandidat-Makro einfügen. Im sicheren Standardzustand expandiert das Makro zu `1`; bei ausdrücklicher `--enable`-Aktivierung auf den Kandidaten.
2. Nur das bekannte `apply_hits`-Kommando der inneren **Stage-Schleife** durch die zusätzliche Bedingung absichern; VS- und übrige Modi nicht verändern.
3. Erst **nach dem vollständig abgeschlossenen `game_24.inc`** den echten Adapterkörper inkludieren. Davor liegt die Stage-Funktion noch offen, danach gibt es eine zulässige C-Top-Level-Grenze.
4. `--undo --apply` entfernt nur einen exakt erkannten vollständigen Einbau, stellt sämtliche Originaldateien wieder her und verweigert Änderungen an fremdem Zustand.

Der korrigierte `tools/apply_round31_broadphase.py` implementiert diese zwei Einfügepositionen. Alle **7 Installations-/Rücknahmetests** bestehen erneut.

## Zwei echte Crossbuild-Nachweise (isolierter älterer Quellzweig)

- **Experiment-Zweig:** `experiment/round33-vita-stage-broadphase-compile`
- **Offizieller VitaSDK-Container:** `vitasdk/vitasdk:2026.08` mit `arm-vita-eabi-gcc 15.2.0`
- **A:** [GitHub Actions 41125750730](https://github.com/DannyDLux90/LF2Vita/actions/runs/41125750730): nach Korrektur der C-Einfügegrenzen komplettes `lf2vita`-Target kompiliert/gelinkt; SUCCESS.
- **B:** [GitHub Actions 41126074106](https://github.com/DannyDLux90/LF2Vita/actions/runs/41126074106): neu hinzugefügtes Workflow-Skript `.github/workflows/stage-adapter-vita-crossbuild.yml` aus der Tooling-Branch holt isoliert die historische Quellenbranch `pc-compat-round6-pc-fidelity`, installiert via **echtem Einbauhelfer** (`--apply --enable`), kompiliert `lf2vita` mit dem selben SDK, stellt CMake-/Source-Dateien zurück und bestätigt `git diff --exit-code` plus keine untracked Dateien; SUCCESS.
- Zusätzlicher paralleler [Actions-Run 41126082813](https://github.com/DannyDLux90/LF2Vita/actions/runs/41126082813): SUCCESS.

Bei diesen Tests wird der `vita_create_vpk`-Aufruf *nur im temporären CMake-Workspace* deaktiviert, weil die ursprünglichen Spielassets nicht in der öffentlichen Branch liegen. Es wird **kein VPK-Paket** und kein alter öffentlicher Release erstellt.

## Was noch NICHT nachgewiesen ist

- **Das vollständige Round-27-Archiv aus der Chat-Unterhaltung** war aufgrund wiederkehrender Container-/Python-Verbindungsfehler nicht zugänglich für Programm-Ausführung; die Tests arbeiten auf einer **älteren historischen Spiel-Engine-Branch**. Weder Round 25/26 Steuerungs-/Flaschen-Korrekturen noch Round 27 Performance-Messfelder wurden hier als Gesamtprojekt neu crosskompiliert.
- Das neu hochgeladene `Vita SDK(4).zip` wurde wegen derselben lokalen Ausführungsstörung **nicht direkt geöffnet**. Die erfolgreichen Builds nutzten den öffentlichen, auf 2026.08 festgelegten VitaSDK-Docker-Container.
- Die synthetischen Kontakt-/Geometrietests und der reale **Compiler** zeigen keine tatsächliche PC-Kampf-Parität, sondern mechanische Sicherheit gegen bestimmte False Negatives und C-/Linkbarkeit auf dem älteren Stand.
- **Kein Benchmark:** Eine mögliche CPU-Zeitverbesserung gegenüber den zusätzlichen Kosten für das Bounding-Box-Berechnen ist erst mit `ai_us_per_tu`, `hits_us_per_tu`, Renderzeiten und der tatsächlichen Round-27-Engine messbar. Es gibt noch keinen belegten FPS-Gewinn.

## Nächster Schritt

Sobald die lokale Ausführungsumgebung die Round-27-Archivdatei und das hochgeladene SDK zugänglich macht: Einbauhelfer als Dry-Run auf den **wirklichen Round-27-Quellstand** ausführen; Diff prüfen, Off- und On-Build gegen exakt denselben aktuellen Source-Snapshot erstellen, 46/46 Round-27-Regressionstests und originaldatenbasierte Stage-/AdHoc-Vergleiche fahren und die CPU-Kosten vergleichen.

Erst wenn der Performancegewinn **und** die deterministische PC-/Vita-Treue auf dem aktuellen Quellstand nachvollziehbar sind, ist ein weiterer gezielter Vita-Hardwaretest als VPK gerechtfertigt.
