# VitaSDK-Archiv vor einem Offline-Build prüfen

Der Nutzer hat erneut `Vita SDK(3).zip` bereitgestellt. Diese Datei selbst ist **nicht** Teil des öffentlichen GitHub-Repositories.

```bash
python3 tools/inspect_vitasdk_bundle.py 'Vita SDK(3).zip'
```

Das Programm arbeitet offline mit der Python-Standardbibliothek und verändert oder installiert nichts. Es durchsucht die äußere ZIP und enthaltene Debian-Archive (`.deb`, insbesondere `data.tar.gz` / `data.tar.xz`) nach `arm-vita-eabi-gcc`, `vita-make-fself`, `vita-pack-vpk` und `vita.toolchain.cmake`. Die Ausgabe ist JSON mit `crosscompiler_present`, `packaging_tools_present`, `cmake_toolchain_present` und gegebenenfalls `inspection_errors`.

**Wichtig:** Eine positive Archivprüfung belegt nur das Vorhandensein der Werkzeugdateien. Ob die hostseitigen Linux-Binaries mit der vorhandenen Architektur, den dynamischen Bibliotheken und dem Projekt wirklich funktionieren, muss ein tatsächlicher `--version`-Aufruf und ein VitaSDK-Crossbuild nachweisen.

Ein vollständiger Wiederaufbau des letzten **Round-27**-Spiels braucht außerdem:
- den vollständigen lokalen Round-27-Quellstand samt `CMakeLists.txt`;
- VitaSDK-Header, -Libraries, CMake-Toolchain und die Packager;
- die eigene lokale VPK bzw. legal vorhandene Spieldaten, **nicht** zum Commit in ein öffentliches GitHub-Repository bestimmt.

Die vorhandene `tools/rebuild_offline_vpk.sh` aus dem Round-24/27-Quellarchiv ist der dokumentierte Offline-Buildweg; dieses neue Tool ersetzt den Buildvorgang nicht. Bei abweichenden SDK-Paketformaten, etwa `data.tar.zst`, meldet der reine Standardbibliothek-Scanner die nicht lesbaren Pakete. Fehlende Funde sind dann kein Beweis für ein fehlendes Compiler-Binary.

Das derzeitige GitHub-PR #3 fügt nur Host-Analyse- und SDK-Prüfwerkzeuge hinzu. Die lokalen Round-27-Spielquellen sind noch nicht nach GitHub synchronisiert. Es gibt hier **keine** neue VPK und **keinen** behaupteten Hardware-Erfolg.
