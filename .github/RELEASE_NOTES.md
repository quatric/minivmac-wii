Mini vMac, the classic Macintosh emulator, for the Wii (Homebrew Channel) and the Wii U (Aroma).

## Downloads

| File | Console | Macs |
|------|---------|------|
| `MinivMacWii-<version>-hbc.zip` | Wii, or a Wii U in Wii mode | 128K, 512Ke, Plus, SE, Classic, II, IIx (one app, pick at start) |
| `MinivMacWiiU-<version>-wiiu.zip` | Wii U with Aroma | 128K, 512Ke, Plus, SE, Classic, II, IIx (one app, pick at start) |

## You will need

* **A ROM** from the Mac you want to emulate. Mini vMac cannot include one: copy it from your own Mac. Each model looks for its own file name:

  | Mac | ROM file |
  |-----|----------|
  | Macintosh 128K | `Mac128K.ROM` |
  | Macintosh 512Ke, Macintosh Plus | `vMac.ROM` |
  | Macintosh SE | `MacSE.ROM` |
  | Macintosh Classic | `Classic.ROM` |
  | Macintosh II | `MacII.ROM` |
  | Macintosh IIx | `MacIIx.ROM` |

* **Disk images** (`.dsk`) with a System Folder to boot from, plus any software. Name the startup disk `disk1.dsk` and further ones `disk2.dsk`, `disk3.dsk`, ...: they are inserted when the Mac starts.
* **An SD card or USB drive** (FAT32).

## Wii setup

1. Install the [Homebrew Channel](https://wii.hacks.guide/) if you do not have it.
2. Unzip `MinivMacWii-<version>-hbc.zip` to the root of the SD card or USB drive. You get `apps/MinivMacWii/`, with one empty folder per Mac inside it (`128K`, `512Ke`, `Plus`, `SE`, `Classic`, `II`, `IIx`).
3. Copy your ROMs into `apps/MinivMacWii/` (or into the model's folder, e.g. `apps/MinivMacWii/II/MacII.ROM`).
4. Copy your disk images into the model's folder, e.g. `apps/MinivMacWii/II/disk1.dsk`. Images in `apps/MinivMacWii/` itself are used by every model that has none of its own.
5. Open **Mini vMac Wii** from the Homebrew Channel. The list shows which ROMs were found; the Mac you used last starts after three seconds, or pick another with the D-Pad and A. With a single ROM it starts straight away.

Example layout for a Macintosh II and a Macintosh Plus:

```
sd:/apps/MinivMacWii/
    boot.dol  icon.png  meta.xml
    II/MacII.ROM
    II/disk1.dsk          System 7 startup disk
    Plus/vMac.ROM
    Plus/disk1.dsk        System 6 startup disk
```

### Wii controls

| Wii Remote | Mac |
|------------|-----|
| Point at the screen | move the mouse |
| A or B (Nunchuk Z) | mouse button |
| D-Pad | arrow keys |
| 1 (Nunchuk C) / 2 | command / option |
| + | type text with the on-screen keyboard |
| - | display mode: fill screen / keep aspect / integer scale |
| HOME | menu: insert disk, display, smoothing, speed, sound volume, Mac keyboard, interrupt, reset, **switch Mac model**, quit |

Nunchuk, Classic Controller, GameCube controllers, USB keyboards and USB mice work too (on a USB keyboard Alt is command).

## Wii U setup (Aroma)

1. Unzip `MinivMacWiiU-<version>-wiiu.zip` to the root of the SD card. You get `wiiu/apps/MinivMacWiiU/`, with one empty folder per Mac inside it (`128K`, `512Ke`, `Plus`, `SE`, `Classic`, `II`, `IIx`).
2. Copy your ROMs into `sd:/wiiu/apps/MinivMacWiiU/` (or into the model's folder, e.g. `II/MacII.ROM`).
3. Put `disk1.dsk`, `disk2.dsk`, ... in the model's folder, e.g. `II/disk1.dsk`. Images in `MinivMacWiiU/` itself are used by every model that has none of its own.
4. Open **Mini vMac Wii U** from the Wii U Menu. The list shows which ROMs were found; the Mac you used last starts after three seconds, or pick another with the D-Pad, a pointer or the GamePad's touch screen. With a single ROM it starts straight away.
5. Touch the GamePad screen to move the mouse and click; HOME opens the emulator's menu, which has *Switch Mac model...*.

## Tips

* No ROM for a Mac? It starts anyway, and HOME > *Load ROM...* lets you pick one from the card.
* HOME > *Insert disk image...* inserts any other image while the Mac runs.
* Sound is only right at 1x speed; set *Speed* to 1x in the HOME menu for games and music.
* The app checks this page for updates when it starts and offers to install a newer version over itself.
* Quit Mac OS with *Shut Down* before leaving, so disk images are not left dirty.
