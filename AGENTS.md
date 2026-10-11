# AGENTS.md

## Git identity firewall

The maintainer has two GitHub identities that must never cross. Each repo belongs to exactly
one of them, and the owner of its `origin` remote decides which. Every commit, push, issue,
PR and doc in this repo uses this repo's identity only.

**Lane: `mercurious` (pseudonymous).** This repo is `mercurious/nes-pickleball` (private).

- Commit as `mercurious <dutch.money@gmail.com>`. That is already this repo's `user.email`.
- Keep the maintainer's real name, institution and other accounts out of everything here:
  code, docs, commit messages, issues and PRs. Nothing in this repo may link this identity to
  the other one. No LAN addresses or host names of the maintainer's machines either.
- `gh` usually has the other account active. Never switch it globally (another session may need
  it). Scope the identity to the command: `T=$(gh auth token --user mercurious)`, verify in the
  same command that `GH_TOKEN=$T gh api user --jq .login` prints `mercurious`, and only then push
  (a one-off `credential.helper`) or call `gh api`. Abort otherwise.

On the maintainer's machines, a Claude Code `PreToolUse` hook enforces this:
`~/.claude/hooks/git-identity-firewall.py`, registered on `Bash` in `~/.claude/settings.json`
(self-tests: `python3 ~/.claude/hooks/test_git_identity_firewall.py`). It runs before
`git commit|merge|rebase|cherry-pick|am|revert|push`, including inside `ssh HOST '...'`, and
before `gh repo create --push` and `gh pr create`. It reads the origin owner, the configured
`user.email`, and any `-c user.*`, `--author` or `GIT_*_EMAIL` override. It denies the command
when that identity doesn't belong to the owner's lane. The owner-to-email map is `RULES` in the
script. A repo with no remote is allowed.

If the hook denies a command, the identity is wrong. Fix `user.email`, or ask the maintainer.
Don't route around the hook: no overrides, no other shells, no rewriting the remote. Agents
other than Claude Code aren't covered by that hook, but they are covered by the next one.

A second firewall runs inside git itself, so it covers every tool (Claude Code, gemini, copilot,
a plain shell): the global `core.hooksPath` is `~/.config/git/identity-firewall/hooks`, the
logic is in `firewall.py`, and the self-tests are in `test_firewall.py`. `pre-commit` and
`commit-msg` block the other lane's names and email addresses in staged changes, file paths
and commit messages, and check the commit identity. `pre-push` checks every outgoing commit.
Git has no global identity (`user.useConfigOnly`), so each repo needs its own `user.name`
and `user.email` (`git config --local`). The repo's own hooks in `.git/hooks` still run after
the firewall. Never pass `--no-verify` or change `core.hooksPath`; only the maintainer may
bypass a block.

## Commit trailers

Commits made with an AI agent end with `Assisted-by: Claude Code (<model>)`. Never
`Co-Authored-By` for an AI (the maintainer's rule, 2026-10-01).

## Where work runs

Agents work from the maintainer's daily driver: an Asahi Linux M1 with 8 GB of RAM and no fan. It
is the smallest computer in this project and it is usually busy.

- **cc65 builds are seconds** and are fine on the laptop: `make`, `tools/*.py`, `py_compile`.
- **MesenCE** (the test runner) is a .NET build: build it on the maintainer's OCI Ampere node
  `etk-cloud` (4 OCPU, 24 GB, Ubuntu 24.04, aarch64, so the binary also runs on the laptop), and
  run long test soaks there. Never a .NET build or a long emulator soak on the laptop.
- **No model servers on the laptop** (it has OOM-crashed before). The voice layer's models run on
  the handhelds only; on the laptop the sidecar is tested with a stub voice.
- **The handhelds** (`car8`, an 8 GB Flip 2; `car12`, a 12 GB Flip 2, shared with another project)
  are the maintainer's. Ask before a session on `car12`. Deploy to your own directory under
  `/storage`, never edit `retroarch.cfg` (use `--appendconfig`), kill RetroArch by PID, and
  restart `essway.service` if the launcher goes black. Listening tests need the maintainer's ears:
  put the WAVs where they can hear them and wait for the verdict.
- The nodes and rigs belong to the maintainer; if a step there is blocked (a permission prompt, a
  denial, a failing ssh), stop and ask rather than falling back to the laptop.
