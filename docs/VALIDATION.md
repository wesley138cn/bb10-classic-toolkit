# Validation matrix

Recorded on 2026-10-21. Hardware: BlackBerry Classic SQC100-2, BB10 10.3.3.2049; BerryCore 0.90.1.

| Area | Automated/device evidence from implementation | Physical/user acceptance | Remaining boundary |
| --- | --- | --- | --- |
| CJK display | 10,027 native cell/parser checks; actual SDL renderer | Normal spacing and visible Chinese confirmed | Not complete Unicode/emoji support |
| Offline Pinyin | ASan/UBSan, 24,000 randomized edits; native decoder; real PTY exact commits and 32-character transfer | User selected Chinese candidates, submitted to Pi and received a reply | Not a system-wide IME; personal ranking disabled; not every key/touch route separately accepted |
| History v2 | Bounded ring, freeze, resize, wide cells, parser, real SDL callback and event dispatch; PTY regression | User confirmed buttons flip history and return to input; 3 Up events and return recorded | Physical dragging and Meta shortcuts not separately accepted; history is in memory |
| Pi 0.74.2 | 18,239 width comparisons; real write/bash and read/edit/bash tasks; same-session resume checked separately; 52×38 PTY startup/exit | Pi interface and input confirmed; later Pinyin round trip confirmed | No WebAssembly image processing, other providers/extensions and long-term behavior not accepted |
| BerryCloud | 14 host tests, 9 Python 3.2 simulated device checks; real HTTPS streaming/non-streaming and resumed context | API behavior observed; no complete visual screen audit claimed | Provider terms/support and account quota are separate from technical connectivity |
| Local Qwen | Arithmetic 15 in 193.474 s; Chinese notification in 200.283 s; loopback service restart | No phone browser visual acceptance | Original llama-server still failed initialization; adapter reloads the model each question |

The first history revision was not accepted physically. Revision 2 introduced visible controls; only the acceptance above supersedes that failure. Injected gestures and framebuffer images were never treated as proof of a physical gesture.

The first Pi two-turn test did not establish session resume because upstream session migration changed its placement. A subsequent explicit pi -c check with matching first-entry ID established resume; those are different claims.

The source publication workflow reruns host tests separately and records the result in docs/PUBLICATION_CHECKS.md. No device is connected or modified by repository preparation. Build/install helpers consolidated for publication still need fresh-device validation; no universal installer or fleet reliability is claimed.
