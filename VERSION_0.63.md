# LF2Vita 0.63

Hardware-test release focused on the missing LF2 opoint/projectile runtime and Championship round-result flow.

- Character DAT `opoint:` blocks now spawn stock 2.00a arrows, balls, chase objects, wind/blast/flame/column effects from their own DAT/BMP definitions.
- Projectile `itr` collision applies damage, knockback and authored effect flags.
- Henry's `facing: 50` attack creates five rendered arrows.
- New `OBJECT` log lines record object cache, spawn and hits.
- 1-on-1 and 2-on-2 Championship now show winner/defeated and next opponent/team after each human round.

Known remaining special-object gaps: Rudolf clone/transform semantics and held-weapon attachment (`opoint kind: 2`, e.g. Freeze's ice sword).
