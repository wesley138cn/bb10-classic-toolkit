# Host checks

From the repository root, with Python 3, a C/C++ compiler and a modern Node/npm host (tested with Node 22.22.3; Node 24 also meets the build dependency requirements):

```sh
python3 -m unittest discover -s berrycloud -p 'test_*.py'
sh term49/test-host.sh
python3 term49/fetch-deps.py --pinyin-only
sh term49/test-host.sh --ime
npm ci --prefix pi-classic/package --ignore-scripts --omit=optional
npm ci --prefix pi-classic/build-tools --ignore-scripts
node pi-classic/transform-regex.cjs
node pi-classic/test-unicode.mjs
node pi-classic/test-curl-fetch.mjs
node pi-classic/test-guard.mjs
node pi-classic/bundle.cjs
python3 scripts/check-source.py
```

All API transports in the host tests are fakes or local loopback; no paid model call occurs. Dependency acquisition contacts upstream registries and source hosts. History and Pinyin host checks use ASan/UBSan. Leak detection is disabled for compatibility with Apple ASan; no LeakSanitizer result is claimed.

The QNX terminal cannot be fully compiled or visually accepted on the host. Device commands are in term49/README.md. Existing physical acceptance is explicitly separated from the publication checks in VALIDATION.md.
