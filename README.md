# Mini vMac for the Wii and Wii U

This repository holds the 3DS port of Mini vMac and, built on it, ports to
the Nintendo Wii and Wii U:

* [wii/README.md](wii/README.md): Homebrew Channel (libogc, GX), one app
  with every model: Macintosh 128K, 512Ke, Plus, SE, Classic, II and IIx
* [wiiu/README.md](wiiu/README.md): Aroma (wut, SDL2)

Both type text through [libwiiosk](https://github.com/quatric/libwiiosk), the
Wii-style software keyboard: press + and type.

The 3DS build notes follow.

# BUILDING
3dsconf.sh Is a wrapper around the main Mini vMac source configuration tool.
It contains the options common to the 3DS port and any options passed to it will be passed on to the Mini vMac configuration tool.

See:
https://www.gryphel.com/c/minivmac/options.html
https://www.gryphel.com/c/minivmac/develop.html

3dsconf.sh Deletes the generated Makefile because I have not figured out how to integrate that part into the Mini vMac build system.
To actually build the binary you need to use Makefile.Plus for Macintosh Plus or Makefile.II for Macintosh II based builds.

# Supported options:
1 Bit colour depth
2 Bit colour depth
4 Bit colour depth
8 Bit colour depth

Resolutions >= 512x384
Resolutions <= 1024x1024

Sound
Sound sample size

...And many more, feel free to experiment :)
