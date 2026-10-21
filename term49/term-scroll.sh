#!/bin/sh
root=/accounts/1000/shared/misc/term49-ime
scroll=/accounts/1000/shared/misc/term49-scroll
case "$1" in
  rollback) source="$root/Term49-IME.before-scroll" ;;
  on) source="$scroll/Term49-Scroll" ;;
  status)
    if cmp -s "$root/Term49-IME" "$scroll/Term49-Scroll"; then echo 'Scrollback + Pinyin installed'
    elif cmp -s "$root/Term49-IME" "$root/Term49-IME.before-scroll"; then echo 'Previous Pinyin installed'
    else echo 'Other terminal build installed'; fi
    exit 0 ;;
  *) echo 'Usage: term-scroll on|rollback|status'; exit 1 ;;
esac
[ -s "$source" ] || { echo 'Source missing'; exit 1; }
cp "$source" "$root/Term49-IME.scroll-new" && chmod 755 "$root/Term49-IME.scroll-new" && mv -f "$root/Term49-IME.scroll-new" "$root/Term49-IME" || exit 1
echo 'Updated. Fully close and reopen Term49.'
