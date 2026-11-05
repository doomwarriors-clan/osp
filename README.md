# osp

**Quake 3 Arena OSP mod** (recovered, byte-identical, security patched, bot learning added)

This is a reverse-engineered source code of **OSP Tourney DM/CA/CTF 1.03a** mod for Quake III
Arena, the mod by the OrangeSmoothie team whose source was never
released. All three game modules were restored from the shipped QVM bytecode. Compiled with
id Software's original 1.32b `q3lcc` and `q3asm`, the sources produce files that are
**byte-identical** to the QVMs OSP shipped in 2002 and 2003:

| Module | Shipped in | SHA-256 of the QVM |
|---|---|---|
| `qagame` (server) | `zz-osp-server3a.pk3`, 4 Mar 2003 | `7ce6a6ac1fcb444206461e19129d4f5cd1c4ba79a3cbf881653e6450b8277160` |
| `cgame` (client) | `zz-osp-pak3.pk3`, 28 Feb 2003 | `48e17665b0b274baa95670364364be373fbbda4aea6e74c1fb587aaa5feb33b3` |
| `ui` (menus) | `zz-osp-pak1.pk3`, 13 Jan 2002 | `36133dea02f56992a8683f852893a414e330cc87063895671ee268b51891147c` |

## Build

TODO: you need a tool chain (lcc, etc.). Currently, it's build locally with a local 32-bit lcc (since id software used 32-bit lcc). We need to check if 64-bit lcc is a better choice.

## Credits

The restoration would not have been possible without these projects:

- [BigIndian](https://github.com/snems/BigIndian)
- [OSP2](https://github.com/snems/OSP2)
- [q3vm](https://github.com/brugal/q3vm)
- [baseq3a](https://github.com/ec-/baseq3a)
- [Quake III Arena 1.32b source](https://github.com/id-Software/Quake-III-Arena)
- dlmalloc by Doug Lea
- OSP Tourney by the OrangeSmoothie team

## License

Fuck license. All rights reversed ®
