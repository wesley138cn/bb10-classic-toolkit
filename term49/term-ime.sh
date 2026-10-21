#!/bin/sh
ime_dir=/accounts/1000/shared/misc/term49-ime
case "${1-status}" in
 on)
  test -x "$ime_dir/Term49-IME" && test -r "$ime_dir/dict_pinyin.dat" || exit 1
  : > "$ime_dir/enabled"
  echo 'Pinyin enabled. Close and reopen Term49.' ;;
 off)
  rm -f "$ime_dir/enabled"
  echo 'Pinyin disabled. Close and reopen Term49 for the previous CJK terminal.' ;;
 status)
  if test -f "$ime_dir/enabled"; then echo 'Pinyin: enabled for next launch'; else echo 'Pinyin: disabled'; fi
  echo "Current terminal IME: ${TERM49_IME-0}" ;;
 *) echo 'Usage: term-ime [status|on|off]'; exit 2 ;;
esac
