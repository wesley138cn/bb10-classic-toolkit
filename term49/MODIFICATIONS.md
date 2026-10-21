# Changes to Term49

Based on BerryFarm/Term49; copyright notices retained.

Modified or added source files: buffer.c, buffer.h, ecma48.c, ime_engine.h, main.c, preferences.c, preferences.h, scrollback.c, scrollback.h, symmenu.c, types.h.

Changes made on 2026-10-21: CJK cell widths and dual-font rendering; attachment to the foreground signed Term49 window group; offline Pinyin integration; 4,096-line scrollback, frozen history view, visible navigation buttons, input-event handling and device test hooks. SDL_ttf sources retain their own zlib-style notices. The aggregate patch is for review against the recorded baseline; the complete final source is in src/. ime_engine.cpp is supplied alongside it.
