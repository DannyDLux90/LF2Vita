#!/usr/bin/env python3
import os, sys, struct, zlib, zipfile
from pathlib import Path

MAGIC=b'LF2PAK01'
PATH_LEN=96
ENTRY=struct.Struct('<96sIIIII')
HDR=struct.Struct('<8sII')

def pack_payload(raw):
    comp=zlib.compress(raw,9)
    return (comp,1) if len(comp)+8 < len(raw) else (raw,0)

def normalize_zip_name(name):
    name=name.replace('\\','/').lstrip('/')
    parts=[p for p in name.split('/') if p and p!='.']
    if parts and parts[0].lower()=='littlefighter': parts=parts[1:]
    if any(p=='..' for p in parts): raise SystemExit(f'unsafe path in zip: {name}')
    return '/'.join(parts)

def collect(source):
    source=Path(source)
    files=[]
    if source.is_file() and zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as z:
            for zi in sorted(z.infolist(),key=lambda x:x.filename.lower()):
                raw_name=zi.filename.replace('\\','/')
                if zi.is_dir() or raw_name.startswith('__MACOSX/') or '/._' in raw_name or raw_name.endswith('/.DS_Store') or raw_name.endswith('.DS_Store'): continue
                rel=normalize_zip_name(raw_name)
                if not rel: continue
                raw=z.read(zi)
                data,method=pack_payload(raw)
                files.append((rel,raw,data,method,zlib.crc32(raw)&0xffffffff))
    else:
        root=source
        if (root/'LittleFighter').is_dir(): root=root/'LittleFighter'
        for p in sorted(root.rglob('*')):
            if p.is_file():
                rel=p.relative_to(root).as_posix()
                raw=p.read_bytes(); data,method=pack_payload(raw)
                files.append((rel,raw,data,method,zlib.crc32(raw)&0xffffffff))
    return files

def main(source,out):
    files=collect(source)
    if not files: raise SystemExit('no game files found')
    for rel,*_ in files:
        if len(rel.encode('utf-8'))>=PATH_LEN: raise SystemExit(f'path too long: {rel}')
    off=0
    Path(out).parent.mkdir(parents=True,exist_ok=True)
    with open(out,'wb') as f:
        f.write(HDR.pack(MAGIC,1,len(files)))
        for rel,raw,data,method,crc in files:
            rb=rel.encode('utf-8')+b'\0'*(PATH_LEN-len(rel.encode('utf-8')))
            f.write(ENTRY.pack(rb,off,len(data),len(raw),crc,method)); off+=len(data)
        for _,_,data,_,_ in files: f.write(data)
    raw_total=sum(len(x[1]) for x in files); packed=os.path.getsize(out)
    print(f'{len(files)} files, raw={raw_total}, pak={packed}, ratio={packed/raw_total:.3f}')

if __name__=='__main__':
    if len(sys.argv)!=3: raise SystemExit('usage: make_gamepak.py LITTLEFIGHTER_DIR_OR_ZIP OUT')
    main(sys.argv[1],sys.argv[2])
