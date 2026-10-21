# BlackBerry Classic development environment

Work in the current project directory. Ask before modifying files outside it or deleting existing data. Never read or print API credentials, .berrycloud/config.json, or .pi/agent/models.json.

Device: BlackBerry Classic, BB10/QNX, ARM 32-bit, 2 GB RAM. This is NOT Linux or modern Android. Node 22.0.0 is available at /accounts/1000/shared/misc/pi-classic/node/bin/node (jitless). Python is /base/usr/bin/python3.2 (Python 3.2); modern Python syntax and third-party packages usually do not work. BerryCore tools are at /accounts/1000/shared/misc/berrycore/bin.

For Python scripts, use the system library path: LD_LIBRARY_PATH=/base/lib:/base/usr/lib /base/usr/bin/python3.2 script.py. Do not replace system libraries, change the global shell profile, erase apps, or install packages without asking. Keep notes, credentials, existing AI applications and models intact.

Prefer small UTF-8 text programs, CLI tools, or BB10-compatible local web interfaces. Verify changes on this device. Report actual commands and results; do not claim visual acceptance or long-term stability from a passing unit test. Write responses in concise Chinese.
