# Term49 Classic: CJK, offline Pinyin and scrollback

Complete final terminal source is in `src/`, based on BerryFarm/Term49 commit `a02f06232c3af9781d4a7f70ed5939e7447825f7`. All 18 files in our recorded upstream source snapshot matched that commit byte-for-byte. `term49-classic.patch` shows our terminal changes; `ime_engine.cpp/.h` and the libgooglepinyin patch supply the decoder integration.

## Behavior

- Latin uses the phone's original Andale Mono; Chinese uses Noto Sans Mono CJK SC in two terminal cells with aligned baselines. It does not stretch Chinese glyphs horizontally.
- Shift+Space, Meta+i or the bottom input label toggles English/Pinyin. Select a candidate using touch, Space or a number; use arrows to navigate. Preedit stays outside the PTY. Enter during unfinished spelling commits literal spelling without sending a newline. This is a terminal-local simplified-Chinese IME, not the BB10 global input method.
- History stores at most 4,096 lines per screen. Entering history freezes a copy while the live terminal keeps receiving output. Ordinary typing is intercepted in history mode; return to input to resume typing. Unfinished Pinyin is preserved.
- Visible Up/Down/Return buttons were physically accepted. Drag and Meta+U/D/B are implemented and programmatically checked, but their physical routes were not separately accepted. History is in memory and ends when the terminal exits; Pi session persistence is separate.

## Fetch prerequisites on a host

```sh
python3 term49/fetch-deps.py
sh term49/test-host.sh --ime
```

Downloads go to ignored `term49/.cache/`. Every archive/font is SHA-256 checked against dependencies.json. The Pinyin source is patched before use. The old decoder's serialized dictionary is architecture-dependent: the host test dictionary is not the phone dictionary. The phone must build its own.

The fetched Term49 snapshot supplies `external/include` and `external/lib`; these libraries remain external prerequisites, not included in the public repository. The Noto font is downloaded separately. You also need an existing BerryCore GCC 4.6.3 toolchain and `target_10_3_1_995/qnx6` SDK on the phone. This repository does not provide the QNX SDK or a newly signed BAR.

## Native device build

Transfer `src/`, `ime_engine.cpp`, `ime_engine.h`, `test-engine.cpp`, `test-pty.c`, `build-device.py`, and the prepared dependency directories to a separate working directory on the device. Keep the directory name `libgooglepinyin-0.1.2` for the decoder. The following command is run **inside Term49 on the phone**, with paths adjusted to that working directory:

```sh
LD_LIBRARY_PATH=/base/lib:/base/usr/lib /base/usr/bin/python3.2 build-device.py \
  --external .cache/Term49-a02f06232c3af9781d4a7f70ed5939e7447825f7/external \
  --pinyin .cache/libgooglepinyin-0.1.2
```

The helper compiles every C/C++ object, links `build/Term49-Classic`, builds a native dictionary and runs the decoder test. It does not change launchers or an installed terminal. Final terminal linking uses the **C compiler plus system -lcpp**; using G++'s implicit GNU libstdc++ conflicts with the device ICU/SDL combination.

This build helper consolidates the recorded per-stage commands. Host checks do not establish a fresh device build: the original final runtime was built with the recorded stage-specific commands before publication. `test-pty.c` is also supplied for native PTY testing against the real terminal converter (compile with io.c and the BB10 ICU/clipboard libraries); it cannot be replaced by a host Unicode string test.

## Runtime layout and integration

Keep the installed signed Term49 application. The companion attaches its SDL child to that foreground application's nonzero window group. The supplied launcher refuses a locked/inactive or zero-PID target.

| Runtime destination | Source |
| --- | --- |
| `shared/misc/term49-ime/Term49-IME` | Native `build/Term49-Classic` (executable) |
| `shared/misc/term49-ime/dict_pinyin.dat` | Dictionary generated on ARM by the device build |
| `shared/misc/term49-cjk/NotoSansMonoCJKsc-Regular.otf` | Hash-checked upstream font |
| `shared/misc/term49-cjk/launch-attached.py` | Supplied Python launcher |
| `shared/misc/term49-cjk/start-cjk.sh` | Supplied shell wrapper |
| `shared/misc/term49-cjk/lib/libSDL12.so` | Matched upstream external/lib library |
| `shared/misc/term49-cjk/lib/libTouchControlOverlay.so` | Matched upstream external/lib library |
| `shared/misc/term49-cjk/lib/libconfig.so.11` | Upstream external/lib/libconfig.so with the required runtime name |

Paths above are under `/accounts/1000/`. Keep an existing `Term49-CJK` fallback if one is already installed. Never overwrite working files without a distinct backup. On a fresh source deployment, disabling IME requires a separately built CJK fallback or returning to the original signed terminal; this repository does not pretend that a fallback binary already exists.

Copy your own `~/.term49rc` to a new private `~/.term49cjkrc` if the latter is absent. In the copy only, set `font_path` to the Noto runtime path and retain UTF-8 and the existing preferences. The source uses `.term49cjkrc`; it does not replace the original configuration. Create `shared/misc/term49-ime/enabled` only after the binary and native dictionary are installed. Then, with signed Term49 in the foreground:

```sh
sh /accounts/1000/shared/misc/term49-cjk/start-cjk.sh
```

First validate on the actual phone: enter `nihao`, select a Chinese candidate, send it to a chat tool, produce sufficient output, click Up and then Return, and verify typing still works. Keep a second route to the original terminal. Only after this succeeds, optionally add the following block to a backed-up `~/.profile` (skip it if an equivalent block already exists; inspect any top-level exec/exit first):

```sh
if [ -t 0 ] && [ -z "${TERM49_CJK-}" ] && [ -f /accounts/1000/shared/misc/term49-cjk/enabled ]; then
  sh /accounts/1000/shared/misc/term49-cjk/start-cjk.sh
fi
```

Create `term49-cjk/enabled` to enable that block. Close and reopen Term49 to verify startup separately.

## Rollback

Remove the `term49-cjk/enabled` marker and fully reopen the signed application to use the original terminal. Preserve original configs, app data and model credentials. On the already accepted installation, `term-ime.sh off/on` and `term-scroll.sh rollback/on/status` use the preserved CJK/Pinyin binaries; inspect their source paths first on another device. `term-scroll.sh rollback` requires `Term49-IME.before-scroll`; it is not a synthesized backup.

No runtime binaries or signing credentials are published. Our existing final scrollback v2 binary hash was `6ceae8169cf4cce25d9ac71a0db1a65eb29ebd5c039c490e0342e7b715ced47b`; a new build's hash need not match due to build environment differences. See [validation scope](../docs/VALIDATION.md).
