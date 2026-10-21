#!/bin/sh
# Isolated Pi installation for BlackBerry Classic. Existing AI commands are unchanged.
pi_root=/accounts/1000/shared/misc/pi-classic
core_root=/accounts/1000/shared/misc/berrycore
export PATH="$pi_root/node/bin:$core_root/bin:/base/usr/bin:/bin:/usr/bin:$PATH"
export LD_LIBRARY_PATH="$pi_root/node/bin:$core_root/lib:/base/lib:/base/usr/lib"
if [ "$PWD" = "$HOME" ]; then
  cd /accounts/1000/shared/documents/pi-projects/my-project || exit 1
fi
export PYTHONIOENCODING=utf-8
export PI_OFFLINE=1
export PI_SKIP_VERSION_CHECK=1
export TERM=xterm-256color
if [ -t 0 ] && [ -t 1 ]; then
  printf "Pi 正在启动，请稍候…\n" >&2
fi
exec "$pi_root/node/bin/node.bin" --jitless --no-expose-wasm --no-experimental-global-navigator --max-old-space-size=384 "$pi_root/entry.mjs" "$@"
