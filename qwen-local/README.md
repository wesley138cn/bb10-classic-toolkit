# Local Qwen compatibility work

This independently added Python 3.2 service wraps the **working llama-completion runner**. It is not a fix that makes the original llama-server work. Each question reloads the model; requests are single-turn and non-streaming.

Prerequisites from upstream: bcllm 1.2.1; Qwen2.5-0.5B-Instruct-Q4_K_M.gguf; compatible GCC 9 libstdc++.so.6/libgcc_s.so.1 scoped to bcllm/lib. The test device's system C++ library lacked symbols needed by the runner. Do not replace system libraries.

Model filename: `/accounts/1000/shared/misc/bcllm/models/qwen2.5-0.5b-instruct-q4_k_m.gguf`.
Model SHA-256: `6eb923e7d26e9cea28811e1a8e852009b21242fb157b26149d3b188f3a8c8653` (397,808,192 bytes). Obtain it from [the upstream conversion](https://huggingface.co/bartowski/Qwen2.5-0.5B-Instruct-GGUF).

Settings: one thread, context 512, batch 512, microbatch 16, fit off, Qwen ChatML, 32 output tokens by default (service caps at 64). Tiny prompt batches caused an empty-response case in the tested runner; the final settings are recorded in the source.

## Run on the device

Create `bcllm/var` if absent. Run from Term49 with the original local service stopped (or use another free port):

```sh
LD_LIBRARY_PATH=/base/lib:/base/usr/lib /base/usr/bin/python3.2 classic-web-server.py --port 8080
```

The service binds only 127.0.0.1. Test `GET /health` and `POST /v1/chat/completions`:

```json
{"messages":[{"role":"user","content":"7加8等于多少？"}],"max_tokens":32,"stream":false}
```

The handler uses only the last user message; it does not preserve chat context. It accepts up to 160 input characters, one active request, and kills inference after 10 minutes. Ctrl+C stops the foreground service and its active child. Do not start it over an existing service PID file; keep backups of your own configuration before adapting it.

The root page route expects an existing `bcllm/web/index.html`. The locally customized upstream HTML and bcllm launcher are not distributed here because their redistribution scope has not been established. The HTTP API works independently of that page. This source repository therefore does not offer a fresh browser UI installation. The original device keeps its working page unchanged.

Recorded results: arithmetic output 15 in 193.474 seconds; Chinese notification in 200.283 seconds. These are two specific runs, not a performance guarantee. The original llama-server remained stuck initializing slots; browser visual acceptance and long-term operation were not established.

Integration notes for upstream: scope newer C++ libraries to the inference process, isolate Python's libraries, back up existing models before installers replace bcllm, and preserve all model/config data. `ai-classic.conf.example` documents the tested values without replacing a user's current config.
