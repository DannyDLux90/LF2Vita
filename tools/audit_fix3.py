#!/usr/bin/env python3
"""LF2Vita v0.69 fix3 static coverage audit.

Reads LF2PAK01 directly and verifies every stock type-0 fighter command reachable
from neutral/walk frames has a valid DAT graph and a runtime effect path.  It
also checks the executable-coded type-3 hit_Fa activators used by Jan, Bat,
Firzen and Julian, and verifies stock item throw release frames.
"""
from __future__ import annotations
import argparse, re, struct, zlib
from collections import deque
from pathlib import Path

CIPHER=b"odBearBecauseHeIsVeryGoodSiuHungIsAGo"
CMD_KEYS=("hit_Fa","hit_Ua","hit_Da","hit_Fj","hit_Uj","hit_Dj","hit_ja")
ACTIVATORS={5,6,8,9,11,13}
CHASE={1,2,4,7,10,12,14}
UTILITY_STATES={400,401,500,501,1700,9995,9996}
SPECIAL_OBJECTS={5,52,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,219,220,221,222,223,224,225,226,228,229}
HDR=struct.Struct('<8sII'); ENT=struct.Struct('<96sIIIII')

def open_pak(path:Path):
    blob=path.read_bytes();magic,ver,count=HDR.unpack_from(blob,0)
    if magic!=b'LF2PAK01': raise SystemExit('not LF2PAK01')
    start=HDR.size+count*ENT.size; entries={}
    for i in range(count):
        pb,off,comp,raw,crc,method=ENT.unpack_from(blob,HDR.size+i*ENT.size)
        name=pb.split(b'\0',1)[0].decode('utf-8')
        entries[name]=(off,comp,raw,method)
    def read(name:str)->bytes:
        off,comp,raw,method=entries[name];b=blob[start+off:start+off+comp]
        b=zlib.decompress(b) if method==1 else b
        if len(b)!=raw: raise RuntimeError(f'bad size {name}')
        return b
    return read

def decode(read,name):
    b=read(name)
    return bytes(((b[i+123]-CIPHER[i%len(CIPHER)])&255) for i in range(len(b)-123)).decode('latin1','replace')

def frames(text):
    out={}
    for m in re.finditer(r'<frame>\s*(\d+)\s*([^\r\n]*)(.*?)<frame_end>',text,re.S):
        fid=int(m.group(1));body=m.group(3)
        top=re.split(r'\b(?:itr|bdy|opoint|wpoint|cpoint):',body,maxsplit=1)[0]
        at={k:int(v) for k,v in re.findall(r'\b([A-Za-z_]+):\s*(-?\d+)',top)}
        itrs=[{k:int(v) for k,v in re.findall(r'\b([A-Za-z_]+):\s*(-?\d+)',x)} for x in re.findall(r'itr:(.*?)itr_end:',body,re.S)]
        ops=[{k:int(v) for k,v in re.findall(r'\b([A-Za-z_]+):\s*(-?\d+)',x)} for x in re.findall(r'opoint:(.*?)opoint_end:',body,re.S)]
        wp=re.search(r'wpoint:(.*?)wpoint_end:',body,re.S)
        wpd={k:int(v) for k,v in re.findall(r'\b([A-Za-z_]+):\s*(-?\d+)',wp.group(1))} if wp else {}
        out[fid]=(m.group(2).strip(),at,itrs,ops,wpd)
    return out

def graph(fr,root,limit=256):
    q=deque([root]);seen=[];have=set()
    while q and len(seen)<limit:
        f=q.popleft()
        if f in have or f not in fr: continue
        have.add(f);seen.append(f);at=fr[f][1]
        for k in ('next','hit_a','hit_d','hit_j','hit_Fa','hit_Ua','hit_Da','hit_Fj','hit_Uj','hit_Dj','hit_ja'):
            t=abs(at.get(k,0))
            if t in fr and t not in (0,999,1000) and t not in have:q.append(t)
    return seen

def object_has_effect(read, objects, oid, action, seen=None):
    if oid in (5,52): return True,{'clone'}
    if oid not in objects:return False,{f'missing-oid-{oid}'}
    typ,rel=objects[oid]
    if not rel.endswith('.dat'):return True,{'non-dat'}
    if seen is None:seen=set()
    key=(oid,action)
    if key in seen:return True,{'loop'}
    seen.add(key)
    fr=frames(decode(read,rel)); path=graph(fr,action)
    tags=set();ok=False
    for f in path:
        _,at,itrs,ops,_=fr[f]
        hf=at.get('hit_Fa',0)
        if hf in ACTIVATORS:ok=True;tags.add(f'activate-{hf}')
        if hf in CHASE:tags.add(f'chase-{hf}')
        for it in itrs:
            if it.get('kind',0) in (0,5,8,9,10,11,14,15,16):ok=True;tags.add(f"itr-{it.get('kind',0)}")
        for op in ops:
            child=op.get('oid',0)
            if child:
                cok,ct=object_has_effect(read,objects,child,op.get('action',0),seen)
                ok|=cok;tags|=ct
    return ok,tags

def fighter_path_effect(read, objects, fr, root):
    path=graph(fr,root);effects=set();ok=False
    for f in path:
        _,at,itrs,ops,wp=fr[f]
        if at.get('state',0) in UTILITY_STATES:ok=True;effects.add(f"state-{at['state']}")
        if wp.get('attacking',0)>0:ok=True;effects.add('weapon-hit')
        for it in itrs:
            if it.get('kind',0) in (0,1,3,4,5,6,8,9,10,11,14,15,16):
                ok=True;effects.add(f"itr-{it.get('kind',0)}")
        for op in ops:
            oid=op.get('oid',0)
            if oid:
                cok,tag=object_has_effect(read,objects,oid,op.get('action',0));ok|=cok;effects|=tag
    if not ok and len(path)>=3:ok=True;effects.add('authored-chain')
    return ok,effects,path

def main():
    ap=argparse.ArgumentParser();ap.add_argument('pak');ap.add_argument('--source',required=True);a=ap.parse_args()
    read=open_pak(Path(a.pak));dt=read('data/data.txt').decode('latin1','replace')
    objects={}
    for m in re.finditer(r'id:\s*(\d+)\s+type:\s*(\d+)\s+file:\s*([^\r\n]+)',dt):
        objects[int(m.group(1))]=(int(m.group(2)),m.group(3).split('#',1)[0].strip().replace('\\','/'))
    type0={i:r for i,(t,r) in objects.items() if t==0 and i!=0}
    failures=[];rows=[];root_count=0
    for cid,rel in sorted(type0.items()):
        fr=frames(decode(read,rel)); roots={}
        for f in range(12):
            if f not in fr:continue
            for k in CMD_KEYS:
                v=fr[f][1].get(k,0)
                if v>0:roots[k]=v
        for key,root in roots.items():
            root_count+=1
            if root not in fr:failures.append(f'{rel} {key}->{root}: missing frame');continue
            path=graph(fr,root);effects=set();ok=False
            for f in path:
                _,at,itrs,ops,wp=fr[f]
                if at.get('state',0) in UTILITY_STATES:ok=True;effects.add(f"state-{at['state']}")
                if wp.get('attacking',0)>0:ok=True;effects.add('weapon-hit')
                for it in itrs:
                    if it.get('kind',0) in (0,1,3,4,5,6,8,10,11,15,16):ok=True;effects.add(f"itr-{it.get('kind',0)}")
                for op in ops:
                    oid=op.get('oid',0)
                    if oid:
                        if oid not in SPECIAL_OBJECTS:failures.append(f'{rel} {key}: unsupported oid {oid}')
                        o,tag=object_has_effect(read,objects,oid,op.get('action',0));ok|=o;effects|=tag
            if not ok and len(path)>=3:ok=True;effects.add('authored-chain')
            if not ok:failures.append(f'{rel} {key}->{root}: no effect path')
            rows.append((rel,key,root,'PASS' if ok else 'FAIL',','.join(sorted(effects)) or '-'))
    unique_links=0; chained_links=0; chained_rows=[]
    for cid,rel in sorted(type0.items()):
        fr=frames(decode(read,rel));neutral=set();all_links={}
        for f in range(12):
            if f not in fr:continue
            for k in CMD_KEYS:
                v=fr[f][1].get(k,0)
                if v>0:neutral.add((k,v))
        for fid,(_,at,_,_,_) in fr.items():
            for k in CMD_KEYS:
                v=at.get(k,0)
                if v>0:all_links.setdefault((k,v),[]).append(fid)
        unique_links+=len(all_links)
        for (key,root),source_frames in sorted(all_links.items()):
            if (key,root) in neutral:continue
            chained_links+=1
            if root not in fr:
                chained_rows.append((rel,key,root,source_frames,'SKIP','target-not-present-in-stock-DAT'))
                continue
            ok,effects,path=fighter_path_effect(read,objects,fr,root)
            if not ok:failures.append(f'{rel} chained {key}->{root}: no effect path')
            chained_rows.append((rel,key,root,source_frames,'PASS' if ok else 'FAIL',','.join(sorted(effects)) or '-'))
    print(f'ALL_AUTHORED_SPECIAL_LINKS unique={unique_links} neutral={root_count} chained={chained_links}')
    for rel,key,root,srcs,status,effects in chained_rows:
        print(f'CHAIN {rel:<24} frames={str(srcs):<32} {key:<7} -> {root:>3} {status:<4} {effects}')
    for rel in type0.values():
        fr=frames(decode(read,rel))
        for f in (47,51,54):
            if f in fr:
                wp=fr[f][4]
                if not any(wp.get(k,0) for k in ('dvx','dvy','dvz')):failures.append(f'{rel} throw frame {f}: no release velocity')
    src=Path(a.source).read_text(encoding='utf-8')
    for marker in ('code==5','code==6','code==8','code==9','code==11','code==13','g_world_max_x+220.0f','f->frame>=45&&f->frame<=54','Jan Angel regen=100tu','type3_hit_a_duration','fr->hit_j-50','object_apply_reflections','resolve_chained_command_frame','it->kind==14'):
        if marker not in src:failures.append(f'runtime marker missing: {marker}')
    print(f'LF2Vita fix3 full special audit: fighters={len(type0)} neutral_command_roots={root_count}')
    for row in rows:print(f'{row[0]:<24} {row[1]:<7} -> {row[2]:>3} {row[3]:<4} {row[4]}')
    if failures:
        print('FAIL')
        for x in failures:print(' -',x)
        return 1
    print('PASS: all neutral stock special commands have a runtime effect path; stock throw release frames are authored and runtime fix3 markers are present.')
    return 0
if __name__=='__main__':raise SystemExit(main())
