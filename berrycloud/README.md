# BerryCloud 1.0.0

Python 3.2+ terminal chat for BerryCore. Supports streaming, multi-line input, saved sessions/resume, context, export, cancellation and manual retry. It does not execute model replies or automatically read phone files.

On the phone, copy this directory's berrycloud.py, ai-cloud and README.txt to `/accounts/1000/shared/misc/berrycloud/`. Invoke it directly:

```sh
sh /accounts/1000/shared/misc/berrycloud/ai-cloud setup
sh /accounts/1000/shared/misc/berrycloud/ai-cloud
sh /accounts/1000/shared/misc/berrycloud/ai-cloud --resume
```

The wrapper selects the system Python libraries, avoiding BerryCore/system OpenSSL conflicts. Use the setup prompt to enter your own provider credentials. Configuration and sessions live under the calling Term49 user's `~/.berrycloud`, not in this repository. Permissions are 0700/0600; files are local plaintext, not an encrypted vault.

For a short `ai-cloud` command, after confirming no unrelated command exists, place the following script at `berrycore/bin/ai-cloud` and make it executable:

```sh
#!/bin/sh
exec /bin/sh /accounts/1000/shared/misc/berrycloud/ai-cloud "$@"
```

The existing device also has `ai cloud` dispatch integrated into its original ai launcher. A new installation can use the standalone command without changing that launcher. The runtime client is the same.

Current defaults retain the tested GLM-5.3-Flash configuration. Technical connectivity to the Coding Plan endpoint does not imply official support or permission for a custom client; select an endpoint/model your account permits. Other providers have not been accepted. Calls may consume paid quota; tests in test_berrycloud.py use a local fake transport.

Rollback: remove only the shortcut you created; keep configuration/history if desired. If you previously added the `ai cloud` integration, restore your own pre-integration launcher backup after inspecting later changes. Do not overwrite another installation's launcher or delete its private data.

Host tests: `python3 -m unittest discover -s berrycloud -p 'test_*.py'` from the repository root. `device-selftest.py` is a separate simulated transport test for the phone's installed client, not a real API call.
