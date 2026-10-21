#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
${CC:-cc} -std=c99 -g -fsanitize=address,undefined -Itests/include -Isrc tests/test-history.c src/scrollback.c -o build/test-history
# Apple ASan has no LeakSanitizer; do not claim leak coverage from this override.
ASAN_OPTIONS=detect_leaks=0 ./build/test-history
if [ "${1-}" = '--ime' ]; then
  test -d .cache/libgooglepinyin-0.1.2 || { echo 'Run python3 term49/fetch-deps.py --pinyin-only first.'; exit 1; }
  ${CXX:-c++} -std=c++11 -O1 -g -fsanitize=address,undefined -I.cache .cache/libgooglepinyin-0.1.2/src/*.cpp .cache/libgooglepinyin-0.1.2/tools/pinyinime_dictbuilder.cpp -o build/dictbuilder
  ASAN_OPTIONS=detect_leaks=0 ./build/dictbuilder .cache/libgooglepinyin-0.1.2/data/rawdict_utf16_65105_freq.txt .cache/libgooglepinyin-0.1.2/data/valid_utf16.txt build/dict_pinyin.dat
  ${CXX:-c++} -std=c++11 -O1 -g -fsanitize=address,undefined -I.cache .cache/libgooglepinyin-0.1.2/src/*.cpp ime_engine.cpp test-engine.cpp -o build/test-engine
  ASAN_OPTIONS=detect_leaks=0 ./build/test-engine build/dict_pinyin.dat build/test-user.dat
fi
