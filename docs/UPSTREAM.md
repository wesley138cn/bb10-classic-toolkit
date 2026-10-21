# Upstream contribution map

This repository is a community companion. None of the changes below is claimed to be merged or endorsed upstream. No issues, comments or pull requests are sent as part of source packaging.

| Target | Candidate contribution | Best review boundary |
| --- | --- | --- |
| BerryFarm/Term49 | CJK double-cell width and dual fonts | Parser/buffer/rendering with regression cases |
| BerryFarm/Term49 | Offline Pinyin adapter | Optional terminal-local IME, decoder attribution and no PTY preedit leakage |
| BerryFarm/Term49 | Scrollback v2 | Ring buffer/frozen view first; controls and SDL input compatibility separately |
| sw7ft/BerryCore | Classic runtime/inference notes | Scoped libstdc++ resolution, one-thread parameters, Qwen ChatML and completion fallback; preserve existing models |
| Pi upstream | QNX portability findings | Intl segmentation, unsupported Unicode regex, HTTPS transport, minimum Node versions |
| libgooglepinyin maintenance | Decoder pool-bound fixes | Focused patch with randomized-edit reproducer |

A patch against the recorded Term49 baseline is in term49/term49-classic.patch; it is a review aid accompanying complete source, not an already submitted PR. Before submission, rebase against the target's current branch and discuss optional platform-specific behavior. Treat the decoder patch separately from UI integration.

The local BerryCore bcllm script and its derived HTML are deliberately not republished under a guessed global license. qwen-local contains the independently added Python adapter, configuration values and integration notes; launcher changes can be proposed once the relevant source license/contribution terms are clarified.
