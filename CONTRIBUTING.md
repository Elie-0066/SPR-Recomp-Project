# Contributing

SPR Recomp is a private fork of [DKB Recomp](https://github.com/Lucas-blip-png/dkb-pc-recomp) whose target is
*Splatoon Raiders*. Work on it goes through its maintainer; issues about Raiders do not belong on the DKB Recomp or TotK
Recomp trackers. A fix to the shared code that would also help those projects can be offered to them separately, under
their rules.

## Where to start

- [docs/raiders-plan.md](docs/raiders-plan.md) has the current state and the open work.
- [docs/architecture.md](docs/architecture.md) explains how the pieces fit, [docs/building.md](docs/building.md) how to
  build, [docs/dkb-testing.md](docs/dkb-testing.md) how to run and diagnose.
- The Bananza documents (`docs/bananza-*.md`, `docs/devlog.md`) are DKB Recomp's history, kept as reference for the
  shared pipeline.

## The one hard rule

Nothing that came out of the game goes into the repository: no game files, keys, extracted data, recompiled C, compiled binaries, shaders, shader caches, fonts, audio or movies. Not in commits, not in issues, not in pull requests. Only information about the game goes in: addresses, hashes, patch code we wrote, shader and bind-state identifiers.

Two checks enforce this:

- CI runs `tools/check_content.py` on every push and pull request.
- The same check runs before each local commit once you enable the hook:

  ```sh
  git config core.hooksPath .githooks
  ```

A failing check is never bypassed with `--no-verify`. If it flags a file by mistake, change the check in a separate pull request and say why.

Links to game files, keys or firmware in issues or pull requests get the issue closed and the content removed.

## Code from other projects

The rules are in [AGENTS.md](AGENTS.md), "Reusing code". In short: the repository is GPL-3.0-or-later as a whole, so code under MIT, BSD, ISC, zlib, Apache-2.0 and GPL-3.0-compatible licences (Eden, Eglantine, yuzu and the like) may be copied. Each copy keeps its original header and gets a row in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) with the upstream commit. Files that came from TotK Recomp keep their MIT terms, and a fix meant for TotK Recomp itself must not use GPL code. Unicorn is used only as a separate test tool.

## Reporting bugs

Report to the maintainer. Include the commit you built (`git rev-parse --short HEAD`), your GPU, driver and Windows version, the log (`logs\dkb.log` in the build folder, next to `spr-recomp.exe`; after a crash, copy it before you start the game again), and the steps to reproduce. Describe what you see instead of attaching screenshots of the game. [docs/dkb-testing.md](docs/dkb-testing.md) covers building, running and the useful settings.

## Pull requests

- One topic per pull request.
- Describe the problem, then what changed.
- Say how you tested it: which commit, which scene, how long, which GPU.
- A change to the lifter (`recomp/`) or the CPU headers passes the differential tests in [docs/building.md](docs/building.md), "Checks".
- Commit messages are plain sentences, without `feat:` or other prefixes.
- AI-assisted code is welcome. You are responsible for having read and run it.

## Code and docs

- Code, comments, commit messages and documentation are in English.
- Generated code is never edited by hand: change the generator.
