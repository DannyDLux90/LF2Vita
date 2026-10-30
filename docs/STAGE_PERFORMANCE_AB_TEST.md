# LF2 Vita: kontrollierter Stage-Performance-Vergleich (WIP)

**Status:** Testprotokoll, kein Nachweis einer FPS-Verbesserung und keine Aussage über 100 % PC-Treue.

## Ausgangsbefund

Im Vita-Hardwarelog zur Round-24-Test-VPK sind langsame Phasen unter hoher Gegnerlast und direkt nach Unterabschnittswechseln zu sehen. Die bisherige Datenlage unterscheidet *zeitlich naheliegendes* Laden von belegten CPU-Engpässen noch nicht sicher. Die Round-27-Spielquelle ergänzt KI-/Objekt-/Kollisionszeit, wurde auf GitHub aber noch nicht synchronisiert.

## Vergleichsaufbau

1. Auf **derselben PS Vita** die bisherige Round-24-VPK als Referenz und später eine neue, eindeutig gekennzeichnete Test-VPK verwenden (nacheinander, nicht gleichzeitig).
2. Gleiche Spielkonfiguration: **Stage 1 und Stage 2, Easy, Henry, sieben CPU-Mitspieler** (John, Julian, Rudolf, Louis, Firen, Freeze, Bat), gleiche CPU-Kommandos. Keine absichtlich abweichende Schwierigkeit oder Figurenauswahl.
3. Für die Referenz und den Kandidaten jeweils mehrmals bis zu einer dicht besetzten Welle spielen. **Pausen, Menüwechsel, Unterbrechungen, Ladehänger und Szenenwechsel separat protokollieren**. Nie eine Aufnahme mit vielen Pausen ungeprüft als normale Spielleistung bewerten.
4. Nach jedem Durchlauf `ux0:data/LF2V00001/lf2.log` sichern und den Build-Tag notieren. Keine Original-Spielpakete, Assets oder personenbezogenen Pfade im öffentlichen GitHub hochladen.
5. Die Messungen nach Stage-/Phasenstand, Gegnerzahl und gleichzeitigem Nachladen trennen. Die CPU-Kommandos aus Round 26, Trefferregeln, Special-Animationen und 30-Hz-Simulation müssen beim Vergleich unverändert bleiben.

## Auswertung auf dem PC

```sh
python3 tools/analyze_stage_perf_log.py lf2_round24.log \
  --json round24_summary.json --csv round24_samples.csv

python3 tools/analyze_stage_perf_log.py lf2_candidate.log \
  --baseline lf2_round24.log \
  --min-comparison-samples 3 \
  --json candidate_vs_round24.json --csv candidate_samples.csv
```

Die Vergleichsfunktion untersucht **nur saubere Fenster ohne kürzlich protokollierten Textur-Ladevorgang/Abschnittswechsel** und gruppiert nach 0–10, 11–20, 21–30, 31–40 und 41+ Gegnern. Dies ist eine **explorative** Auswertung: Gleiche Gegnerzahl allein bedeutet nicht identische Kampfaktionen. Angaben zu `last_reported_cdram_bytes` können veraltet sein; fehlende Detailzeiten in älteren Logs bleiben ausdrücklich fehlend.

## Freigabekriterien für Performance-Korrekturen

- Die Simulationsrate ist in vergleichbaren 31–40-Gegner-Intervallen möglichst stabil bei den **vorgesehenen 30 Schritten/s**.
- Keine Änderung der PC-DAT-Animationszeiten, KI-Trefferregeln, Blockschadenswerte, RNG-Reihenfolge oder der wirksamen Gegnerzahl lediglich für bessere FPS.
- Kein Verlust von P1, CPU-Begleitern, Effekten, Grafiksätzen oder Gegnern beim Stage-Wechsel; VPK-Hardwaretest muss weiterhin die Phasenfolge vollständig durchlaufen.
- Beim Unterabschnittswechsel Texturladevorgänge und CDRAM-Auslastung getrennt prüfen. **Keine vermeintliche CPU-Optimierung aufgrund eines reinen Ladehänger-Intervalls**.
- Native C-Tests, sämtliche bisher vorhandenen Regressionstests und ein VitaSDK-Crossbuild müssen bestanden sein, bevor eine weitere Test-VPK verteilt wird.

## Einschränkung

Die aktuelle GitHub-Branch `tooling/round28-stage-perf-audit` enthält die **Host-Analysetools**, nicht die neue Spielengine. GitHub-`main` ist noch älter als die lokale Round-27-Quelle. Deshalb führt ein erfolgreicher Test dieser Branch weder zu einer neuen VPK noch zu geänderter Vita-Spielperformance.
