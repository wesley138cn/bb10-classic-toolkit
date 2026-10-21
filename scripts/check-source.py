#!/usr/bin/env python3
"""Check publishable (Git-tracked or Git-eligible) source, without printing secrets."""
import ast, pathlib, re, subprocess
root=pathlib.Path(__file__).resolve().parents[1]
files=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=root).decode().split('\0')
fail=[];count=0
patterns=[rb'gh[pousr]_[A-Za-z0-9]{30,}',rb'github_pat_[A-Za-z0-9_]{40,}',rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',rb'(?<![A-Za-z0-9])sk-[A-Za-z0-9_-]{24,}']
for rel in sorted(set(filter(None,files))):
    p=root/rel
    if not p.is_file() or p.is_symlink():fail.append((rel,'not a regular file'));continue
    b=p.read_bytes();count+=1
    if any(re.search(pat,b) for pat in patterns):fail.append((rel,'credential-like content'))
    if (b'/' + b'Users/') in b or (b'/' + b'Volumes/') in b:fail.append((rel,'private workstation path'))
    if b'com.example.Term49.' in b and re.search(rb'com\.example\.Term49\.[A-Za-z0-9_-]{12,}',b):fail.append((rel,'device app identifier'))
    if len(b)>5*1024*1024:fail.append((rel,'unexpected large file'))
    if p.suffix=='.py':
        try:ast.parse(b,filename=rel)
        except SyntaxError:fail.append((rel,'Python syntax'))
    if p.suffix=='.md':
        for target in re.findall(r'\]\(([^)]+)\)',b.decode('utf-8')):
            if '://' in target or target.startswith('#'):continue
            if not (p.parent/target.split('#')[0]).exists():fail.append((rel,'broken relative link: '+target))
if fail:
    for rel,kind in fail:print(rel+': '+kind)
    raise SystemExit(1)
print('PASS:',count,'source files; relative links, Python syntax, path and credential-pattern checks')
