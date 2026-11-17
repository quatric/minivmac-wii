# Mini vMac for the Wii

A Wii port of Mini vMac based on the 3DS port in this repository. The emulated
Mac screen is drawn with GX across the whole TV picture: the video line is
widened to the full 720 pixels so there are no side borders, on 4:3 and 16:9
TVs alike.

* **Input** comes from [libwiiinput](https://github.com/quatric/libwiiinput):
  Wii Remote (IR pointer), Nunchuk, Classic Controller, GameCube pads, USB
  keyboards and USB mice.
* **Updates** come from [libwiiupdate](https://github.com/quatric/libwiiupdate):
  at start the app checks the GitHub releases in the background and, when a
  newer one is out, offers to install it over itself.

Two builds:

| Model | HBC folder | ROM | Screen |
|-------|------------|-----|--------|
| Macintosh Plus | `apps/MinivMacWii` | `vMac.ROM` | 512x342, black and white |
| Macintosh II | `apps/MinivMacWiiII` | `MacII.ROM` | 640x480, 256 colours |

## Installing

1. Unzip `MinivMacWii-<version>-hbc.zip` (and/or the `MinivMacWiiII` one) to
   the root of the SD card or USB drive.
2. Copy the ROM into the app folder (`sd:/apps/MinivMacWii/vMac.ROM`).
   `sd:/minivmac/` and the root of the card are searched as well.
3. Put disk images next to it. `disk1.dsk`, `disk2.dsk`, ... are inserted at
   start; any other image can be inserted from the HOME menu.

No ROM? The app starts anyway and HOME > *Load ROM...* picks one.

## Controls

| Wii Remote | Mac |
|------------|-----|
| Point at the screen | move the mouse |
| A or B (Nunchuk Z) | mouse button |
| D-Pad | arrow keys |
| 1 (Nunchuk C) | command |
| 2 | option |
| + | show/hide the on-screen keyboard |
| - | display mode: fill screen / keep aspect / integer scale |
| HOME | menu: insert disk, display, smoothing, speed, interrupt, reset, quit |
| Nunchuk / Classic / GameCube stick | move the mouse |

A Classic Controller or GameCube pad works through the same layout. On a USB
keyboard Alt is command and the Windows key is option; Ctrl is Mini vMac's
own control key (Ctrl+H lists its commands). A USB mouse moves the pointer
directly.

The on-screen keyboard is used by pointing and pressing A. Shift, option,
command and ctrl apply to the next key; caps locks.

Display mode, smoothing and speed are saved to `settings.txt` in the app
folder. The power button (console or Wii Remote) turns the Wii off, the reset
button returns to the loader.

## Building

Needs devkitPPC and libogc.

```sh
git submodule update --init
export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC
cd wii
make            # MinivMacWii.dol (Plus) and MinivMacWiiII.dol (II)
make MODEL=II   # one model
make dist       # dist/apps/... and one HBC zip per model
```

Variables: `VERSION` (default `1.0.0`, compared with the release tags),
`UPDATE_OWNER` (default `quatric`), `UPDATE_REPO` (default `minivmac-wii`,
used by the Plus build) and `UPDATE_REPO_II` (default `minivmac-wii-ii`).

libwiiupdate installs the first `apps/<Name>/` folder of the release zip it
picks, so each model is released from its own repository: publish
`MinivMacWii-<version>-hbc.zip` on `UPDATE_REPO` and
`MinivMacWiiII-<version>-hbc.zip` on `UPDATE_REPO_II`.

### CI

`.github/workflows/build.yml` builds both models in the `devkitpro/devkitppc`
container on every push and pull request and uploads the HBC zips as
artifacts. Pushes to the default branch refresh a rolling `latest` release
(the updater ignores that tag). A `v1.2.0` tag builds version 1.2.0 and
releases the Plus zip on this repository; with an `II_RELEASE_TOKEN` secret
(a token that can write to `<owner>/<repo>-ii`, which needs at least one
commit) it also releases the II zip there.

The configuration headers in `cfg/Plus` and `cfg/II` are the 3DS ones with
the platform includes swapped for libogc and big-endian access enabled.
