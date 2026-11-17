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

Two builds:

| Model | Folder | ROM | Screen |
|-------|--------|-----|--------|
| Macintosh Plus | `wiiu/apps/MinivMacWiiU` | `vMac.ROM` | 512x342, black and white |
| Macintosh II | `wiiu/apps/MinivMacWiiUII` | `MacII.ROM` | 640x480, 256 colours |

## Installing

1. Unzip `MinivMacWiiU-<version>-wiiu.zip` (and/or the `MinivMacWiiUII` one)
   to the root of the SD card.
2. Copy the ROM into the app folder (`sd:/wiiu/apps/MinivMacWiiU/vMac.ROM`).
   `sd:/minivmac/` and the root of the card are searched as well.
3. Put disk images next to it. `disk1.dsk`, `disk2.dsk`, ... are inserted at
   start; any other image can be inserted from the HOME menu.
4. Open Mini vMac from the Wii U Menu.

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
| HOME | menu: insert disk, display, smoothing, speed, type text, Mac keyboard, interrupt, reset, quit |

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
make            # MinivMacWiiU.wuhb (Plus) and MinivMacWiiUII.wuhb (II)
make MODEL=II   # one model
make dist       # dist/wiiu/apps/... and one zip per model
```

The variables are the Wii build's: `VERSION`, `UPDATE_OWNER`, `UPDATE_REPO`
(the Plus) and `UPDATE_REPO_II`. CI puts the Wii U zips on the same releases
as the Wii zips: libwiiuupdate picks the zip named for the Wii U (`-wiiu.zip`)
and libwiiupdate never takes it.

The glue is `src/OSGLUWIU.c`. The configuration headers in `cfg/` are the
Wii ones with the platform includes swapped for wut. `src/WIIUFONT.h` is the
8x16 VGA console font the Wii build gets from libogc.
