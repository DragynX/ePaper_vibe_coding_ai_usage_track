# Usage Monitor Local Stats Service

Optional computer-side service for the e-paper firmware. It exposes local CLI-log statistics as JSON:

```sh
python3 server.py --host 0.0.0.0 --port 8787
```

Check the endpoint:

```sh
curl "http://127.0.0.1:8787/v1/snapshot?providers=claude,codex,zai"
```

The firmware uses the same endpoint when built with `UM_ENABLE_LOCAL_STATS`.

## Provider Log Roots

Default roots:

| Provider | Default path |
| --- | --- |
| Claude | `~/.claude/projects` |
| Codex | `~/.codex` |
| Copilot | `~/.copilot` |
| MiniMax | `~/.minimax` |
| Kimi | `~/.kimi` |
| Zai | `~/.zai` |

Override paths with environment variables:

```sh
UM_LOCAL_CODEX_LOG_DIR=/path/to/logs python3 server.py --host 0.0.0.0 --port 8787
```

The service returns `available=false` for providers without readable JSONL usage records.
