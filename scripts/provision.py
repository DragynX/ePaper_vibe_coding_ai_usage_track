#!/usr/bin/env python3
"""Print secrets.h #define lines from your local Claude/Codex credential files.

Reads ~/.claude/.credentials.json and ~/.codex/auth.json and emits the macro
lines to paste into src/secrets.h. Prints token VALUES to your terminal only --
nothing is written to disk or sent anywhere. Fill in WiFi separately.

读取本机 Claude/Codex 凭证文件,打印可粘贴到 src/secrets.h 的宏行。token 仅打印到
终端,不落盘、不外传。WiFi 需自行填写。
"""
import json
import os
import sys


def load(path):
    try:
        with open(os.path.expanduser(path), "r") as f:
            return json.load(f)
    except FileNotFoundError:
        print("# WARNING: %s not found -- log in with the CLI first" % path,
              file=sys.stderr)
        return {}
    except (ValueError, OSError) as e:
        print("# WARNING: could not read %s: %s" % (path, e), file=sys.stderr)
        return {}


def main():
    claude = load("~/.claude/.credentials.json").get("claudeAiOauth", {})
    codex_doc = load("~/.codex/auth.json")
    codex = codex_doc.get("tokens", {})

    print("// --- paste into src/secrets.h (fill WiFi yourself) ---")
    print('#define UM_WIFI_SSID     "your_wifi_ssid"')
    print('#define UM_WIFI_PASSWORD "your_wifi_password"')
    print('#define UM_TZ            "UTC0"')
    print()
    print('#define UM_CLAUDE_ACCESS_TOKEN  "%s"' % claude.get("accessToken", ""))
    print('#define UM_CLAUDE_REFRESH_TOKEN "%s"' % claude.get("refreshToken", ""))
    print('#define UM_CLAUDE_EXPIRES_AT_MS "%s"' % claude.get("expiresAt", 0))
    print('#define UM_CLAUDE_SUBSCRIPTION  "%s"'
          % claude.get("subscriptionType", "pro"))
    print()
    print('#define UM_CODEX_ACCESS_TOKEN   "%s"' % codex.get("access_token", ""))
    print('#define UM_CODEX_REFRESH_TOKEN  "%s"' % codex.get("refresh_token", ""))
    print('#define UM_CODEX_ACCOUNT_ID     "%s"' % codex.get("account_id", ""))
    print('#define UM_CODEX_LAST_REFRESH   "%s"'
          % codex_doc.get("last_refresh", "0"))


if __name__ == "__main__":
    main()
