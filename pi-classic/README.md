# Pi Classic: QNX/ARM adaptation

Pi is upstream's coding agent. This directory supplies compatibility code for Pi **0.74.2**, Node **22.0.0 QNX/ARM**, BerryCore curl and a 2 GB Classic. Keep the pinned versions; the contemporary Pi 1.3.0 requires a newer Node version than this port.

## Host build

Use a modern Node/npm host (tested with Node 22.22.3; Node 24 also meets the build dependencies), Python 3, and a fresh checkout:

```sh
npm ci --prefix pi-classic/package --ignore-scripts --omit=optional
npm ci --prefix pi-classic/build-tools --ignore-scripts
node pi-classic/transform-regex.cjs
node pi-classic/test-unicode.mjs
node pi-classic/test-curl-fetch.mjs
node pi-classic/test-guard.mjs
node pi-classic/bundle.cjs
```

The transform saves upstream originals under pi-classic/upstream and generates a changed-file report in pi-classic/build. The bundle preserves the upstream package's runtime asset paths. Copy `main-bundle.mjs` to `package/node_modules/@earendil-works/pi-coding-agent/dist/bb10-main.mjs` before transferring the package tree. The bundle alone is not a complete install; package metadata, assets, external modules and the wrappers remain required.

## Device layout

Under `/accounts/1000/shared/misc/pi-classic/` place:

- `package/` (the entire prepared runtime npm tree, including bb10-main.mjs)
- `compat.mjs`, `curl-fetch.mjs`, `entry.mjs`, `pi-launch.sh`, `classic-guard.js`, `AGENTS-phone.md`
- `node/bin/node.bin` and its port libraries, obtained separately from [BerryCore's Node port](https://github.com/sw7ft/BerryCore/blob/main/ports/lang-node-term49-22.0.0.zip)

The tested Node archive SHA-256 is `6b6ca11c70eaf110ec1d305fda762979d48702a581328455404c7444279d4744`. `install-node.py` expects it at `/accounts/1000/shared/documents/pi-node-term49.zip`, validates the hash and refuses an existing node destination. `install-package.py` similarly expects `/accounts/1000/shared/documents/pi-package.zip` with package contents at its ZIP root, and refuses an existing package directory. These are device-side Python 3.2 helpers, run with system library isolation; they are not host programs.

If BerryCloud is already configured for the tested GLM endpoint, `configure.py` copies its credential **on the device only** into private Pi settings. It refuses existing Pi config files. Review it before use: it is provider-specific and is not a generic migration tool. `install-settings.py` installs the backup/confirmation extension and project guidance; `install-entry.py` creates the `pi` launcher and default project, refusing an unrelated command. Use `LD_LIBRARY_PATH=/base/lib:/base/usr/lib /base/usr/bin/python3.2 script.py` in the Term49 private HOME.

Start with `pi`, resume with `pi -c`, select a saved session with `pi -r`. Startup was about 42 seconds on the recorded device. Read/write/edit/bash and resume were verified, including a later physical Chinese input round trip.

## Limits and rollback

No WebAssembly image processing; other providers and extensions are not accepted. The guard backs up modified files and confirms selected operations; it is not a security sandbox. Prices set to zero in provider metadata mean cost accounting is unavailable, not that API use is free. Keep credentials, private session files and backups outside Git.

For a fresh install, disable only the `pi` launcher you installed to return to the previous environment. The existing BerryCore and AI commands are independent. Preserve `~/.pi` sessions/backups and project data. For an upgrade, retain the previous whole pi-classic directory and compare configs before swapping; the supplied historical install helpers intentionally refuse blind replacement.
