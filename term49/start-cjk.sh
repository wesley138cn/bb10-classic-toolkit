#!/bin/sh
# Local CJK companion for Term49; the signed app remains installed.
cjk_root=/accounts/1000/shared/misc/term49-cjk
case "${1-}" in
  off)
    rm -f "$cjk_root/enabled"
    echo 'CJK auto-start disabled. Close and reopen Term49 for the original terminal.'
    exit 0 ;;
  on)
    : > "$cjk_root/enabled"
    echo 'CJK auto-start enabled.'
    exit 0 ;;
  '') ;;
  *) echo 'Usage: term-cjk [on|off]'; exit 2 ;;
esac
if [ "${TERM49_CJK-}" = 1 ]; then
  echo 'This terminal already uses CJK character widths.'
  exit 0
fi
if [ ! -r "$cjk_root/Term49-CJK" ]; then
  echo 'CJK terminal is unavailable; original terminal remains usable.'
  exit 1
fi
LD_LIBRARY_PATH="/base/lib:/base/usr/lib" \
  /base/usr/bin/python3.2 "$cjk_root/launch-attached.py"
status=$?
if [ "$status" -ne 0 ]; then
  echo "CJK terminal exited with status $status. Original terminal is still available."
fi
exit "$status"
