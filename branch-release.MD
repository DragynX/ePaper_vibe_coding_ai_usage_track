# Branch Release Runbook — publish my version as a public GitHub fork

Goal: put **my** version of this firmware on **my** GitHub account, public and
**independent** of the upstream it came from — **not** merged back via a Pull
Request.

## Background

- This repo descends from the public upstream
  **`limengdu/ePaper_vibe_coding_ai_usage_track`** (that's what `origin` points to).
- My working branch `rebuild-hardened` is ~50 commits ahead of the shared
  merge-base (`3cab1f1`): a personal hardening/rebuild (security, render overhaul,
  Claude/credential rework, 429 backoff, new defaults), currently at **v1.15.9**.

## Why NOT a Pull Request

A PR is a request to **merge my commits into limengdu's repo** — the opposite of the
goal. The work is ~50 diverged commits of opinionated, device-specific direction, not
a targeted fix a maintainer would take wholesale; a PR also needs the upstream owner
to accept it. A fork published under my account already makes the code public, with
full history and credit to the original. **Fork = right tool; PR = wrong tool.**

## Decisions (locked)

- **Method:** GitHub Fork (linked — keeps "forked from", history, upstream license).
- **Published default branch:** `Rebuild-updated` (set as repo default); leave `main`
  mirroring upstream.
- **Pending work:** commit v1.15.9 (Claude raw-epoch Expires-At field) first.

## Tooling

- **GitHub CLI `gh` v2.94.0 is installed** at `C:\Program Files\GitHub CLI\gh.exe`,
  but **not yet on PATH** (installed mid-session) and **not authenticated**. Either
  open a NEW terminal so `gh` resolves, or call it by full path. `gh` collapses the
  fork + remote wiring + default-branch into a few commands and configures git auth,
  so no manual PAT/browser credential juggling on push.

## Pre-flight (safety)

- No real tokens in tracked files (verified `git grep sk-ant-...` = clean); secrets
  live in NVS, `secrets.h` gitignored.
- `builds/` is gitignored — release binaries are NOT published.
- Keep untracked / unpublished: `docs/claude_token_monitor.txt` (debug serial
  capture); `docs/claude_setup_token_esp32_workflow_1.md` (documents the setup-token
  approach that 403s on `/api/oauth/usage` — misleading to ship).
- **License caveat:** no LICENSE file. GitHub TOS always permits forking a public
  repo, so the fork is fine — but with no upstream license the code is technically
  "all rights reserved" by the original author; you can't relicense it. Fork +
  credit is the safe path; check upstream's license before any formal release.

---

## Primary path (GitHub CLI)

### 1. Commit pending v1.15.9 (local — Claude can run; no network/auth)
```
git add ProjectConfig.h SettingsServer.cpp branch-release.MD
git commit -m "v1.15.9: Claude Expires At raw-epoch field + live readout; add release runbook"
```
(Stages named files only; the untracked docs/debug capture stay out.)

### 2. Name the branch `Rebuild-updated` (local — Claude can run)
```
git branch -m rebuild-hardened Rebuild-updated
```

### 3. Authenticate gh (YOU — interactive, one time)
New terminal (so `gh` is on PATH), or use the full path. Choose GitHub.com → HTTPS →
"authenticate Git with your GitHub credentials = Yes" (this wires git auth for push):
```
gh auth login
gh auth status        # confirm logged in
```

### 4. Fork + wire remotes in one shot (YOU, from inside the repo)
```
gh repo fork --remote --clone=false
```
This creates `<YOUR-USER>/ePaper_vibe_coding_ai_usage_track`, adds it as remote
**`origin`**, and renames the original limengdu remote to **`upstream`**. Verify:
```
git remote -v         # origin = your fork, upstream = limengdu
```

### 5. Push your branch (YOU — gh already authorized git)
```
git push -u origin Rebuild-updated
```

### 6. Set the public default branch (YOU)
```
gh repo edit --default-branch Rebuild-updated
```
(`gh repo edit` targets the `origin` repo = your fork. The branch must exist remotely,
so run after Step 5.)

### 7. (Optional, after it's live)
- README: add a "Fork of limengdu/… with personal hardening (v1.15.x)" note.
- Decide whether to clean up + publish the workflow doc, or delete the debug capture.
- Track upstream later: `git fetch upstream && git log upstream/main`. Daily work
  stays on `Rebuild-updated`.

---

## Fallback path (web UI, no gh)

1. github.com → `limengdu/ePaper_vibe_coding_ai_usage_track` → **Fork**.
2. `git remote rename origin upstream`
3. `git remote add origin https://github.com/<YOUR-USER>/ePaper_vibe_coding_ai_usage_track.git`
4. `git branch -m rebuild-hardened Rebuild-updated`
5. `git push -u origin Rebuild-updated` (first push prompts for GitHub auth / PAT)
6. Fork → **Settings → General → Default branch** → `Rebuild-updated`.

---

## Who runs what
- **Claude (local, safe):** Step 1 (commit), Step 2 (branch rename).
- **You (GitHub account / auth):** Step 3 (`gh auth login`), Step 4 (fork), Step 5
  (push), Step 6 (default branch). All outward/auth actions are yours.

## Verification
- `git remote -v` → `origin` = your fork, `upstream` = limengdu.
- `git branch -vv` → `Rebuild-updated` tracks `origin/Rebuild-updated`.
- `gh repo view --web` → fork shows "forked from limengdu/…", branch
  `Rebuild-updated` with all commits through **v1.15.9**, set as default.
- `builds/`, tokens, and the debug capture are absent from the published tree.
- Upstream `limengdu` is untouched (no PR, nothing pushed to `upstream`).
