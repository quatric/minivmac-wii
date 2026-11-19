# Mini vMac for the Wii

A Wii port of Mini vMac based on the 3DS port in this repository: one app
with every Mac Mini vMac emulates, from the Macintosh 128K to the IIx. The
emulated Mac screen is drawn with GX across the whole TV picture: the video
line is widened to the full 720 pixels so there are no side borders, on 4:3
and 16:9 TVs alike.

* **Input** comes from [libwiiinput](https://github.com/quatric/libwiiinput):
  Wii Remote (IR pointer), Nunchuk, Classic Controller, GameCube pads, USB
  keyboards and USB mice.
* **Typing** uses [libwiiosk](https://github.com/quatric/libwiiosk), the
  Wii-style software keyboard: press + and type, and the text is typed into
  the Mac when you press OK.
* **Updates** come from [libwiiupdate](https://github.com/quatric/libwiiupdate):
  at start the app checks the GitHub releases in the background and, when a
  newer one is out, offers to install it over itself.
* **Sound** plays through ASND: the compact Macs' sound buffer and the Mac
  II's Apple Sound Chip (startup chime included) at 22255 Hz. HOME > *Sound*
  sets the volume (off, 25% ... 100%). As in every Mini vMac build, sound is
  only right at 1x; at higher speeds the emulator drops back to 1x on its own
  while the Mac is idle.

## Macs

| Model | ROM | Screen | RAM |
|-------|-----|--------|-----|
| Macintosh 128K | `Mac128K.ROM` | 512x342, black and white | 128 KB |
| Macintosh 512Ke | `vMac.ROM` | 512x342, black and white | 512 KB |
| Macintosh Plus | `vMac.ROM` | 512x342, black and white | 4 MB |
| Macintosh SE | `MacSE.ROM` | 512x342, black and white | 4 MB |
| Macintosh Classic | `Classic.ROM` | 512x342, black and white | 4 MB |
| Macintosh II | `MacII.ROM` | 640x480, 256 colours | 8 MB |
| Macintosh IIx | `MacIIx.ROM` | 640x480, 256 colours | 8 MB |

When the app starts it lists the Macs and which ROMs it found. The Mac used
last (or, the first time, the most capable one with a ROM) starts after
three seconds unless you pick another with the D-Pad and A; with a single
ROM there is nothing to choose and it starts at once. HOME > *Switch Mac
model...* goes back to the list. HOME there quits to the loader.

## Installing

1. Unzip `MinivMacWii-<version>-hbc.zip` to the root of the SD card or USB
   drive.
2. Copy your ROMs into the app folder (`sd:/apps/MinivMacWii/`), or into a
   model's own folder there (`sd:/apps/MinivMacWii/II/MacII.ROM`).
   `sd:/minivmac/` and the root of the card are searched as well.
3. Put disk images next to them. `disk1.dsk`, `disk2.dsk`, ... are inserted
   at start, those in the model's folder (`II/disk1.dsk`) before those in
   the app folder; any other image can be inserted from the HOME menu.

No ROM for a model? It starts anyway and HOME > *Load ROM...* picks one.

## Controls

| Wii Remote | Mac |
|------------|-----|
| Point at the screen | move the mouse |
| A or B (Nunchuk Z) | mouse button |
| D-Pad | arrow keys |
| 1 (Nunchuk C) | command |
| 2 | option |
| + | type text (libwiiosk keyboard); while text is being typed, + stops it |
| - | display mode: fill screen / keep aspect / integer scale |
| HOME | menu: insert disk, display, smoothing, speed, sound, type text, Mac keyboard, interrupt, reset, switch Mac model, quit |
| Nunchuk / Classic / GameCube stick | move the mouse |

A Classic Controller or GameCube pad works through the same layout (on a
GameCube pad R or Z is +, Start is HOME). On a USB
keyboard Alt is command and the Windows key is option; Ctrl is Mini vMac's
own control key (Ctrl+H lists its commands). A USB mouse moves the pointer
directly.

### Typing text

+ opens the Wii-style keyboard from libwiiosk (the console's language picks
its layout). Type with the pointer, the D-Pad and A, or a USB keyboard;
Enter starts a new line. OK (or + on the keyboard) closes it and the text is
typed into the Mac as if on a US Mac keyboard, about 30 characters a second:
capitals and symbols with shift, accented letters through the option dead
keys (option-e then e for é) and Mac Roman symbols (€, ©, ™, ...) with
option. Characters the Mac cannot type are skipped. Quit or HOME closes the
keyboard without typing. Press + again while the text is still being typed
to stop it.

### Mac keyboard

For keys the text keyboard does not have (command shortcuts, escape, arrows,
held modifiers), HOME > *Show Mac keyboard* puts a Mac keyboard over the
screen. Point and press A. Shift, option, command and ctrl apply to the next
key; caps locks. + hides it again.

Display mode, smoothing, speed and sound volume are saved to `settings.txt`
in the app folder, the last Mac to `model.txt`. The power button (console or
Wii Remote) turns the Wii off, the reset button returns to the loader.

## Building

Needs devkitPPC and libogc.

```sh
git submodule update --init
export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC
cd wii
make            # MinivMacWii.dol, every model
make dist       # dist/apps/MinivMacWii/ and the HBC zip
```

Variables: `VERSION` (default `1.0.0`, compared with the release tags),
`UPDATE_OWNER` (default `quatric`) and `UPDATE_REPO` (default
`minivmac-wii`).

### How the models share one app

Mini vMac picks the emulated Mac at compile time, so the Makefile builds
the whole of Mini vMac (emulator and the Wii glue, `src/OSGLUWII.c`) once
per model with that model's configuration, partially links each to
`build/<model>/core.o` and renames its global symbols `mv_<model>_...`.
`launcher.c` is the real `main`: it shows the list, runs
`mv_<model>_main` and, after *Switch Mac model*, puts the model's data and
bss (gathered by `core.ld`) back to their initial state before it runs a
model again. The models are compiled with `-G0` so none of their variables
land in the small data sections.

`models.h` lists the models. Their configuration headers, `cfg/<model>/`,
come from Mini vMac's setup tool: `make cfg` (or `./mkcfg.sh II`) generates
the 3DS configuration and swaps the platform includes for libogc and enables
big-endian access.

### Testing in Dolphin

`scripts/dolphin-test.sh` boots the app in Dolphin from a virtual SD card
(needs mtools), dumps the audio and optionally the frames, and stops Dolphin:

```sh
SECONDS_RUN=40 FRAMES=True scripts/dolphin-test.sh out MacII.ROM disk1.dsk=II/disk1.dsk
# out/dsp.wav: what the Wii played, out/frames/: the screen
```

`MODEL=II` saves II as the last Mac used. Build with
`EXTRA_CFLAGS=-DTEST_SHOW_UI` to open the HOME menu and the Mac keyboard
from the start, or `EXTRA_CFLAGS=-DTEST_AUTOSWITCH=25` to switch to the next
Mac with a ROM every 25 seconds.

### CI

`.github/workflows/build.yml` builds the app in the `devkitpro/devkitppc`
container on every push and pull request and uploads the HBC zip as an
artifact. Pushes (and manual runs) on the default branch refresh a rolling
`latest` release (the updater ignores that tag). A `v1.2.0` tag builds
version 1.2.0 and releases it on this repository.
