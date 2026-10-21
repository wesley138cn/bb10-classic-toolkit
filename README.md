# BB10 Classic Toolkit

[中文说明](README.zh-CN.md)

Chinese terminal input, readable history, and AI tools for a BlackBerry Classic running BerryCore.

An independent community companion to [BerryCore](https://github.com/sw7ft/BerryCore). It brings together our Term49 enhancements, Pi adaptation, BerryCloud client, and local Qwen compatibility work. It is not an official BerryCore release, a replacement operating system, or a claim that these changes have been merged upstream.

## What is here

| Component | Contribution | Observed on the test device |
| --- | --- | --- |
| [Term49 Classic](term49/README.md) | CJK two-cell rendering, separate Latin/CJK fonts, offline Pinyin, 4,096-line scrollback and visible navigation buttons | Chinese candidate selection → Pi reply; history buttons → return to input |
| [Pi Classic](pi-classic/README.md) | Pi 0.74.2 adaptation for the BerryCore QNX/ARM Node port: segmentation, Unicode regex transforms, curl transport, startup and backup hooks | Read/write/edit/bash workflow, session resume, interactive interface |
| [BerryCloud](berrycloud/README.md) | Python 3.2 terminal chat client with streaming, session save/resume and export | Direct HTTPS conversation, follow-up context and resume |
| [Local Qwen](qwen-local/README.md) | Loopback HTTP adapter around a working single-thread llama-completion runner | Arithmetic and Chinese output; approximately 193–200 seconds for the recorded examples |

Test device: **BlackBerry Classic SQC100-2, BB10 10.3.3.2049, BerryCore 0.90.1**. Evidence is dated **2026-10-21**. Other devices, physical drag gestures, all keyboard shortcuts and long-term stability are not claimed as validated. See the [validation matrix](docs/VALIDATION.md).

## Start here

This is a complete source sharing repository for the listed adaptations. Bring your existing BerryCore and signed Term49 installation; platform SDKs, runtime libraries, model weights, credentials and conversations are not bundled. Device build/install instructions and rollback boundaries live with each component.

- For Chinese input and history: [Term49 build and integration](term49/README.md).
- For coding with Pi: [pinned build and device layout](pi-classic/README.md).
- For terminal chat: [BerryCloud setup](berrycloud/README.md).
- For offline inference experiments: [Qwen configuration and limitations](qwen-local/README.md).
- For local verification: [testing](docs/TESTING.md).

Source packaging has its own checks. Repackaging a tested implementation is not a second-device installation acceptance test. This repository does not include an auto-install-everything script.

## Relationship to upstream projects

BerryCore provides the BB10/QNX userland and ports. Term49/Term48 provide the terminal. Pi provides the coding agent. libgooglepinyin provides the offline decoder and dictionary. Our contribution is the adaptation and additional behavior listed above; original authors retain credit. See [credits and license scope](THIRD_PARTY_NOTICES.md) and [upstream contribution candidates](docs/UPSTREAM.md).

New toolkit code and documentation are Apache-2.0 unless a file says otherwise. Existing third-party notices remain authoritative. No upstream acceptance or official endorsement is implied.
