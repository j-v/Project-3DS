# Project 3DS: MegaSlide CoolMix+

Project DIVA for the Nintendo 3DS!

**Project 3DS: MegaSlide CoolMix+** is a port of [Project DS](https://github.com/Hydr8gon/Project-DS), the
open-source rhythm game for the Nintendo DS that plays _Hatsune Miku: Project DIVA_
charts and music.

## Installation

**Song Library:** If you have Project DS already installed, it will use the same library automatically - **BUT** you must have the OGG audio files present (raw PCM not presently supported). For a new installation, DSC and OGG files dumped from a compatible game should be placed in the `project-ds/dsc` and `project-ds/ogg` folders on the root of your SD card. To load song names and other information, database files ending in _db.txt should be placed in `project-ds/db`. You can follow [Hydr8gon's video guide](https://www.youtube.com/watch?v=ZQ4uYyCW7aA), but make sure you copy the OGG files to the SD card and not the raw PCM files.

**Game executable:** From the Releases page, copy the latest `cia` to your 3DS and install using FBI, or copy the `3dsx` and open it with Homebrew Launcher

### Releases

Releases use semantic versioning and are built from tags by GitHub Actions.
The installed build carries its version (`v0.1.0`) in the SMDH description.
Release archives include the licence and notice files, and the release notes
link to the source code at the tag. See [docs/releasing.md](docs/releasing.md)
for the tag format and release steps.

## Building

The script needs [Docker](https://www.docker.com/). 

```sh
./build.sh
```

The script downloads `devkitpro/devkitarm:latest` Docker image if it is missing, and it can be changed with the `DKP_IMAGE` environment variable. 

`./build.sh cia` downloads `makerom` and builds a `cia` file. The Home Menu
banner in `assets/banner.bnr` is included by default; set
`BANNER=/path/to/banner.bnr` to use a different one.

### Editor

Extract devkitpro headers from the Docker container and configure VSCode intellisense to read them:

```sh
./tools/setup-intellisense.sh
```


### Diagnostic build

`./build.sh diag` builds `project-3ds-diag.3dsx` in `build-diag/` instead of
replacing the game. It measures the clock for a few seconds at startup and
shows the result on the top screen, and it appends its findings to
`sdmc:/project-3ds-diag.log`. It is meant for checking the timing of a
console, not for playing.

### Running the tests

The gameplay core has no platform calls in it, so it compiles with a normal
host compiler, and the tests run without a console:

```sh
make -f Makefile.tests test
```

The tests can also be run in the container, which is handy when the host
compiler is not the one the project is built with:

```sh
./build.sh test
```

The outcomes of the differential test are frozen in `tests/fixtures/`: the
same chart and the same scripted input were run through the original DS
engine and the ported one, and the two agreed on every judgement, on the
score, on the clear percentage, on the life total and on the maximum combo.


### The project

```
src/core/      the gameplay core, free of platform calls
src/platform/  the 3DS application
tests/         the host-native tests and the shim for the original engine
gfx/           the artwork
assets/        the Home Menu banner and its source image
tools/         the script that prepares the artwork
```

## Licence

Project 3DS is free software, and is distributed under the terms of the GNU
General Public License, version 3 or later (GPL-3.0-or-later), like Project DS.
The original author of Project DS is Hydr8gon, and the copyright of the
original work is kept. See [NOTICE.md](NOTICE.md) for the details and
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the libraries the build
links.
