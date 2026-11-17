# Notice

Project 3DS is a derivative work of [Project DS](https://github.com/Hydr8gon/Project-DS),
which is an open-source implementation of the Japanese rhythm games *Hatsune
Miku: Project DIVA Future Tone* and *Hatsune Miku: Project DIVA Mega Mix* for
the Nintendo DS.

## Copyright

* Project DS: Copyright 2022-2024 Hydr8gon
* Project 3DS: Copyright 2026 Project 3DS contributors

The files in `src/` are ported from, or written from, Project DS. They keep
the upstream copyright notice and carry a notice that they were modified for
this port. The files in `tests/` are new code written for this port.

## Licence

Project 3DS is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Project 3DS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Project 3DS. If not, see <https://www.gnu.org/licenses/>.

The full text is in [LICENSE](LICENSE). The source files carry a short SPDX
licence notice instead of repeating the full text. Project DS is distributed
under the same licence, version 3 or later; its copyright and licence notices
are kept in the files that came from it.

## The artwork

The note artwork, the holes, the arrows, the judgement labels and the life
gauge are the artwork of Project DS, with the transparent colour of the DS
build made transparent and the multi-note variant of the notes recoloured,
which is done by `tools/make_assets.py`. They are distributed under the same
licence as the rest of the project, as received from Project DS.

The Home Menu icon and banner use ["SvgMiku3.svg"](https://commons.wikimedia.org/wiki/File:SvgMiku3.svg)
by Granjita, published on Wikimedia Commons under
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). The artwork was
rendered, scaled and composited onto new backgrounds for `gfx/icon.png` and
`assets/banner.png`, the latter packed as `assets/banner.bnr`. CC0 does not
require attribution, which is given here anyway. Hatsune Miku is a character
of Crypton Future Media; the CC0 dedication covers this fan artwork, not the
character, and no endorsement is implied.

The charts, the music, the lyrics and the other game data are not part of this
project. They remain the property of their respective owners, and they are not
distributed here.

## The font

The interface uses the Liberation font, converted to BCFNT when the project is
built. The font is licensed under the SIL Open Font License, version 1.1; the
licence is in [fonts/LICENSE-Liberation.txt](fonts/LICENSE-Liberation.txt).
The font is bundled and embedded with the software, and it is not sold on its
own.

## Third-party libraries

The build links the devkitPro 3DS libraries and `libvorbisidec` (libtremor).
Those libraries keep the copyright notices and licences of their authors; they
are not covered by this project's licence. Keep their notices with any binary
distribution.
