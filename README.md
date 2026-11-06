# osp

**Quake 3 Arena OSP mod** for the [doomwarriors](https://www.doomwarriors.de)
servers: the recovered OSP 1.03a sources with security fixes, plus bot
learning ("botmem") in the server module. Released as **OSP 1.03a-dwc**.

## Origin

[JKornev/xq3osp](https://github.com/JKornev/xq3osp) is reverse-engineered
source code of **OSP Tourney DM/CA/CTF 1.03a**, the mod by the OrangeSmoothie
team whose source was never released. All three game modules were restored
from the shipped QVM bytecode; compiled with id Software's original 1.32b
`q3lcc` and `q3asm`, its `main` branch produces files byte-identical to the
QVMs OSP shipped in 2002 and 2003:

| Module | Shipped in | SHA-256 of the original QVM |
|---|---|---|
| `qagame` (server) | `zz-osp-server3a.pk3`, 4 Mar 2003 | `7ce6a6ac1fcb444206461e19129d4f5cd1c4ba79a3cbf881653e6450b8277160` |
| `cgame` (client) | `zz-osp-pak3.pk3`, 28 Feb 2003 | `48e17665b0b274baa95670364364be373fbbda4aea6e74c1fb587aaa5feb33b3` |
| `ui` (menus) | `zz-osp-pak1.pk3`, 13 Jan 2002 | `36133dea02f56992a8683f852893a414e330cc87063895671ee268b51891147c` |

JKornev's `security-patches` branch fixes memory-safety and input-validation
bugs of 1.03a (release
[1.03a-secured](https://github.com/JKornev/xq3osp/releases/tag/1.03a-secured)).

## Branches

| Branch | What |
|---|---|
| `main` | JKornev's `security-patches` plus our changes (botmem); releases are built from here |
| `security-patches` | mirror of JKornev's branch, for merging his new fixes into `main` |

Changes go through pull requests against `main`. New fixes from JKornev:
update `security-patches` from upstream, then merge it into `main`.

## What we change

Only the server module `qagame`:

- **botmem** (`src/game/g_botmem.c`, hooks in `ai_dmnet.c` and `g_main.c`):
  bots remember per map which goals paid off and weight their goal choice
  accordingly. Off unless `bot_memory 1`; `bot_learn_enable` collects,
  `bot_learn_use` applies. Learn files go to `fs_homepath/osp/bot_memory/`
  (`learn_<map>.txt`, `<map>.mem`).
- It needs the botmem syscalls of
  [doomwarriors-clan/Quake3e](https://github.com/doomwarriors-clan/Quake3e)
  (branch `dwc`). The QVM probes for them via `trap_GetValue` at startup and
  runs in log-only mode on engines without them, so it also works on stock
  Quake3e.

`cgame` and `ui` stay byte-identical to 1.03a-secured; the build checks that.
Clients therefore use JKornev's `zz-osp-pak5.pk3` and need nothing from us.

## Build

Only Docker is needed:

```sh
docker build -f docker/Dockerfile -t xq3osp-build .
docker run --rm -v "${PWD}:/work" -w /work xq3osp-build make -C build MODULE=qagame all
```

The result is `build/vm/qagame.qvm` (`MODULE=cgame` / `ui` likewise; `info`
prints size and SHA-256). `make verify` compares with the original 1.03a
hashes and only passes on JKornev's unpatched `main`.

## Releases

Push a tag `1.03a-dwc.N` on `main`. The `build` workflow builds all modules,
checks `cgame`/`ui` against 1.03a-secured and attaches to the release:

- `zzz-osp-dwcN.pk3`: our `qagame`, for the server's `osp` folder next to the
  original OSP pk3s and 1.03a-secured's two pk3s. The name sorts after
  `zz-osp-*`, so it wins; it never reuses an OSP or xq3osp file name, because
  clients with a same-named pk3 of other content fail the pure check.
- the loose QVMs, a zip of them and `SHA256SUMS.txt`.

Built files are never committed. On the servers
([doomwarriors-clan/q3-servers](https://github.com/doomwarriors-clan/q3-servers))
a release runs on the test instance first.

## Credits

- OSP Tourney by the OrangeSmoothie team
- Source restoration: [JKornev/xq3osp](https://github.com/JKornev/xq3osp),
  building on [BigIndian](https://github.com/snems/BigIndian),
  [OSP2](https://github.com/snems/OSP2), [q3vm](https://github.com/brugal/q3vm),
  [baseq3a](https://github.com/ec-/baseq3a),
  [Quake III Arena 1.32b source](https://github.com/id-Software/Quake-III-Arena),
  dlmalloc by Doug Lea
- botmem: Hannes (dwc.motter), doomwarriors clan

## License

Not settled yet. OSP Tourney is © OrangeSmoothie Productions and was released
as freeware without source. The recovered sources in JKornev/xq3osp carry no
license; permission to fork and redistribute is still to be asked. Our own
additions (botmem) will follow the terms of the sources they extend once those
are clear.
