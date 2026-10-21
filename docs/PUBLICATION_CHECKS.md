# Source publication checks

Checked on the publication host, 2026-10-21 (Asia/Shanghai).

- Clean npm ci from both lockfiles completed with Node 22.22.3.
- BerryCloud: 14 tests passed using a fake transport.
- Pi: 26 dependency files transformed; 18,239 width comparisons and segmentation checks passed.
- Pi curl transport: Unicode/body/header integrity, HTTP error preservation and cancellation passed.
- Pi guard: selected outside-path, symlink, credential-path and destructive-command cases passed; this is not proof of sandboxing.
- Pi bundle: 10,438,481 bytes, 2,930 input modules. Generated dependencies and bundle are excluded from the source repository.
- History: ASan/UBSan ring/frozen-view/resize/clear/teardown checks passed.
- Pinyin: dictionary build, candidate/phrase/edit/reopen tests and 24,000 randomized edits passed under ASan/UBSan. Old decoder compilation emits format warnings; no sanitizer failure occurred. LeakSanitizer is disabled.
- Term49: 18 recorded upstream source files matched the pinned upstream archive. The aggregate patch reconstructed the final terminal source exactly.
- Decoder: fetched, patched src/include/tools match the previously accepted implementation.
- Dependency fetch: Term49 archive, decoder archive and Noto font hashes matched.
- Source-only publication checks cover relative document links, Python syntax, known credential patterns and private workstation/device-path patterns. Source selection uses a curated file list and Git exclusions; pattern checks are not a guarantee against all possible secrets.

No phone installation, live model call, new physical acceptance or long-term test was performed during publication. The consolidated device build helper is syntax-checked and documented; it has not been rerun on a fresh phone. Historical device acceptance remains separately listed in VALIDATION.md. Remote CI state is reported by GitHub Actions rather than inferred from these host results.
