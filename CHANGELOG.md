# Changelog

## Host validation fixes — 2026-10-21

- Correct mismatched array/scalar deallocation in the pinned Pinyin decoder, found by Linux AddressSanitizer during decoder reopening.
- Enable allocation/deallocation matching explicitly in host tests and refresh the generated decoder cache when its patch changes.
- These source fixes have not been installed or validated on the phone.

## Initial source sharing — 2026-10-21

- Publish the final Term49 CJK/Pinyin/scrollback v2 source and upstream review patch.
- Publish Pi 0.74.2 QNX/ARM compatibility code and locked build dependencies.
- Publish BerryCloud 1.0.0 source and tests.
- Publish the independently added local Qwen HTTP adapter and measured compatibility notes.
- Add provenance, reproducibility instructions, validation boundaries and source-publication checks.

The runtime behavior was developed and accepted on the device before this repository was packaged. This is not an upstream release or a claim of fresh-device deployment acceptance.
