# Game data

`game.lf2pak` is generated locally from a user-provided Little Fighter 2 2.00a installation/archive and is intentionally not committed.

Generate it with:

```sh
python3 tools/make_gamepak.py /path/to/LittleFighter.zip gamepack/game.lf2pak
```

The pack keeps all original 2.00a files in one indexed/CRC-checked container to make VitaShell installation much faster than installing hundreds of tiny VPK entries.
