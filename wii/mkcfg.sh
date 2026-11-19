#!/bin/sh
# Regenerates wii/cfg/<Model>/ with the Mini vMac setup tool: the 3DS
# configuration (which the Wii glue was ported from) with the platform
# includes swapped for libogc and big-endian access enabled.
#
#   wii/mkcfg.sh [MODEL...]     default: every model the Wii app includes
#
# Needs a host C compiler (cc).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cc -w -o "$tmp/tool" "$root/setup/tool.c"

models=${*:-128K 512Ke Plus SE Classic II IIx}
for m in $models; do
    case $m in
        II|IIx) extra="-depth 3 -hres 640 -vres 480" ;;
        *) extra="" ;;
    esac
    mkdir -p "$tmp/$m"
    # shellcheck disable=SC2086
    (cd "$tmp/$m" && "$tmp/tool" -t 3dsa -api 3ds -ta 0 -sound 1 \
        -n minivmac-3ds -an mnvm3ds -m "$m" $extra > setup.sh \
        && sh setup.sh > /dev/null)
    out="$here/cfg/$m"
    mkdir -p "$out"
    cp "$tmp/$m/cfg/EMCONFIG.h" "$tmp/$m/cfg/STRCONST.h" "$out/"

    awk '
        /^\/\* --- integer types ---- \*\/$/ && !done {
            print "/* PowerPC: big-endian, unaligned access is fine */"
            print "#define BigEndianUnaligned 1"
            print "#define LittleEndianUnaligned 0"
            print "#define my_cond_rare(x) (__builtin_expect(x, 0))"
            print "#define Have_ASR 1"
            print ""
            done = 1
        }
        { print }
    ' "$tmp/$m/cfg/CNFGGLOB.h" > "$out/CNFGGLOB.h"

    awk '
        /^#include <3ds.h>$/ {
            print "#include <gccore.h>"
            print "#include <ogc/lwp_watchdog.h>"
            print "#include <fat.h>"
            print "#include <asndlib.h>"
            print "#include <wiiuse/wpad.h>"
            print "#include <math.h>"
            print "#include <dirent.h>"
            print "#include <sys/stat.h>"
            next
        }
        /^#include <citro[23]d.h>$/ || /^#include <sys\/times.h>$/ { next }
        { gsub(/minivmac-3ds/, "minivmac-wii"); gsub(/-t 3dsa/, "-t wii"); print }
    ' "$tmp/$m/cfg/CNFGRAPI.h" > "$out/CNFGRAPI.h"
    echo "cfg/$m"
done
