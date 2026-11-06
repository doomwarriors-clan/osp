# AGENTS.md – doomwarriors-clan/osp

Guidance for coding agents (and people) working on this repository. See
README.md for what the repo contains, how to build and how releases work.

## The project around this repo

The doomwarriors clan (https://www.doomwarriors.de) runs four Quake III Arena
servers (Duel, TDM, CTF, Instagib) with the OSP mod. Repos in the GitHub org
`doomwarriors-clan`:

| Repo | What | Default branch |
|---|---|---|
| `Quake3e` | engine: fork of ec-/Quake3e, releases tagged `dwc-YYYY.MM.DD` | `dwc` |
| `osp` (this) | OSP 1.03a sources (JKornev/xq3osp `security-patches`) + botmem, released as `1.03a-dwc.N` | `main` |
| `q3-servers` (private) | Docker Compose deployment of the servers | `main` |
| `dwc-hp` | clan homepage | `master` |
| `qbot` | Discord bot (stats, live match reports) | `main` |
| `dwc-bots` | custom bot pack | `main` |

Maintainers: Micha (`n4cer`) and Hannes (`dwc-motter`), both org owners.

## Working rules

- Open topics are GitHub issues. Run `gh issue list -R doomwarriors-clan/osp`
  before starting and reference the issue in the PR.
- Every change goes through a pull request against `main` (protected: no
  direct push, no force push).
- **`gh pr create` in a fork targets the upstream repo (JKornev/xq3osp) by
  default.** Always pass `-R doomwarriors-clan/osp --base main`. Never open PRs
  against JKornev/xq3osp unless a maintainer asks for it (security fixes we
  find are welcome there, but agreed first).
- Never commit built files (`*.qvm`, `*.pk3`, zips). They are release assets.
- Commit messages in English.

## Rules for the code

- **Only `qagame` carries our changes.** `cgame` and `ui` must stay
  byte-identical to JKornev's 1.03a-secured; the `build` workflow fails
  otherwise. Changing client code needs a maintainer decision, an own client
  pk3 name and an update of that check.
- **pk3 names**: ours are `zzz-osp-dwcN.pk3`. Never reuse a name of OSP or
  xq3osp (`zz-osp-*`): same name with other content breaks the pure check for
  clients.
- **Engine syscalls**: the botmem traps in `src/game/g_syscalls.asm` must match
  the `gameImport_t` enum of doomwarriors-clan/Quake3e (`dwc`): appended after
  `G_TRAP_GETVALUE` (700), QVM numbers `-1 - value`. Probe with
  `trap_GetValue` ("…_DWC" names) and keep working without them.
- Keep gameplay unchanged when `bot_memory` is 0. Do not change `gamename`,
  `gameversion` or the `CS_OSP_VERSION` configstring (clients and server
  browsers rely on them).
- Upstream fixes: update `security-patches` from JKornev, merge into `main`.
  Keep our changes in few, separate files where possible so merges stay easy.

## Test

Build with Docker (README "Build"), then run the pk3 on the q3-servers test
instance (UDP 27970, pk3 into its `test/` folder) together with a Quake3e
release that has the botmem syscalls, with bots on a TDM map, before a
release goes to the live servers.
