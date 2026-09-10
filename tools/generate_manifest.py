#!/usr/bin/env python3
import sys, zlib
from pathlib import Path
root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2])
rows=[]
for p in sorted(x for x in root.rglob('*') if x.is_file()):
    rel=p.relative_to(root).as_posix()
    data=p.read_bytes()
    rows.append(f"{len(data)}\t{zlib.crc32(data)&0xffffffff:08x}\t{rel}\n")
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(''.join(rows), encoding='utf-8')
print(f"manifest: {len(rows)} files, {sum(int(r.split(chr(9))[0]) for r in rows)} bytes")
