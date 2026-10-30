# LF2 Vita – ein vollständiges Backup-ZIP erzeugen

**Stand: 30.10.2026.** Dieser Backup-Helfer speichert sechs Komponenten **unverändert in einem einzigen ZIP**. Der GitHub-Quellstand wird auf den Commit `e11ba19ccefc198304a62dabb564411eb0a79475` fixiert, damit der Inhalt reproduzierbar bleibt.

## Einmalige Vorbereitung (Windows)

1. Lege einen Ordner an, zum Beispiel `C:\LF2Backup`.
2. Lade die **fünf Originaldateien aus der ChatGPT-Unterhaltung** in diesen Ordner; weder die SDK-Datei noch Spielepakete werden auf GitHub veröffentlicht:
   - `LF2Vita_Round27_Vollstaendiger_Quellcode_2026-10-30.zip`
   - `LF2Vita-next-chat-source-workspace-2026-09-13.zip`
   - `LF2Vita_Round24_Stage_Performance_Test_2026-10-30.vpk`
   - `Vita SDK(4).zip`
   - `LF2Vita_Round27_Performance_Profiling.patch`
3. Lade `backup/create_lf2_complete_backup.ps1` aus diesem GitHub-Branch herunter und lege das Skript ebenfalls im Ordner ab.
4. Öffne PowerShell in diesem Ordner und führe aus:

```powershell
powershell -ExecutionPolicy Bypass -File .\create_lf2_complete_backup.ps1 -InputDirectory .
```

Bei PowerShell 7 statt `powershell` das Kommando `pwsh` verwenden.

Das Skript lädt den jüngsten experimentellen GitHub-Tooling-Stand **einmalig** als ZIP hinzu. Es installiert keinen Compiler, verändert keine Spielelemente und baut keine neue VPK. Falls kein Internet verfügbar ist, kann eine lokal bereitliegende, zuvor heruntergeladene GitHub-Commit-ZIP mit `-GitHubSnapshotPath .\tooling.zip` übergeben werden.

## Ergebnis

Die Ausgabedatei `LF2Vita_Gesamtbackup_2026-10-30.zip` liegt danach im gleichen Ordner. Sie enthält:
- den vollständig zusammengeführten **Round-27-Quellcode**;
- den älteren Workspace einschließlich der darin vorhandenen Projekt- und Ressourcendateien;
- die **letzte wirklich erstellte Round-24-VPK**;
- den hochgeladenen SDK-Stand 4;
- den Round-27-Patch;
- den fixierten GitHub-Stand mit Round-28–33-Prototypen und Tests;
- `00_README_Backup.txt`, `00_MANIFEST.json` und `00_SHA256SUMS.txt`.

Die Archivdatei umfasst **etwa 320 MB plus Metadaten**. Das Skript speichert bereits komprimierte Dateien ohne erneute Kompression und liest sämtliche ZIP-Einträge nochmals, um die SHA-256-Prüfsummen mit den Eingabedateien abzugleichen. Ein bereits vorhandenes Archiv gleichen Namens wird **nicht überschrieben**.

**Nicht enthalten und auch nicht behauptet:** Eine neue Round-33-VPK oder ein verifizierter Round-33-Hardware-Build. Für die jüngeren Runden gibt es nur die experimentellen Werkzeuge auf GitHub. Bitte eine ältere VPK nicht als neues Release kennzeichnen.

## Test

Der automatisierte GitHub-Actions-Lauf [41131196624](https://github.com/DannyDLux90/LF2Vita/actions/runs/41131196624) wurde mit **synthetischen, nicht proprietären kleinen Dateien** erfolgreich ausgeführt. Damit ist der Pack- und Prüfablauf getestet, aber noch nicht der tatsächlich aus rund 320 MB Nutzdaten bestehende Endstand in diesem Chat erstellt.