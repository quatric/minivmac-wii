#!/bin/sh
# Boots wii/MinivMacWii.dol in Dolphin from a virtual SD card holding the
# app, ROMs and disk images, dumps the audio (and optionally frames), then
# stops Dolphin.
#
#   scripts/dolphin-test.sh OUTDIR FILES...
#
#   FILES   ROMs and disk images, copied into the app folder; SRC=DEST
#           copies SRC to DEST in the app folder (e.g. disk.dsk=II/disk1.dsk)
#
# Environment: SECONDS_RUN (default 60), MODEL (saved as the last model
# used, e.g. II), FRAMES=True to dump frames as images, DSPHLE=False for the
# low-level DSP, GFX (default Metal), DOLPHIN (path to the Dolphin binary).
# Output: OUTDIR/dsp.wav (the Wii's DSP mix at 48 kHz, after ASND starts;
# Dolphin writes the few frames before that to a separate file) and, with
# FRAMES, OUTDIR/frames/.
# Needs mtools.
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
out=$1; shift
app=MinivMacWii
dol="$here/wii/$app.dol"
[ -f "$dol" ] || { echo "build $dol first" >&2; exit 1; }
dolphin=${DOLPHIN:-/Applications/Dolphin.app/Contents/MacOS/Dolphin}
secs=${SECONDS_RUN:-60}

mkdir -p "$out"
work="$out/.dolphin"
rm -rf "$work"; mkdir -p "$work/user/Config"

size=64
for f in "$@"; do size=$((size + $(du -m "${f%%=*}" | cut -f1) + 1)); done
sd="$work/sd.img"
dd if=/dev/zero of="$sd" bs=1m count=0 seek=$size 2>/dev/null
mformat -i "$sd" -F ::
mmd -i "$sd" ::/apps ::/apps/$app
mcopy -i "$sd" "$dol" ::/apps/$app/boot.dol
for f in "$@"; do
    case $f in
        *=*) src=${f%%=*}; dst=${f#*=}
             mmd -i "$sd" "::/apps/$app/$(dirname "$dst")" 2>/dev/null || true
             mcopy -i "$sd" "$src" "::/apps/$app/$dst" ;;
        *) mcopy -i "$sd" "$f" ::/apps/$app/ ;;
    esac
done
if [ -n "${MODEL:-}" ]; then
    printf '%s\n' "$MODEL" > "$work/model.txt"
    mcopy -i "$sd" "$work/model.txt" ::/apps/$app/model.txt
fi

cat > "$work/user/Config/Dolphin.ini" <<INI
[General]
WiiSDCardPath = $sd
DumpPath = $work/dump/
[Core]
DSPHLE = ${DSPHLE:-True}
WiiSDCard = True
WiiSDCardAllowWrites = True
WiiSDCardEnableFolderSync = False
GFXBackend = ${GFX:-Metal}
[Interface]
ConfirmStop = False
UsePanicHandlers = False
[DSP]
EnableJIT = True
Backend = No Audio Output
DumpAudio = True
DumpAudioSilent = True
[Movie]
DumpFrames = ${FRAMES:-False}
DumpFramesSilent = True
[Analytics]
Enabled = False
PermissionAsked = True
INI
cat > "$work/user/Config/GFX.ini" <<INI
[Settings]
DumpFramesAsImages = True
[Hacks]
EFBToTextureEnable = False
XFBToTextureEnable = False
ImmediateXFBEnable = False
INI

# Dolphin is started with the app path as argv[0], like the Homebrew Channel.
"$dolphin" -u "$work/user" -b -e "$dol" -v "${GFX:-Metal}" >"$work/dolphin.log" 2>&1 &
pid=$!
i=0
while [ $i -lt "$secs" ]; do
    sleep 1; i=$((i + 1))
    kill -0 $pid 2>/dev/null || break
done
kill $pid 2>/dev/null || true
sleep 1; kill $pid 2>/dev/null || true
sleep 2; kill -9 $pid 2>/dev/null || true
wait $pid 2>/dev/null || true

wav=$(find "$work/user" "$work/dump" -name '*_dspdump1.wav' 2>/dev/null | head -1)
[ -n "$wav" ] && cp "$wav" "$out/dsp.wav" && echo "$out/dsp.wav"
fr=$(find "$work/user" "$work/dump" -type d -name Frames 2>/dev/null | head -1)
[ -n "$fr" ] && rm -rf "$out/frames" && cp -R "$fr" "$out/frames" && echo "$out/frames"
exit 0
