#!/usr/bin/env python3
"""Host-only: retrieve hash-pinned upstream prerequisites into an ignored cache."""
import argparse, hashlib, json, pathlib, shutil, subprocess, tarfile, tempfile, urllib.request, zipfile
root=pathlib.Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--pinyin-only',action='store_true');args=parser.parse_args()
cache=root/'.cache';cache.mkdir(exist_ok=True)
spec=json.loads((root/'dependencies.json').read_text())
for name,item in spec.items():
    if args.pinyin_only and name!='pinyin':continue
    target=cache/item['filename']
    if not target.exists():
        with urllib.request.urlopen(item['url'],timeout=60) as response:data=response.read()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError('Hash mismatch: '+name)
        target.write_bytes(data)
    if hashlib.sha256(target.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Cached hash mismatch: '+name)
    print('Verified',name)
    if name=='term49':
        dest=cache/item['root']
        if not dest.exists():
            with zipfile.ZipFile(target) as archive:
                for entry in archive.infolist():
                    p=pathlib.PurePosixPath(entry.filename)
                    if p.is_absolute() or '..' in p.parts:raise RuntimeError('Unsafe archive member')
                    # Copy regular files only; never create archive-provided links.
                    if entry.is_dir():continue
                    if (entry.external_attr>>16)&0o170000==0o120000:raise RuntimeError('Unexpected archive link')
                archive.extractall(cache)
    elif name=='pinyin':
        dest=cache/'libgooglepinyin-0.1.2'
        if not dest.exists():
            with tempfile.TemporaryDirectory(dir=cache) as td:
                stage=pathlib.Path(td)
                with tarfile.open(target) as archive:
                    for entry in archive.getmembers():
                        p=pathlib.PurePosixPath(entry.name)
                        if p.is_absolute() or '..' in p.parts or not (entry.isdir() or entry.isfile()):raise RuntimeError('Unsafe source member')
                    archive.extractall(stage)
                folder=stage/'libgooglepinyin-0.1.2'
                subprocess.run(['patch','--batch','-p1','-i',str(root/'googlepinyin-qnx.patch')],cwd=folder,check=True)
                shutil.move(str(folder),str(dest))
print('Dependencies ready; nothing installed on a device.')
