# Agent rules

## Commits

- Prefix the commit subject with `feat:` when the commit adds a new feature (e.g. `feat: add visual keyboard editor`).
- Prefix the commit subject with `ci:` when the commit changes files under `.github/` (e.g. `ci: add release workflow`).
- Prefix the commit subject with `test:` when the commit changes files under `tests/` (e.g. `test: cover the Android layer`).
- Prefix the commit subject with `docs:` when the commit only changes documentation (e.g. `docs: add the no auto push rule to AGENTS.md`).
- Commit messages are a single line: no body.
- Do not add a `Co-Authored-By` trailer.
- Never push a commit on your own: commit locally, and push only when the user asks for it in that message (an earlier push or an open PR does not cover later commits).
- Keep tests and codebase in separate commits: files under `tests/` go in their own commit, apart from the source, web, config and docs changes they cover.

## README

- Keep the README short: point to the file and give a one-line description, not the details. Lists of names, counts, per-OS tables and the like belong in the referenced file (its header comment), not the README.

## Pull requests

- Do not add a "Generated with Claude Code" line to pull request descriptions.
