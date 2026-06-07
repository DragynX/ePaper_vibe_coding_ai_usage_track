# Security Notes

This firmware originated from a third-party project and was audited. This file
records the threat model, what was hardened, and what is intentionally deferred.

## Threat model

A personal, **LAN-only** ESP32-S3 e-paper dashboard. It holds OAuth tokens / API
keys for AI usage providers (Claude, Codex, Copilot, MiniMax, Kimi, Zai, Claude
Platform) entered through a local web UI and stored in NVS. The assumed-trusted
boundary is the home Wi-Fi network and physical possession of the device.

## Hardened in v1.1.9

- **Tokens no longer leak over the LAN.** `GET /api/settings` previously returned
  every stored access/refresh token and API key in plaintext to any client on the
  network. Secret fields are now returned empty, with a `<key>_set` boolean so the
  UI can show "saved". Saving the form without retyping preserves stored secrets
  (`ConfigStore::fromJson` keep-if-blank).
- **POST body size cap (8 KB)** on `/api/settings` — an unbounded request body
  could exhaust heap and reboot the device. Oversize bodies are dropped and answered
  with `413`.
- **`/api/status` JSON is escaped.** The SSID (AP-controlled) was concatenated raw
  into the JSON, allowing malformed/injected output. Now escaped (`\`, `"`, control
  chars).
- **`.gitignore`** now ignores root `secrets.h` (sources moved out of `src/`), so a
  real key file can't be committed by accident.

## Deferred (accepted risk for a trusted LAN; revisit before any wider exposure)

- **No authentication / CSRF on `/api/*`.** Any LAN device can change settings or
  trigger `/api/restart` and `/api/wifi-reset`. Acceptable on a trusted home network;
  add a shared-secret gate before exposing the device to untrusted users.
- **TLS uses `WiFiClientSecure::setInsecure()`** (`ProjectHttpClient.cpp`) — outbound
  HTTPS to provider APIs does not validate certificates, so an on-path attacker can
  MITM token-bearing requests. Proper fix: pin a CA-root bundle for the provider
  hosts. Deferred due to cert-rotation maintenance; the device only talks to known
  hosts over the home network.
- **NVS stores tokens in plaintext.** Recovering them requires physical flash access.
  Fix: enable flash/NVS encryption (Espressif NVS Encryption / flash encryption).
- **Open onboarding AP** ("UsageMonitor", no password) during Wi-Fi provisioning —
  standard captive-portal pattern, only active until Wi-Fi is configured.
- **The web server is plain HTTP** (AsyncWebServer has no practical HTTPS). Credentials
  entered in the UI cross the LAN in cleartext.

## Audit false positives (do not re-flag)

- "`strncpy` missing null termination → buffer overflow" in the provider clients:
  the `char[16]` targets (`ProviderQuota::planType`, `name`) are zero-initialized and
  the struct is reset (`out = ProviderQuota()`) before each fetch; `strncpy(dst, src,
  sizeof(dst)-1)` never writes the final byte, so the terminator always survives. Not
  exploitable.
- "`secrets.h` committed to git": it was never tracked; only the `.gitignore` path was
  stale (now fixed).

## References

- Espressif ESP-IDF Security — NVS Encryption, flash encryption, CA validation:
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/security/security.html
- HTTPS server on ESP32 (esp32_https_server) for a future authenticated/TLS UI.
