# Ground0 licensing

## Ground0's own code
Copyright (c) 2026 Irak4t0n. Released under the MIT licence (see [`LICENSE`](../LICENSE) for the text). This covers `projects/APPLaunch/main/ui/ground0_*`, `projects/APPLaunch/APPLaunch/share/ground0/` and everything under `ground0/`.

## The launcher it is built on
[CardputerZero/launcher](https://github.com/CardputerZero/launcher), Copyright (c) 2026 M5Stack Technology CO LTD, MIT. The upstream inventory of third-party components is in [`docs/OPEN_SOURCE_COMPONENTS.md`](../docs/OPEN_SOURCE_COMPONENTS.md), with the licence texts in [`LICENSES/`](../LICENSES/README.md).

## The compiled launcher binary is GPL-3.0
The launcher's settings pages use **tree.hh** by Kasper Peeters, which is licensed **GPL-3.0-only** and is compiled into the binary. The source files in this repository keep their own licences (MIT for Ground0 and for M5Stack's code), but the **compiled launcher, and therefore the `ground0` package, is distributed under the terms of the GNU General Public License version 3**. The complete corresponding source is this repository at the tag matching the package version, built as described in the README. The stock launcher binary is in the same position.

Other components linked into the binary: LVGL (MIT), LodePNG (zlib), eventpp (Apache-2.0), sigslot (MIT), miniaudio (Unlicense or MIT-0). Linked from the system at run time: FreeType (FTL), libcurl, GLib (LGPL-2.1-or-later), libinput and xkbcommon (MIT). SimpleBLE (BUSL-1.1) is present in the SDK download cache but is **not** compiled or linked into the launcher.

## Themes from 1999 Online
The fourteen classic desktop skins are adapted from the skin table of [1999 Online](https://github.com/GoatsAndMonkeys/1999-online) by GoatsAndMonkeys (GPL-3.0), used with the author's permission.

The skin names refer to the operating systems they resemble. Those names are trademarks of their respective owners; Ground0 is not affiliated with or endorsed by them.

## Fonts
JetBrains Mono (upstream) and the six fonts Ground0 adds — VT323, Press Start 2P, Chakra Petch, Orbitron, Share Tech Mono, Saira Stencil One — are under the SIL Open Font License 1.1. Each licence text sits beside its font in `projects/APPLaunch/APPLaunch/share/font/` and is installed with the package.

## Programs Ground0 talks to but does not include
- **Desk Mode** (`cz-desk-mode`) by InstantComet, MIT. Ground0 reads and writes that daemon's own status and request files; none of its code is in this repository.
- **0Hero**. Ground0 imports the installed updater at run time.
- **MeshZero**. Ground0 starts and stops its systemd unit.

## Screenshots and clips
The images under `ground0/docs/` are captures of Ground0 itself. App icons that appear in them belong to their apps' authors.
