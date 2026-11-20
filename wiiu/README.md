# Mini vMac for the Wii U

A Wii U port of Mini vMac for [Aroma](https://aroma.foryour.cafe/), built
with wut and SDL2 from the same emulator sources as the Wii port. The Mac
screen is shown on the TV and on the GamePad at once.

* **Input** comes from [libwiiuinput](https://github.com/quatric/libwiiuinput):
  the GamePad (buttons and touch screen), Wii Remotes with Nunchuk or
  Classic Controller, Wii U Pro Controllers, the GameCube controller adapter,
  USB game controllers, USB keyboards and USB mice.
* **Typing** uses [libwiiosk](https://github.com/quatric/libwiiosk), the same
  Wii-style software keyboard as on the Wii, drawn through SDL2: press + and
  type, and the text is typed into the Mac when you press OK.
* **Updates** come from [libwiiuupdate](https://github.com/quatric/libwiiuupdate):
  at start the app checks the GitHub releases in the background and, when a
  newer one is out, offers to install it over itself.

One app with every Mac: Macintosh 128K, 512Ke, Plus, SE, Classic, II and IIx
(the II and IIx have a 640x480, 256 colour screen). Mini vMac picks the Mac
at compile time, so the app holds one build of the emulator per model behind
a launcher, as the Wii app does. The launcher lists the Macs and the ROMs it
found; the one you used last starts after three seconds, or pick another with
the D-Pad, a pointer or the touch screen. With a single ROM it starts at once.
HOME > *Switch Mac model...* comes back to the list.

| Mac | ROM |
|-----|-----|
| Macintosh 128K | `Mac128K.ROM` |
| Macintosh 512Ke, Macintosh Plus | `vMac.ROM` |
| Macintosh SE | `MacSE.ROM` |
| Macintosh Classic | `Classic.ROM` |
| Macintosh II | `MacII.ROM` |
| Macintosh IIx | `MacIIx.ROM` |

## Installing

1. Unzip `MinivMacWiiU-<version>-wiiu.zip` to the root of the SD card. You get
   `wiiu/apps/MinivMacWiiU/` with one empty folder per Mac in it.
2. Copy the ROMs into `sd:/wiiu/apps/MinivMacWiiU/`, or into the Mac's folder
   (`II/MacII.ROM`). `sd:/minivmac/` and the root of the card are searched
   as well.
3. Put disk images in the Mac's folder (`II/disk1.dsk`); images next to the
   app are used by every Mac that has none of its own. `disk1.dsk`,
   `disk2.dsk`, ... are inserted at start; any other image can be inserted
   from the HOME menu.
4. Open Mini vMac Wii U from the Wii U Menu.

No ROM? The app starts anyway and HOME > *Load ROM...* picks one.

## Controls

| GamePad / Pro Controller | Mac |
|--------------------------|-----|
| Touch the screen | move the mouse there and click |
| Wii Remote: point at the screen | move the mouse |
| A or B (Nunchuk Z) | mouse button |
| Left stick | move the mouse |
| D-Pad | arrow keys |
| Y (Wii Remote 1, Nunchuk C) | command |
| X (Wii Remote 2) | option |
| + / R / ZR | type text (libwiiosk keyboard); while text is being typed, + stops it |
| - / L / ZL | display mode: fill screen / keep aspect / integer scale |
| HOME | menu: insert disk, display, smoothing, speed, type text, Mac keyboard, interrupt, reset, switch Mac model, quit |

HOME opens the emulator's menu instead of the HOME Menu; *Quit* in it goes
back to the Wii U Menu. On a USB keyboard Alt is command and the Windows key
is option; Ctrl is Mini vMac's own control key (Ctrl+H lists its commands).

### Typing text

+ opens the Wii-style keyboard from libwiiosk (the console's language picks
its layout). Touch the keys on the GamePad, point with a Wii Remote, use the
D-Pad and A, or type on a USB keyboard; Enter starts a new line. OK closes it
and the text is typed into the Mac as on a US Mac keyboard: capitals and
symbols with shift, accented letters through the option dead keys (option-e
then e for é) and Mac Roman symbols (€, ©, ™, ...) with option. Quit or HOME
closes the keyboard without typing.

### Mac keyboard

For keys the text keyboard does not have (command shortcuts, escape, arrows,
held modifiers), HOME > *Show Mac keyboard* puts a Mac keyboard over the
screen.

Display mode, smoothing and speed are saved to `settings.txt` in the app
folder.

## Building

Needs devkitPPC, wut and `wiiu-sdl2`.

```sh
git submodule update --init
export DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC
cd wiiu
make            # MinivMacWiiU.wuhb (the launcher and every model)
make dist       # dist/wiiu/apps/MinivMacWiiU and its zip
make cfg        # regenerate cfg/ from the Wii's (mkcfg.sh)
```

The variables are the Wii build's: `VERSION`, `UPDATE_OWNER` and
`UPDATE_REPO`. CI puts the Wii U zip on the same releases as the Wii zip:
libwiiuupdate picks the zip named for the Wii U (`-wiiu.zip`) and
libwiiupdate never takes it.

Each model is compiled with its own configuration, partially linked and its
globals renamed `mv_<model>_...` (the Makefile has the details); `launcher.c`
owns SDL, the window and the controllers and runs one model at a time,
resetting its data and bss (gathered by `core.ld`) before it starts again.

The glue is `src/OSGLUWIU.c`. The configuration headers in `cfg/` are the
Wii ones with the platform includes swapped for wut. `src/WIIUFONT.h` is the
8x16 VGA console font the Wii build gets from libogc.
