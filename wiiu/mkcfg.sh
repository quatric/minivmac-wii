#!/bin/sh
# Regenerates wiiu/cfg/<Model>/ from the Wii's configurations (wii/cfg, made
# by wii/mkcfg.sh with the Mini vMac setup tool): the emulator configuration
# is the same, only the platform includes and the build name change.
#
#   wiiu/mkcfg.sh [MODEL...]     default: every model the Wii U app includes
set -eu
here=$(cd "$(dirname "$0")" && pwd)

models=${*:-128K 512Ke Plus SE Classic II IIx}
for m in $models; do
    src="$here/../wii/cfg/$m"
    out="$here/cfg/$m"
    mkdir -p "$out"
    cp "$src/CNFGGLOB.h" "$src/EMCONFIG.h" "$src/STRCONST.h" "$out/"
    awk '
        /^#include <gccore.h>$/ {
            print "#include <wut.h>"
            print "#include <padscore/wpad.h>"
            print "#include <vpad/input.h>"
            next
        }
        /^#include <ogc\/lwp_watchdog.h>$/ || /^#include <fat.h>$/ \
            || /^#include <asndlib.h>$/ || /^#include <wiiuse\/wpad.h>$/ { next }
        { gsub(/minivmac-wii/, "minivmac-wiiu"); gsub(/-t wii/, "-t wiiu"); print }
    ' "$src/CNFGRAPI.h" > "$out/CNFGRAPI.h"
    echo "cfg/$m"
done
