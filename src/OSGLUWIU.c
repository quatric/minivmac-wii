/*
	OSGLUWIU.c

	Operating System GLUe for the Nintendo Wii U (wut, Aroma).

	Based on OSGLU3DS.c / OSGLUSDL.c,
	Copyright (C) 2012 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.

	Video:  SDL2; one window shown on the TV and the GamePad. The
	        emulated screen is converted to an RGBA texture.
	Sound:  SDL2 audio.
	Input:  libwiiuinput (GamePad buttons and touch screen, Wii Remote
	        IR / Nunchuk / Classic, Pro Controller, GameCube adapter,
	        USB keyboard and mouse).
	Typing: libwiiosk, drawn through SDL2.
	Update: libwiiuupdate checks GitHub releases in the background and
	        offers to install a newer version.
*/

#include "CNFGRAPI.h"
#include "SYSDEPNS.h"
#include "ENDIANAC.h"

#include "MYOSGLUE.h"

#include "STRCONST.h"

#include <stdarg.h>
#include <malloc.h>
#include <errno.h>

#include <SDL2/SDL.h>
#include <coreinit/time.h>
#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <proc_ui/procui.h>
#include <sys/stat.h>

#include <input_wiiu.h>
#include <wiiuupdate.h>
#include <wiiosk_sdl.h>
#include <wiiosk_input_wiiu.h>

#ifndef APP_VERSION
#define APP_VERSION "1.1.0"
#endif
#ifndef UPDATE_OWNER
#define UPDATE_OWNER "quatric"
#endif
#ifndef UPDATE_REPO
#define UPDATE_REPO "minivmac-wii"
#endif
#ifndef APP_FOLDER
#define APP_FOLDER "MinivMacWiiU"
#endif
#ifndef APP_TITLE
#define APP_TITLE "Mini vMac Wii U"
#endif

/* Virtual screen used for all drawing and for the pointer: 16:9 with
   square pixels. SDL scales it onto the TV and the GamePad. */
#define VirtW 854
#define VirtH 480

/* --- forward declarations --- */

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail);
LOCALFUNC tMacErr LoadMacRomFrom(char *path);
LOCALPROC SaveSettings(void);

/* --- some simple utilities --- */

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#include "INTLCHAR.h"

/* --- sending debugging info to file --- */

#if dbglog_HAVE

LOCALVAR FILE *dbglog_File = NULL;

LOCALFUNC blnr dbglog_open0(void)
{
	dbglog_File = fopen("dbglog.txt", "w");
	return (NULL != dbglog_File);
}

LOCALPROC dbglog_write0(char *s, uimr L)
{
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
}

LOCALPROC dbglog_close0(void)
{
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
}

#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"

#include "PBUFSTDC.h"

#include "CONTROLM.h"

/* --- text translation --- */

LOCALPROC NativeStrFromCStr(char *r, char *s)
{
	ui3b ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

#define NotAfileRef NULL

LOCALVAR FILE *Drives[NumDrives]; /* open disk image files */

LOCALPROC InitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
	}
}

GLOBALOSGLUFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (0 == fseek(refnum, Sony_Start, SEEK_SET)) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err;
}

GLOBALOSGLUFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	long v;

	if (0 == fseek(refnum, 0, SEEK_END)) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err;
}

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No, blnr deleteit)
{
	FILE *refnum = Drives[Drive_No];

	UnusedParam(deleteit);

	DiskEjectedNotify(Drive_No);

	fclose(refnum);
	Drives[Drive_No] = NotAfileRef;

	return mnvm_noErr;
}

GLOBALOSGLUFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, falseblnr);
}

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

LOCALFUNC blnr Sony_Insert0(FILE *refnum, blnr locked)
{
	tDrive Drive_No;
	blnr IsOk = falseblnr;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			falseblnr);
	} else {
		Drives[Drive_No] = refnum;
		DiskInsertNotify(Drive_No, locked);

		IsOk = trueblnr;
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail)
{
	blnr locked = falseblnr;
	FILE *refnum = fopen(drivepath, "rb+");

	if (NULL == refnum) {
		locked = trueblnr;
		refnum = fopen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
		}
	} else {
		return Sony_Insert0(refnum, locked);
	}
	return falseblnr;
}

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE *ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = fread(ROM, 1, kROM_Size, ROM_File);
		if (File_Size != kROM_Size) {
			if (feof(ROM_File)) {
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		fclose(ROM_File);
	}

	return err;
}

LOCALFUNC blnr Sony_InsertIth(int i)
{
	blnr v;

	if ((i > 9) || ! FirstFreeDisk(nullpr)) {
		v = falseblnr;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert1(s, trueblnr);
	}

	return v;
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
			/* stop on first error (including file not found) */
		}
	}

	return trueblnr;
}

/* --- ROM --- */

LOCALVAR char *rom_path = NULL;

LOCALFUNC blnr LoadMacRom(void)
{
	static const char *const dirs[] = {
		"", "fs:/vol/external01/minivmac/", "fs:/vol/external01/"
	};
	char path[256];
	tMacErr err = mnvm_fnfErr;
	unsigned int i;

	if (NULL != rom_path) {
		err = LoadMacRomFrom(rom_path);
	}
	for (i = 0; (mnvm_fnfErr == err) && (i < sizeof(dirs) / sizeof(dirs[0]));
		++i)
	{
		snprintf(path, sizeof(path), "%s%s", dirs[i], RomFileName);
		err = LoadMacRomFrom(path);
	}

	return trueblnr; /* keep launching Mini vMac, regardless */
}

/* ======================================================================
	Video (SDL2)
   ====================================================================== */

/* the SD card, as wut mounts it */
#define SDRoot "fs:/vol/external01"

LOCALVAR SDL_Window *MyWindow = NULL;
LOCALVAR SDL_Renderer *MyRenderer = NULL;
LOCALVAR SDL_Texture *ScreenTex = NULL;
LOCALVAR SDL_Texture *FontTex = NULL;

/* The Wii U picture is always 16:9 and VirtW x VirtH has square pixels,
   so nothing is anamorphic here. */
LOCALVAR blnr WideScreen = falseblnr;

/* 60 Hz frame counter, the Wii U's stand-in for the Wii's retrace count */
LOCALVAR OSTime StartTime = 0;

LOCALFUNC u32 FrameCounter(void)
{
	return (u32)((uint64_t)OSTicksToMicroseconds(OSGetSystemTime() - StartTime)
		* 3 / 50000);
}

#define RetraceCount FrameCounter()

#include "WIIUFONT.h"

#define FontTexW 128
#define FontTexH 256

LOCALVAR u32 ScreenPix[vMacScreenHeight][vMacScreenWidth];
LOCALVAR u32 MonoTable[256][8];
#if vMacScreenDepth > 0
LOCALVAR u32 ColorPal[CLUT_size];
LOCALVAR ui5r LastClutSum = 0xFFFFFFFF;
LOCALVAR blnr LastUseColorMode = falseblnr;
#endif

/* rows of the emulated screen that changed since the last frame */
LOCALVAR int DirtyTop = vMacScreenHeight;
LOCALVAR int DirtyBottom = 0;

/* display options */
enum {
	kScaleFill = 0,  /* stretch over the whole screen */
	kScaleAspect,    /* largest picture with square pixels */
	kScaleInteger,   /* largest whole multiple */
	kNumScales
};

LOCALVAR int ScaleMode = kScaleAspect;
LOCALVAR blnr Smoothing = trueblnr;

/* where the Mac screen is drawn, in virtual coordinates */
LOCALVAR f32 DispX = 0;
LOCALVAR f32 DispY = 0;
LOCALVAR f32 DispW = VirtW;
LOCALVAR f32 DispH = VirtH;

LOCALVAR const char *const ScaleNames[kNumScales] = {
	"Fill screen", "Keep aspect", "Integer scale"
};

/* SDL_PIXELFORMAT_RGBA8888: one native 32 bit word, red in the top byte */
#define RGBA(r, g, b, a) \
	((u32)(((r) << 24) | ((g) << 16) | ((b) << 8) | (a)))
#define RGBX(r, g, b) RGBA(r, g, b, 255)

LOCALPROC InitMonoTable(void)
{
	int i;
	int j;

	for (i = 0; i < 256; ++i) {
		for (j = 0; j < 8; ++j) {
			MonoTable[i][j] = (i & (0x80 >> j))
				? RGBX(0, 0, 0) : RGBX(255, 255, 255);
		}
	}
}

#if vMacScreenDepth > 0
LOCALFUNC ui5r ClutSum(void)
{
	ui5r Sum = 0;
	int i;

	for (i = 0; i < CLUT_size; ++i) {
		Sum = (Sum * 31) + CLUT_reds[i];
		Sum = (Sum * 31) + CLUT_greens[i];
		Sum = (Sum * 31) + CLUT_blues[i];
	}

	return Sum;
}

LOCALPROC UpdateColorPal(void)
{
	int i;

	for (i = 0; i < CLUT_size; ++i) {
		ColorPal[i] = RGBX(CLUT_reds[i] >> 8, CLUT_greens[i] >> 8,
			CLUT_blues[i] >> 8);
	}
}
#endif

LOCALPROC MarkWholeScreenDirty(void)
{
	DirtyTop = 0;
	DirtyBottom = vMacScreenHeight;
}

LOCALPROC HaveChangedScreenBuff(ui4r top, ui4r left,
	ui4r bottom, ui4r right)
{
	UnusedParam(left);
	UnusedParam(right);

	if ((int)top < DirtyTop) {
		DirtyTop = top;
	}
	if ((int)bottom > DirtyBottom) {
		DirtyBottom = bottom;
	}
}

LOCALPROC ConvertMonoRow(const ui3b *src, u32 *dst)
{
	int i;

	for (i = 0; i < vMacScreenMonoByteWidth; ++i) {
		memcpy(dst, MonoTable[src[i]], 8 * sizeof(u32));
		dst += 8;
	}
}

#if vMacScreenDepth > 0
LOCALPROC ConvertColorRow(const ui3b *src, u32 *dst)
{
	int i;

#if vMacScreenDepth == 3
	for (i = 0; i < vMacScreenWidth; ++i) {
		dst[i] = ColorPal[src[i]];
	}
#elif vMacScreenDepth == 2
	for (i = 0; i < vMacScreenByteWidth; ++i) {
		ui3r b = src[i];
		*dst++ = ColorPal[b >> 4];
		*dst++ = ColorPal[b & 0x0F];
	}
#elif vMacScreenDepth == 1
	for (i = 0; i < vMacScreenByteWidth; ++i) {
		ui3r b = src[i];
		*dst++ = ColorPal[(b >> 6) & 3];
		*dst++ = ColorPal[(b >> 4) & 3];
		*dst++ = ColorPal[(b >> 2) & 3];
		*dst++ = ColorPal[b & 3];
	}
#else
#error "unsupported screen depth"
#endif
}
#endif

/* Convert the changed rows of the emulated screen into the texture. */
LOCALPROC UpdateScreenTexture(void)
{
	const ui3b *buf;
	SDL_Rect r;
	int top;
	int bottom;
	int y;

#if vMacScreenDepth > 0
	if (UseColorMode != LastUseColorMode) {
		LastUseColorMode = UseColorMode;
		MarkWholeScreenDirty();
	}
	if (UseColorMode) {
		ui5r Sum = ClutSum();

		if ((Sum != LastClutSum) || ColorMappingChanged) {
			LastClutSum = Sum;
			ColorMappingChanged = falseblnr;
			UpdateColorPal();
			MarkWholeScreenDirty();
		}
	}
#endif

	if (DirtyBottom <= DirtyTop) {
		return;
	}

	top = DirtyTop;
	bottom = DirtyBottom;
	if (bottom > vMacScreenHeight) {
		bottom = vMacScreenHeight;
	}
	DirtyTop = vMacScreenHeight;
	DirtyBottom = 0;

	buf = GetCurDrawBuff();

	for (y = top; y < bottom; ++y) {
#if vMacScreenDepth > 0
		if (UseColorMode) {
			ConvertColorRow(buf + y * vMacScreenByteWidth, ScreenPix[y]);
		} else
#endif
		{
			ConvertMonoRow(buf + y * vMacScreenMonoByteWidth, ScreenPix[y]);
		}
	}

	r.x = 0;
	r.y = top;
	r.w = vMacScreenWidth;
	r.h = bottom - top;
	SDL_UpdateTexture(ScreenTex, &r, ScreenPix[top],
		vMacScreenWidth * sizeof(u32));
}

LOCALPROC UpdateDisplayRect(void)
{
	f32 s = (f32)VirtW / vMacScreenWidth;

	if ((f32)VirtH / vMacScreenHeight < s) {
		s = (f32)VirtH / vMacScreenHeight;
	}

	switch (ScaleMode) {
		case kScaleAspect:
			break;
		case kScaleInteger:
			s = (s >= 1.0f) ? floorf(s) : s;
			break;
		case kScaleFill:
		default:
			DispX = 0;
			DispY = 0;
			DispW = VirtW;
			DispH = VirtH;
			return;
	}

	DispW = vMacScreenWidth * s;
	DispH = vMacScreenHeight * s;
	DispX = (VirtW - DispW) / 2;
	DispY = (VirtH - DispH) / 2;
}

LOCALPROC UpdateScreenFilter(void)
{
	if (NULL != ScreenTex) {
		SDL_SetTextureScaleMode(ScreenTex,
			Smoothing ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	}
}

LOCALFUNC blnr BuildFontTexture(void)
{
	static u32 pix[FontTexH][FontTexW];
	int c;
	int gx;
	int gy;

	for (c = 0; c < 256; ++c) {
		int col = c & 15;
		int row = c >> 4;

		for (gy = 0; gy < 16; ++gy) {
			u8 bits = console_font_8x16[c * 16 + gy];

			for (gx = 0; gx < 8; ++gx) {
				pix[row * 16 + gy][col * 8 + gx] =
					(bits & (0x80 >> gx)) ? RGBA(255, 255, 255, 255) : 0;
			}
		}
	}

	FontTex = SDL_CreateTexture(MyRenderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STATIC, FontTexW, FontTexH);
	if (NULL == FontTex) {
		return falseblnr;
	}
	SDL_UpdateTexture(FontTex, NULL, pix, FontTexW * sizeof(u32));
	SDL_SetTextureBlendMode(FontTex, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(FontTex, SDL_ScaleModeNearest);

	return trueblnr;
}

LOCALFUNC blnr Screen_Init(void)
{
	/* One window: SDL shows it on the TV and on the GamePad. */
	MyWindow = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
	if (NULL == MyWindow) {
		return falseblnr;
	}
	MyRenderer = SDL_CreateRenderer(MyWindow, -1,
		SDL_RENDERER_ACCELERATED);
	if (NULL == MyRenderer) {
		return falseblnr;
	}
	SDL_RenderSetLogicalSize(MyRenderer, VirtW, VirtH);

	ScreenTex = SDL_CreateTexture(MyRenderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING, vMacScreenWidth, vMacScreenHeight);
	if (NULL == ScreenTex) {
		return falseblnr;
	}
	memset(ScreenPix, 0xFF, sizeof(ScreenPix));
	SDL_UpdateTexture(ScreenTex, NULL, ScreenPix,
		vMacScreenWidth * sizeof(u32));

	if (! BuildFontTexture()) {
		return falseblnr;
	}
	InitMonoTable();

	/* libwiiosk draws through the same renderer */
	OSK_SDL_SetRenderer(MyRenderer);

#if vMacScreenDepth > 0
	ColorModeWorks = trueblnr;
	ColorMappingChanged = trueblnr;
#endif

	UpdateDisplayRect();
	UpdateScreenFilter();
	MarkWholeScreenDirty();

	return trueblnr;
}

LOCALPROC Screen_UnInit(void)
{
	OSK_SDL_Shutdown();
	if (NULL != FontTex) {
		SDL_DestroyTexture(FontTex);
		FontTex = NULL;
	}
	if (NULL != ScreenTex) {
		SDL_DestroyTexture(ScreenTex);
		ScreenTex = NULL;
	}
	if (NULL != MyRenderer) {
		SDL_DestroyRenderer(MyRenderer);
		MyRenderer = NULL;
	}
	if (NULL != MyWindow) {
		SDL_DestroyWindow(MyWindow);
		MyWindow = NULL;
	}
}

/* --- immediate mode drawing helpers --- */

LOCALPROC SetDrawColor(u32 color)
{
	SDL_SetRenderDrawBlendMode(MyRenderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(MyRenderer, (color >> 24) & 0xFF,
		(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

LOCALPROC FillRect(f32 x, f32 y, f32 w, f32 h, u32 color)
{
	SDL_FRect r;

	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	SetDrawColor(color);
	SDL_RenderFillRectF(MyRenderer, &r);
}

LOCALPROC FrameRect(f32 x, f32 y, f32 w, f32 h, f32 t, u32 color)
{
	FillRect(x, y, w, t, color);
	FillRect(x, y + h - t, w, t, color);
	FillRect(x, y, t, h, color);
	FillRect(x + w - t, y, t, h, color);
}

LOCALPROC FillTriangle(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2,
	u32 color)
{
	SDL_Vertex v[3];
	int i;

	memset(v, 0, sizeof(v));
	v[0].position.x = x0;
	v[0].position.y = y0;
	v[1].position.x = x1;
	v[1].position.y = y1;
	v[2].position.x = x2;
	v[2].position.y = y2;
	for (i = 0; i < 3; ++i) {
		v[i].color.r = (color >> 24) & 0xFF;
		v[i].color.g = (color >> 16) & 0xFF;
		v[i].color.b = (color >> 8) & 0xFF;
		v[i].color.a = color & 0xFF;
	}
	SDL_RenderGeometry(MyRenderer, NULL, v, 3, NULL, 0);
}

LOCALFUNC f32 TextWidth(const char *s)
{
	return 8.0f * strlen(s);
}

LOCALPROC DrawText(f32 x, f32 y, const char *s, u32 color)
{
	SDL_SetTextureColorMod(FontTex, (color >> 24) & 0xFF,
		(color >> 16) & 0xFF, (color >> 8) & 0xFF);
	SDL_SetTextureAlphaMod(FontTex, color & 0xFF);

	for (; *s; ++s, x += 8) {
		u8 c = (u8)*s;

		if (c != ' ') {
			SDL_Rect src;
			SDL_FRect dst;

			src.x = (c & 15) * 8;
			src.y = (c >> 4) * 16;
			src.w = 8;
			src.h = 16;
			dst.x = x;
			dst.y = y;
			dst.w = 8;
			dst.h = 16;
			SDL_RenderCopyF(MyRenderer, FontTex, &src, &dst);
		}
	}
}

LOCALPROC DrawTextCentered(f32 cx, f32 y, const char *s, u32 color)
{
	DrawText(cx - TextWidth(s) / 2, y, s, color);
}

/* Arrow pointer for our own menus and the on-screen keyboard. */
LOCALPROC DrawPointer(f32 x, f32 y)
{
	FillTriangle(x - 2, y - 3, x - 2, y + 21, x + 15, y + 15,
		RGBA(0, 0, 0, 255));
	FillTriangle(x, y, x, y + 16, x + 11, y + 12, RGBA(255, 255, 255, 255));
}

/* ======================================================================
	User interface state
   ====================================================================== */

LOCALVAR blnr MenuOpen = falseblnr;
LOCALVAR blnr BrowserOpen = falseblnr;
LOCALVAR blnr OskVisible = falseblnr;
LOCALVAR blnr TypeOpen = falseblnr;   /* libwiiosk text keyboard */
LOCALVAR blnr PopupOpen = falseblnr;
LOCALVAR wiiuupdate_popup Popup;

LOCALVAR char ToastText[64];
LOCALVAR u32 ToastUntil = 0;   /* retrace count */
LOCALVAR blnr OverlayShown = falseblnr;

LOCALVAR blnr PointerActive = falseblnr;
LOCALVAR int PointerX = 0;
LOCALVAR int PointerY = 0;

LOCALVAR blnr ShutdownRequested = falseblnr;
LOCALVAR blnr ResetRequested = falseblnr;

LOCALVAR char AppDir[256] = "";

LOCALPROC ShowToast(const char *s)
{
	snprintf(ToastText, sizeof(ToastText), "%s", s);
	ToastUntil = RetraceCount + 120;
}

LOCALFUNC blnr UIPausesEmulation(void)
{
	return MenuOpen || BrowserOpen || PopupOpen || TypeOpen;
}

/* --- Mac key state, merged from several sources --- */

enum {
	kSrcPad = 1 << 0,
	kSrcUSB = 1 << 1,
	kSrcOsk = 1 << 2,
	kSrcType = 1 << 3
};

LOCALVAR u8 KeySources[256];

LOCALPROC MacKeySet(int mkc, int src, blnr down)
{
	u8 old;
	u8 nw;

	if ((mkc < 0) || (mkc >= 256) || (MKC_None == mkc)) {
		return;
	}

	old = KeySources[mkc];
	nw = down ? (old | src) : (old & ~src);
	KeySources[mkc] = nw;

	if ((0 != old) != (0 != nw)) {
		Keyboard_UpdateKeyMap2(mkc, 0 != nw);
	}
}

LOCALPROC MacKeysReleaseSource(int src)
{
	int i;

	for (i = 0; i < 256; ++i) {
		if (KeySources[i] & src) {
			MacKeySet(i, src, falseblnr);
		}
	}
}

/* --- USB keyboard (HID usage codes) to Mac key codes --- */

LOCALVAR u8 HidToMac[256];
LOCALVAR blnr HidWasDown[256];
LOCALVAR blnr MacCapsLocked = falseblnr;

LOCALPROC InitHidMap(void)
{
	static const u8 letters[26] = {
		MKC_A, MKC_B, MKC_C, MKC_D, MKC_E, MKC_F, MKC_G, MKC_H, MKC_I,
		MKC_J, MKC_K, MKC_L, MKC_M, MKC_N, MKC_O, MKC_P, MKC_Q, MKC_R,
		MKC_S, MKC_T, MKC_U, MKC_V, MKC_W, MKC_X, MKC_Y, MKC_Z
	};
	static const u8 digits[10] = {
		MKC_1, MKC_2, MKC_3, MKC_4, MKC_5, MKC_6, MKC_7, MKC_8, MKC_9,
		MKC_0
	};
	static const u8 keypad[10] = {
		MKC_KP1, MKC_KP2, MKC_KP3, MKC_KP4, MKC_KP5, MKC_KP6, MKC_KP7,
		MKC_KP8, MKC_KP9, MKC_KP0
	};
	int i;

	memset(HidToMac, MKC_None, sizeof(HidToMac));

	for (i = 0; i < 26; ++i) {
		HidToMac[0x04 + i] = letters[i];
	}
	for (i = 0; i < 10; ++i) {
		HidToMac[0x1E + i] = digits[i];
		HidToMac[0x59 + i] = keypad[i];
	}

	HidToMac[0x28] = MKC_Return;
	HidToMac[0x29] = MKC_formac_Escape;
	HidToMac[0x2A] = MKC_BackSpace;
	HidToMac[0x2B] = MKC_Tab;
	HidToMac[0x2C] = MKC_Space;
	HidToMac[0x2D] = MKC_Minus;
	HidToMac[0x2E] = MKC_Equal;
	HidToMac[0x2F] = MKC_LeftBracket;
	HidToMac[0x30] = MKC_RightBracket;
	HidToMac[0x31] = MKC_formac_BackSlash;
	HidToMac[0x32] = MKC_formac_BackSlash;
	HidToMac[0x33] = MKC_SemiColon;
	HidToMac[0x34] = MKC_SingleQuote;
	HidToMac[0x35] = MKC_formac_Grave;
	HidToMac[0x36] = MKC_Comma;
	HidToMac[0x37] = MKC_Period;
	HidToMac[0x38] = MKC_formac_Slash;
	HidToMac[0x39] = MKC_formac_CapsLock;

	HidToMac[0x3A] = MKC_formac_F1;
	HidToMac[0x3B] = MKC_formac_F2;
	HidToMac[0x3C] = MKC_formac_F3;
	HidToMac[0x3D] = MKC_formac_F4;
	HidToMac[0x3E] = MKC_formac_F5;
	HidToMac[0x3F] = MKC_F6;
	HidToMac[0x40] = MKC_F7;
	HidToMac[0x41] = MKC_F8;
	HidToMac[0x42] = MKC_F9;
	HidToMac[0x43] = MKC_F10;
	HidToMac[0x44] = MKC_F11;
	HidToMac[0x45] = MKC_F12;

	HidToMac[0x46] = MKC_Print;
	HidToMac[0x47] = MKC_ScrollLock;
	HidToMac[0x48] = MKC_Pause;
	HidToMac[0x49] = MKC_formac_Help;
	HidToMac[0x4A] = MKC_formac_Home;
	HidToMac[0x4B] = MKC_formac_PageUp;
	HidToMac[0x4C] = MKC_formac_ForwardDel;
	HidToMac[0x4D] = MKC_formac_End;
	HidToMac[0x4E] = MKC_formac_PageDown;
	HidToMac[0x4F] = MKC_Right;
	HidToMac[0x50] = MKC_Left;
	HidToMac[0x51] = MKC_Down;
	HidToMac[0x52] = MKC_Up;

	HidToMac[0x53] = MKC_Clear;
	HidToMac[0x54] = MKC_KPDevide;
	HidToMac[0x55] = MKC_KPMultiply;
	HidToMac[0x56] = MKC_KPSubtract;
	HidToMac[0x57] = MKC_KPAdd;
	HidToMac[0x58] = MKC_formac_Enter;
	HidToMac[0x63] = MKC_Decimal;
	HidToMac[0x64] = MKC_AngleBracket;
	HidToMac[0x67] = MKC_KPEqual;

	/* PC layout: Alt sits where Command is on a Mac keyboard */
	HidToMac[0xE0] = MKC_formac_Control;
	HidToMac[0xE1] = MKC_formac_Shift;
	HidToMac[0xE2] = MKC_formac_Command;
	HidToMac[0xE3] = MKC_formac_Option;
	HidToMac[0xE4] = MKC_formac_RControl;
	HidToMac[0xE5] = MKC_formac_RShift;
	HidToMac[0xE6] = MKC_formac_RCommand;
	HidToMac[0xE7] = MKC_formac_ROption;
}

LOCALPROC UpdateUSBKeyboard(blnr toMac)
{
	int i;

	for (i = 4; i < 256; ++i) {
		blnr down = INPUT_KeyHeld(i);
		int mkc = HidToMac[i];

		if (down == HidWasDown[i]) {
			continue;
		}
		HidWasDown[i] = down;

		if (MKC_None == mkc) {
			continue;
		}

		if (MKC_CapsLock == mkc) {
			/* the Mac's caps lock key latches down */
			if (down && toMac) {
				MacCapsLocked = ! MacCapsLocked;
				MacKeySet(MKC_CapsLock, kSrcUSB, MacCapsLocked);
				INPUT_KeyboardSetLed(INPUT_LED_CAPS, MacCapsLocked);
			}
			continue;
		}

		if (down) {
			if (toMac) {
				MacKeySet(mkc, kSrcUSB, trueblnr);
			}
		} else {
			MacKeySet(mkc, kSrcUSB, falseblnr);
		}
	}
}

/* ======================================================================
	On-screen keyboard
   ====================================================================== */

typedef struct {
	f32 x;          /* in key units */
	f32 w;
	u8 row;
	const char *lo;
	const char *hi;
	u8 mkc;
} OskKey;

#define OskUnit 40.0f
#define OskRowH 34.0f
#define OskX0 ((VirtW - 15 * OskUnit) / 2)
#define OskY0 (VirtH - 5 * OskRowH - 14.0f)

LOCALVAR const OskKey OskKeys[] = {
	{ 0, 1, 0, "`", "~", MKC_Grave },
	{ 1, 1, 0, "1", "!", MKC_1 },
	{ 2, 1, 0, "2", "@", MKC_2 },
	{ 3, 1, 0, "3", "#", MKC_3 },
	{ 4, 1, 0, "4", "$", MKC_4 },
	{ 5, 1, 0, "5", "%", MKC_5 },
	{ 6, 1, 0, "6", "^", MKC_6 },
	{ 7, 1, 0, "7", "&", MKC_7 },
	{ 8, 1, 0, "8", "*", MKC_8 },
	{ 9, 1, 0, "9", "(", MKC_9 },
	{ 10, 1, 0, "0", ")", MKC_0 },
	{ 11, 1, 0, "-", "_", MKC_Minus },
	{ 12, 1, 0, "=", "+", MKC_Equal },
	{ 13, 2, 0, "delete", "delete", MKC_BackSpace },

	{ 0, 1.5f, 1, "tab", "tab", MKC_Tab },
	{ 1.5f, 1, 1, "q", "Q", MKC_Q },
	{ 2.5f, 1, 1, "w", "W", MKC_W },
	{ 3.5f, 1, 1, "e", "E", MKC_E },
	{ 4.5f, 1, 1, "r", "R", MKC_R },
	{ 5.5f, 1, 1, "t", "T", MKC_T },
	{ 6.5f, 1, 1, "y", "Y", MKC_Y },
	{ 7.5f, 1, 1, "u", "U", MKC_U },
	{ 8.5f, 1, 1, "i", "I", MKC_I },
	{ 9.5f, 1, 1, "o", "O", MKC_O },
	{ 10.5f, 1, 1, "p", "P", MKC_P },
	{ 11.5f, 1, 1, "[", "{", MKC_LeftBracket },
	{ 12.5f, 1, 1, "]", "}", MKC_RightBracket },
	{ 13.5f, 1.5f, 1, "\\", "|", MKC_BackSlash },

	{ 0, 1.75f, 2, "caps", "caps", MKC_CapsLock },
	{ 1.75f, 1, 2, "a", "A", MKC_A },
	{ 2.75f, 1, 2, "s", "S", MKC_S },
	{ 3.75f, 1, 2, "d", "D", MKC_D },
	{ 4.75f, 1, 2, "f", "F", MKC_F },
	{ 5.75f, 1, 2, "g", "G", MKC_G },
	{ 6.75f, 1, 2, "h", "H", MKC_H },
	{ 7.75f, 1, 2, "j", "J", MKC_J },
	{ 8.75f, 1, 2, "k", "K", MKC_K },
	{ 9.75f, 1, 2, "l", "L", MKC_L },
	{ 10.75f, 1, 2, ";", ":", MKC_SemiColon },
	{ 11.75f, 1, 2, "'", "\"", MKC_SingleQuote },
	{ 12.75f, 2.25f, 2, "return", "return", MKC_Return },

	{ 0, 2.25f, 3, "shift", "shift", MKC_Shift },
	{ 2.25f, 1, 3, "z", "Z", MKC_Z },
	{ 3.25f, 1, 3, "x", "X", MKC_X },
	{ 4.25f, 1, 3, "c", "C", MKC_C },
	{ 5.25f, 1, 3, "v", "V", MKC_V },
	{ 6.25f, 1, 3, "b", "B", MKC_B },
	{ 7.25f, 1, 3, "n", "N", MKC_N },
	{ 8.25f, 1, 3, "m", "M", MKC_M },
	{ 9.25f, 1, 3, ",", "<", MKC_Comma },
	{ 10.25f, 1, 3, ".", ">", MKC_Period },
	{ 11.25f, 1, 3, "/", "?", MKC_Slash },
	{ 12.25f, 2.75f, 3, "shift", "shift", MKC_Shift },

	{ 0, 1.5f, 4, "ctrl", "ctrl", MKC_Control },
	{ 1.5f, 1.5f, 4, "option", "option", MKC_Option },
	{ 3, 1.5f, 4, "cmd", "cmd", MKC_Command },
	{ 4.5f, 5.5f, 4, "", "", MKC_Space },
	{ 10, 1, 4, "esc", "esc", MKC_Escape },
	{ 11, 1, 4, "\x1b", "\x1b", MKC_Left },
	{ 12, 1, 4, "\x19", "\x19", MKC_Down },
	{ 13, 1, 4, "\x18", "\x18", MKC_Up },
	{ 14, 1, 4, "\x1a", "\x1a", MKC_Right },
};

#define OskNumKeys ((int)(sizeof(OskKeys) / sizeof(OskKeys[0])))

LOCALVAR int OskPressed = -1;        /* key held by the pointer */
LOCALVAR blnr OskShift = falseblnr;  /* one-shot modifiers */
LOCALVAR blnr OskOption = falseblnr;
LOCALVAR blnr OskCommand = falseblnr;
LOCALVAR blnr OskControl = falseblnr;
LOCALVAR blnr OskCaps = falseblnr;   /* locks */

LOCALPROC OskKeyRect(int i, f32 *x, f32 *y, f32 *w, f32 *h)
{
	*x = OskX0 + OskKeys[i].x * OskUnit + 2;
	*y = OskY0 + OskKeys[i].row * OskRowH + 2;
	*w = OskKeys[i].w * OskUnit - 4;
	*h = OskRowH - 4;
}

LOCALFUNC blnr PointInOsk(int x, int y)
{
	return OskVisible && (x >= OskX0 - 10) && (x < OskX0 + 15 * OskUnit + 10)
		&& (y >= OskY0 - 10) && (y < OskY0 + 5 * OskRowH + 10);
}

LOCALFUNC int OskKeyAt(int px, int py)
{
	int i;

	for (i = 0; i < OskNumKeys; ++i) {
		f32 x, y, w, h;

		OskKeyRect(i, &x, &y, &w, &h);
		if ((px >= x - 2) && (px < x + w + 2)
			&& (py >= y - 2) && (py < y + h + 2))
		{
			return i;
		}
	}

	return -1;
}

LOCALFUNC blnr *OskModifierFlag(int mkc)
{
	switch (mkc) {
		case MKC_Shift: return &OskShift;
		case MKC_Option: return &OskOption;
		case MKC_Command: return &OskCommand;
		case MKC_Control: return &OskControl;
		case MKC_CapsLock: return &OskCaps;
		default: return NULL;
	}
}

LOCALPROC OskReleaseOneShots(void)
{
	if (OskShift) {
		OskShift = falseblnr;
		MacKeySet(MKC_Shift, kSrcOsk, falseblnr);
	}
	if (OskOption) {
		OskOption = falseblnr;
		MacKeySet(MKC_Option, kSrcOsk, falseblnr);
	}
	if (OskCommand) {
		OskCommand = falseblnr;
		MacKeySet(MKC_Command, kSrcOsk, falseblnr);
	}
	if (OskControl) {
		OskControl = falseblnr;
		MacKeySet(MKC_Control, kSrcOsk, falseblnr);
	}
}

LOCALPROC OskPress(int i)
{
	int mkc = OskKeys[i].mkc;
	blnr *flag = OskModifierFlag(mkc);

	if (NULL != flag) {
		*flag = ! *flag;
		MacKeySet(mkc, kSrcOsk, *flag);
	} else {
		OskPressed = i;
		MacKeySet(mkc, kSrcOsk, trueblnr);
	}
}

LOCALPROC OskRelease(void)
{
	if (OskPressed >= 0) {
		MacKeySet(OskKeys[OskPressed].mkc, kSrcOsk, falseblnr);
		OskPressed = -1;
		OskReleaseOneShots();
	}
}

LOCALPROC OskHide(void)
{
	OskRelease();
	OskReleaseOneShots();
	OskVisible = falseblnr;
}

LOCALPROC DrawOsk(void)
{
	int i;
	int hover = PointerActive ? OskKeyAt(PointerX, PointerY) : -1;
	blnr shifted = OskShift || (KeySources[MKC_Shift] != 0);
	blnr upper = shifted || OskCaps || MacCapsLocked;

	FillRect(OskX0 - 10, OskY0 - 10, 15 * OskUnit + 20, 5 * OskRowH + 20,
		RGBA(24, 24, 32, 215));

	for (i = 0; i < OskNumKeys; ++i) {
		f32 x, y, w, h;
		const OskKey *k = &OskKeys[i];
		blnr *flag = OskModifierFlag(k->mkc);
		blnr lit = (i == OskPressed) || ((NULL != flag) && *flag);
		u32 bg = lit ? RGBA(90, 140, 230, 240)
			: (i == hover) ? RGBA(120, 120, 135, 240)
			: RGBA(70, 70, 80, 240);
		const char *label;

		OskKeyRect(i, &x, &y, &w, &h);
		FillRect(x, y, w, h, bg);

		if (1 == strlen(k->lo) && (k->lo[0] >= 'a') && (k->lo[0] <= 'z')) {
			label = upper ? k->hi : k->lo;
		} else {
			label = shifted ? k->hi : k->lo;
		}
		DrawTextCentered(x + w / 2, y + (h - 16) / 2, label,
			RGBA(255, 255, 255, 255));
	}
}

/* ======================================================================
	Typing text (libwiiosk)

	The + button opens the Wii-style software keyboard from libwiiosk.
	When it is confirmed, the text is typed into the Mac one key stroke at
	a time, as a US keyboard would: shift for capitals and symbols, option
	for the Mac Roman extras and option dead keys for accented letters.
   ====================================================================== */

/* modifier bits of a stroke */
#define TS_Shift 1
#define TS_Option 2

typedef struct {
	u8 mkc;
	u8 mods;
} TypeStroke;

#define TypeQueueSz 4096
LOCALVAR TypeStroke TypeQueue[TypeQueueSz];
LOCALVAR int TypeQueueIn = 0;
LOCALVAR int TypeQueueOut = 0;
LOCALVAR blnr TypeKeyDown = falseblnr;   /* a stroke is pressed */
LOCALVAR TypeStroke TypeCur;

LOCALVAR osk_keyboard *TypeOsk = NULL;

LOCALFUNC blnr TypePush(u8 mkc, u8 mods)
{
	int next = (TypeQueueIn + 1) % TypeQueueSz;

	if (next == TypeQueueOut) {
		return falseblnr;
	}
	TypeQueue[TypeQueueIn].mkc = mkc;
	TypeQueue[TypeQueueIn].mods = mods;
	TypeQueueIn = next;

	return trueblnr;
}

/* US layout: printable ASCII to a key and whether shift is needed */
LOCALFUNC blnr AsciiStroke(u32 c, u8 *mkc, u8 *mods)
{
	static const char *const unshifted = "`1234567890-=[]\\;',./";
	static const char *const shifted = "~!@#$%^&*()_+{}|:\"<>?";
	static const u8 keys[] = {
		MKC_Grave, MKC_1, MKC_2, MKC_3, MKC_4, MKC_5, MKC_6, MKC_7,
		MKC_8, MKC_9, MKC_0, MKC_Minus, MKC_Equal, MKC_LeftBracket,
		MKC_RightBracket, MKC_BackSlash, MKC_SemiColon, MKC_SingleQuote,
		MKC_Comma, MKC_Period, MKC_Slash
	};
	static const u8 letters[26] = {
		MKC_A, MKC_B, MKC_C, MKC_D, MKC_E, MKC_F, MKC_G, MKC_H, MKC_I,
		MKC_J, MKC_K, MKC_L, MKC_M, MKC_N, MKC_O, MKC_P, MKC_Q, MKC_R,
		MKC_S, MKC_T, MKC_U, MKC_V, MKC_W, MKC_X, MKC_Y, MKC_Z
	};
	const char *p;

	*mods = 0;
	if ((c >= 'a') && (c <= 'z')) {
		*mkc = letters[c - 'a'];
		return trueblnr;
	}
	if ((c >= 'A') && (c <= 'Z')) {
		*mkc = letters[c - 'A'];
		*mods = TS_Shift;
		return trueblnr;
	}
	switch (c) {
		case ' ': *mkc = MKC_Space; return trueblnr;
		case '\n': *mkc = MKC_Return; return trueblnr;
		case '\t': *mkc = MKC_Tab; return trueblnr;
		default: break;
	}
	if ((c > ' ') && (c < 0x7F)) {
		p = strchr(unshifted, (int)c);
		if (NULL != p) {
			*mkc = keys[p - unshifted];
			return trueblnr;
		}
		p = strchr(shifted, (int)c);
		if (NULL != p) {
			*mkc = keys[p - shifted];
			*mods = TS_Shift;
			return trueblnr;
		}
	}

	return falseblnr;
}

/* Characters outside ASCII that a US Mac keyboard types: either one
   option stroke ('dead' == 0) or an option dead key followed by the base
   letter. */
typedef struct {
	u16 cp;
	char key;     /* ASCII key pressed with option (shifted form = shift) */
	char base;    /* letter after the dead key, or 0 */
} TypeExtra;

LOCALVAR const TypeExtra TypeExtras[] = {
	/* option dead keys: acute e, grave `, circumflex i, umlaut u, tilde n */
	{ 0xE1, 'e', 'a' }, { 0xE9, 'e', 'e' }, { 0xED, 'e', 'i' },
	{ 0xF3, 'e', 'o' }, { 0xFA, 'e', 'u' }, { 0xC1, 'e', 'A' },
	{ 0xC9, 'e', 'E' }, { 0xCD, 'e', 'I' }, { 0xD3, 'e', 'O' },
	{ 0xDA, 'e', 'U' },
	{ 0xE0, '`', 'a' }, { 0xE8, '`', 'e' }, { 0xEC, '`', 'i' },
	{ 0xF2, '`', 'o' }, { 0xF9, '`', 'u' }, { 0xC0, '`', 'A' },
	{ 0xC8, '`', 'E' }, { 0xCC, '`', 'I' }, { 0xD2, '`', 'O' },
	{ 0xD9, '`', 'U' },
	{ 0xE2, 'i', 'a' }, { 0xEA, 'i', 'e' }, { 0xEE, 'i', 'i' },
	{ 0xF4, 'i', 'o' }, { 0xFB, 'i', 'u' }, { 0xC2, 'i', 'A' },
	{ 0xCA, 'i', 'E' }, { 0xCE, 'i', 'I' }, { 0xD4, 'i', 'O' },
	{ 0xDB, 'i', 'U' },
	{ 0xE4, 'u', 'a' }, { 0xEB, 'u', 'e' }, { 0xEF, 'u', 'i' },
	{ 0xF6, 'u', 'o' }, { 0xFC, 'u', 'u' }, { 0xFF, 'u', 'y' },
	{ 0xC4, 'u', 'A' }, { 0xCB, 'u', 'E' }, { 0xCF, 'u', 'I' },
	{ 0xD6, 'u', 'O' }, { 0xDC, 'u', 'U' }, { 0x178, 'u', 'Y' },
	{ 0xE3, 'n', 'a' }, { 0xF1, 'n', 'n' }, { 0xF5, 'n', 'o' },
	{ 0xC3, 'n', 'A' }, { 0xD1, 'n', 'N' }, { 0xD5, 'n', 'O' },

	/* single option strokes */
	{ 0xE7, 'c', 0 }, { 0xC7, 'C', 0 }, { 0xDF, 's', 0 },
	{ 0xE5, 'a', 0 }, { 0xC5, 'A', 0 }, { 0xE6, '\'', 0 },
	{ 0xC6, '"', 0 }, { 0xF8, 'o', 0 }, { 0xD8, 'O', 0 },
	{ 0x153, 'q', 0 }, { 0x152, 'Q', 0 }, { 0xBF, '?', 0 },
	{ 0xA1, '1', 0 }, { 0xA3, '3', 0 }, { 0xA5, 'y', 0 },
	{ 0xA9, 'g', 0 }, { 0xAE, 'r', 0 }, { 0x2122, '2', 0 },
	{ 0xB0, '*', 0 }, { 0x2022, '8', 0 }, { 0xA2, '4', 0 },
	{ 0xA7, '6', 0 }, { 0xB6, '7', 0 }, { 0xAB, '\\', 0 },
	{ 0xBB, '|', 0 }, { 0x201C, '[', 0 }, { 0x201D, '{', 0 },
	{ 0x2018, ']', 0 }, { 0x2019, '}', 0 }, { 0x2026, ';', 0 },
	{ 0x2013, '-', 0 }, { 0x2014, '_', 0 }, { 0xB1, '+', 0 },
	{ 0xF7, '/', 0 }, { 0xB5, 'm', 0 }, { 0xAC, 'l', 0 },
	{ 0x3C0, 'p', 0 }, { 0x221E, '5', 0 }, { 0x2260, '=', 0 },
	{ 0x2264, ',', 0 }, { 0x2265, '.', 0 }, { 0x20AC, '@', 0 },
	{ 0xA0, ' ', 0 }
};

#define TypeNumExtras ((int)(sizeof(TypeExtras) / sizeof(TypeExtras[0])))

/* Queue the strokes for one code point; unknown characters are skipped. */
LOCALPROC TypeCodePoint(u32 cp)
{
	u8 mkc;
	u8 mods;
	int i;

	if ('\r' == cp) {
		return;
	}
	if (AsciiStroke(cp, &mkc, &mods)) {
		(void) TypePush(mkc, mods);
		return;
	}
	for (i = 0; i < TypeNumExtras; ++i) {
		if (TypeExtras[i].cp == cp) {
			if (AsciiStroke((u8)TypeExtras[i].key, &mkc, &mods)) {
				(void) TypePush(mkc, mods | TS_Option);
			}
			if ((0 != TypeExtras[i].base)
				&& AsciiStroke((u8)TypeExtras[i].base, &mkc, &mods))
			{
				(void) TypePush(mkc, mods);
			}
			return;
		}
	}
}

LOCALPROC TypeUTF8(const char *s)
{
	const u8 *p = (const u8 *)s;

	while (0 != *p) {
		u32 cp = *p++;
		int more = 0;

		if (cp >= 0xF0) {
			cp &= 0x07;
			more = 3;
		} else if (cp >= 0xE0) {
			cp &= 0x0F;
			more = 2;
		} else if (cp >= 0xC0) {
			cp &= 0x1F;
			more = 1;
		} else if (cp >= 0x80) {
			continue; /* stray continuation byte */
		}
		for (; (more > 0) && ((*p & 0xC0) == 0x80); --more) {
			cp = (cp << 6) | (*p++ & 0x3F);
		}
		if (0 == more) {
			TypeCodePoint(cp);
		}
	}
}

LOCALFUNC blnr TypePending(void)
{
	return TypeKeyDown || (TypeQueueIn != TypeQueueOut);
}

LOCALPROC TypeSetMods(u8 mods, blnr down)
{
	if (mods & TS_Shift) {
		MacKeySet(MKC_Shift, kSrcType, down);
	}
	if (mods & TS_Option) {
		MacKeySet(MKC_Option, kSrcType, down);
	}
}

/* One step per video field: press a stroke, release it on the next. Waits
   while Mini vMac's event queue is nearly full. */
LOCALPROC TypeStep(void)
{
	if (TypeKeyDown) {
		MacKeySet(TypeCur.mkc, kSrcType, falseblnr);
		TypeSetMods(TypeCur.mods, falseblnr);
		TypeKeyDown = falseblnr;
		return;
	}
	if (TypeQueueIn == TypeQueueOut) {
		return;
	}
	if ((ui4r)(MyEvtQIn - MyEvtQOut) > MyEvtQSz - 6) {
		return;
	}
	TypeCur = TypeQueue[TypeQueueOut];
	TypeQueueOut = (TypeQueueOut + 1) % TypeQueueSz;
	TypeSetMods(TypeCur.mods, trueblnr);
	MacKeySet(TypeCur.mkc, kSrcType, trueblnr);
	TypeKeyDown = trueblnr;
}

LOCALPROC TypeCancelPending(void)
{
	TypeQueueIn = TypeQueueOut = 0;
	TypeKeyDown = falseblnr;
	MacKeysReleaseSource(kSrcType);
}

LOCALPROC TypeOpenKeyboard(void)
{
	osk_config cfg;
	input_key_event ev;

	if (TypeOpen) {
		return;
	}

	OSK_DefaultConfig(&cfg);
	OSK_UseSystemLanguage(&cfg);
	cfg.title = "Type into the Mac";
	cfg.max_chars = 2048;
	cfg.screen_width = VirtW;
	cfg.screen_height = VirtH;
	/* the Wii U picture is 16:9 */
	cfg.flags |= OSK_FLAG_MULTILINE | OSK_FLAG_PREDICT_OFF
		| OSK_FLAG_WIDESCREEN;

	TypeOsk = OSK_Create(&cfg);
	if (NULL == TypeOsk) {
		ShowToast("Could not open the keyboard");
		return;
	}

	/* keys typed on a USB keyboard before now are not for the box */
	while (INPUT_KeyPop(&ev)) {
	}

	if (OskVisible) {
		OskHide();
	}
	TypeOpen = trueblnr;
}

LOCALPROC TypeCloseKeyboard(blnr typeIt)
{
	if (! TypeOpen) {
		return;
	}
	if (typeIt) {
		TypeUTF8(OSK_GetText(TypeOsk));
	}
	OSK_Destroy(TypeOsk);
	TypeOsk = NULL;
	TypeOpen = falseblnr;
}

/* Feed the keyboard one frame of input; true while it stays open. */
LOCALPROC TypeUpdateKeyboard(void)
{
	osk_input in;
	osk_status st;

	OSK_ReadInputWiiU(&in);
	st = OSK_Update(TypeOsk, &in);
	if (OSK_STATUS_DONE == st) {
		TypeCloseKeyboard(trueblnr);
	} else if (OSK_STATUS_CANCELLED == st) {
		TypeCloseKeyboard(falseblnr);
	}
}

/* ======================================================================
	File browser (insert disk image / ROM)
   ====================================================================== */

#define BrowserMaxEntries 512
#define BrowserNameLen 256
#define BrowserLines 18

typedef struct {
	char name[BrowserNameLen];
	blnr isDir;
} BrowserEntry;

LOCALVAR BrowserEntry *BrowserEntries = NULL;
LOCALVAR int BrowserCount = 0;
LOCALVAR int BrowserSel = 0;
LOCALVAR int BrowserTop = 0;
LOCALVAR blnr BrowserAtDevices = falseblnr;
LOCALVAR char BrowserPath[256];

LOCALFUNC int BrowserCompare(const void *a, const void *b)
{
	const BrowserEntry *A = (const BrowserEntry *)a;
	const BrowserEntry *B = (const BrowserEntry *)b;

	if (A->isDir != B->isDir) {
		return A->isDir ? -1 : 1;
	}

	return strcasecmp(A->name, B->name);
}

LOCALFUNC blnr IsDeviceRoot(const char *path)
{
	size_t n = strlen(SDRoot);

	return (0 == strncmp(path, SDRoot, n))
		&& ((0 == path[n]) || (('/' == path[n]) && (0 == path[n + 1])));
}

/* "fs:/vol/external01/x" is shown as "sd:/x" */
LOCALFUNC const char *ShownPath(const char *path, char *buf, size_t n)
{
	size_t k = strlen(SDRoot);

	if (0 == strncmp(path, SDRoot, k)) {
		snprintf(buf, n, "sd:%s", ('/' == path[k]) ? path + k : "/");
		return buf;
	}
	return path;
}

LOCALPROC BrowserListDevices(void)
{
	static const char *const devs[] = { SDRoot "/" };
	unsigned int i;

	BrowserAtDevices = trueblnr;
	BrowserCount = 0;
	snprintf(BrowserPath, sizeof(BrowserPath), "Devices");

	for (i = 0; i < sizeof(devs) / sizeof(devs[0]); ++i) {
		DIR *d = opendir(devs[i]);

		if (NULL != d) {
			closedir(d);
			snprintf(BrowserEntries[BrowserCount].name, BrowserNameLen,
				"%s", devs[i]);
			BrowserEntries[BrowserCount].isDir = trueblnr;
			++BrowserCount;
		}
	}

	BrowserSel = 0;
	BrowserTop = 0;
}

LOCALPROC BrowserPopulate(void)
{
	DIR *d;
	struct dirent *e;

	BrowserAtDevices = falseblnr;
	BrowserCount = 0;
	BrowserSel = 0;
	BrowserTop = 0;

	if (NULL == getcwd(BrowserPath, sizeof(BrowserPath))) {
		BrowserListDevices();
		return;
	}

	strcpy(BrowserEntries[0].name, "..");
	BrowserEntries[0].isDir = trueblnr;
	BrowserCount = 1;

	d = opendir(".");
	if (NULL != d) {
		while ((NULL != (e = readdir(d)))
			&& (BrowserCount < BrowserMaxEntries))
		{
			struct stat st;
			BrowserEntry *be = &BrowserEntries[BrowserCount];

			if ((0 == strcmp(e->d_name, ".")) || (0 == strcmp(e->d_name, ".."))
				|| ('.' == e->d_name[0]))
			{
				continue;
			}

			snprintf(be->name, BrowserNameLen, "%s", e->d_name);
			if (DT_UNKNOWN != e->d_type) {
				be->isDir = (DT_DIR == e->d_type);
			} else {
				be->isDir = (0 == stat(e->d_name, &st)) && S_ISDIR(st.st_mode);
			}
			++BrowserCount;
		}
		closedir(d);
	}

	if (BrowserCount > 1) {
		qsort(&BrowserEntries[1], BrowserCount - 1, sizeof(BrowserEntry),
			BrowserCompare);
	}
}

LOCALPROC BrowserOpenUI(void)
{
	if (NULL == BrowserEntries) {
		BrowserEntries = (BrowserEntry *)malloc(
			BrowserMaxEntries * sizeof(BrowserEntry));
		if (NULL == BrowserEntries) {
			return;
		}
	}

	BrowserPopulate();
	BrowserOpen = trueblnr;
	MenuOpen = falseblnr;
}

LOCALPROC BrowserMoveSel(int delta)
{
	BrowserSel += delta;
	if (BrowserSel < 0) {
		BrowserSel = 0;
	}
	if (BrowserSel >= BrowserCount) {
		BrowserSel = BrowserCount - 1;
	}
	if (BrowserSel < BrowserTop) {
		BrowserTop = BrowserSel;
	}
	if (BrowserSel >= BrowserTop + BrowserLines) {
		BrowserTop = BrowserSel - BrowserLines + 1;
	}
}

LOCALPROC BrowserUp(void)
{
	char cwd[256];

	if (BrowserAtDevices) {
		return;
	}

	if ((NULL == getcwd(cwd, sizeof(cwd))) || IsDeviceRoot(cwd)) {
		BrowserListDevices();
	} else {
		(void) chdir("..");
		BrowserPopulate();
	}
}

LOCALPROC BrowserActivate(void)
{
	BrowserEntry *e;

	if ((BrowserSel < 0) || (BrowserSel >= BrowserCount)) {
		return;
	}
	e = &BrowserEntries[BrowserSel];

	if (BrowserAtDevices) {
		if (0 == chdir(e->name)) {
			BrowserPopulate();
		}
	} else if (e->isDir) {
		if (0 == strcmp(e->name, "..")) {
			BrowserUp();
		} else if (0 == chdir(e->name)) {
			BrowserPopulate();
		}
	} else {
		char path[512];

		snprintf(path, sizeof(path), "%s%s%s", BrowserPath,
			('/' == BrowserPath[strlen(BrowserPath) - 1]) ? "" : "/",
			e->name);

		if (! ROM_loaded) {
			(void) LoadMacRomFrom(path);
		} else if (Sony_Insert1(path, falseblnr)) {
			char msg[64];

			snprintf(msg, sizeof(msg), "Inserted %.40s", e->name);
			ShowToast(msg);
		}
		BrowserOpen = falseblnr;
	}
}

LOCALPROC DrawBrowser(void)
{
	const f32 x = 40;
	const f32 y = 30;
	const f32 w = VirtW - 80;
	const f32 lineH = 20;
	const f32 h = 74 + BrowserLines * lineH;
	char line[100];
	int i;

	FillRect(x, y, w, h, RGBA(235, 235, 240, 245));
	FrameRect(x, y, w, h, 2, RGBA(0, 0, 0, 255));
	FillRect(x, y, w, 24, RGBA(0, 0, 0, 255));
	DrawTextCentered(VirtW / 2, y + 4,
		ROM_loaded ? "Insert disk image" : "Choose a Macintosh ROM",
		RGBA(255, 255, 255, 255));

	{
		char shown[256];

		snprintf(line, sizeof(line), "%.96s",
			ShownPath(BrowserPath, shown, sizeof(shown)));
	}
	DrawText(x + 10, y + 28, line, RGBA(60, 60, 60, 255));

	for (i = 0; (i < BrowserLines) && (BrowserTop + i < BrowserCount); ++i) {
		BrowserEntry *e = &BrowserEntries[BrowserTop + i];
		f32 ly = y + 48 + i * lineH;
		blnr sel = (BrowserTop + i == BrowserSel);

		if (sel) {
			FillRect(x + 6, ly - 2, w - 12, lineH, RGBA(0, 0, 0, 255));
		}
		if (BrowserAtDevices) {
			snprintf(line, sizeof(line), "[SD card]");
		} else {
			snprintf(line, sizeof(line), e->isDir ? "[%.92s]" : "%.94s",
				e->name);
		}
		DrawText(x + 12, ly, line,
			sel ? RGBA(255, 255, 255, 255) : RGBA(0, 0, 0, 255));
	}

	DrawTextCentered(VirtW / 2, y + h - 22,
		"A: open   B: up   Y: close", RGBA(60, 60, 60, 255));
}

LOCALFUNC int BrowserLineAt(int px, int py)
{
	const f32 y = 30 + 48;
	int i;

	if ((px < 46) || (px >= VirtW - 46) || (py < y - 2)) {
		return -1;
	}
	i = (int)((py - (y - 2)) / 20);
	if ((i >= BrowserLines) || (BrowserTop + i >= BrowserCount)) {
		return -1;
	}

	return BrowserTop + i;
}

/* ======================================================================
	Menu (HOME)
   ====================================================================== */

enum {
	kMenuInsert = 0,
	kMenuDisplay,
	kMenuSmoothing,
	kMenuSpeed,
	kMenuType,
	kMenuKeyboard,
	kMenuInterrupt,
	kMenuReset,
	kMenuQuit,
	kMenuResume,
	kNumMenuItems
};

LOCALVAR int MenuSel = 0;
LOCALVAR blnr SettingsChanged = falseblnr;

#define MenuW 440.0f
#define MenuItemH 26.0f
#define MenuX ((VirtW - MenuW) / 2)
#define MenuY 60.0f
#define MenuItemsY (MenuY + 40)

LOCALVAR const ui3b SpeedValues[] = { 0, 1, 2, 3, 4, 5, (ui3b)-1 };
#define NumSpeeds ((int)(sizeof(SpeedValues) / sizeof(SpeedValues[0])))

LOCALFUNC const char *SpeedName(void)
{
	switch (SpeedValue) {
		case 0: return "1x";
		case 1: return "2x";
		case 2: return "4x";
		case 3: return "8x";
		case 4: return "16x";
		case 5: return "32x";
		default: return "All out";
	}
}

LOCALPROC MenuItemText(int i, char *buf, size_t n)
{
	switch (i) {
		case kMenuInsert:
			snprintf(buf, n, ROM_loaded ? "Insert disk image..."
				: "Load ROM...");
			break;
		case kMenuDisplay:
			snprintf(buf, n, "Display:  < %s >", ScaleNames[ScaleMode]);
			break;
		case kMenuSmoothing:
			snprintf(buf, n, "Smoothing:  < %s >", Smoothing ? "On" : "Off");
			break;
		case kMenuSpeed:
			snprintf(buf, n, "Speed:  < %s >", SpeedName());
			break;
		case kMenuType:
			snprintf(buf, n, "Type text...");
			break;
		case kMenuKeyboard:
			snprintf(buf, n, "%s Mac keyboard (special keys)",
				OskVisible ? "Hide" : "Show");
			break;
		case kMenuInterrupt:
			snprintf(buf, n, "Interrupt (programmer's switch)");
			break;
		case kMenuReset:
			snprintf(buf, n, "Reset the Mac");
			break;
		case kMenuQuit:
			snprintf(buf, n, "Quit to loader");
			break;
		case kMenuResume:
		default:
			snprintf(buf, n, "Resume");
			break;
	}
}

LOCALPROC SetScaleMode(int m)
{
	ScaleMode = (m + kNumScales) % kNumScales;
	UpdateDisplayRect();
	UpdateScreenFilter();
	SettingsChanged = trueblnr;
}

LOCALPROC CycleSpeed(int dir)
{
	int i;
	int cur = NumSpeeds - 1;

	for (i = 0; i < NumSpeeds; ++i) {
		if (SpeedValues[i] == SpeedValue) {
			cur = i;
		}
	}
	cur = (cur + dir + NumSpeeds) % NumSpeeds;
	SpeedValue = SpeedValues[cur];
	SettingsChanged = trueblnr;
}

LOCALPROC MenuClose(void)
{
	MenuOpen = falseblnr;
	if (SettingsChanged) {
		SaveSettings();
		SettingsChanged = falseblnr;
	}
}

LOCALPROC MenuChange(int dir)
{
	switch (MenuSel) {
		case kMenuDisplay:
			SetScaleMode(ScaleMode + dir);
			break;
		case kMenuSmoothing:
			Smoothing = ! Smoothing;
			UpdateScreenFilter();
			SettingsChanged = trueblnr;
			break;
		case kMenuSpeed:
			CycleSpeed(dir);
			break;
		default:
			break;
	}
}

LOCALPROC MenuActivate(void)
{
	switch (MenuSel) {
		case kMenuInsert:
			MenuClose();
			BrowserOpenUI();
			break;
		case kMenuDisplay:
		case kMenuSmoothing:
		case kMenuSpeed:
			MenuChange(1);
			break;
		case kMenuType:
			MenuClose();
			TypeOpenKeyboard();
			break;
		case kMenuKeyboard:
			if (OskVisible) {
				OskHide();
			} else {
				OskVisible = trueblnr;
			}
			MenuClose();
			break;
		case kMenuInterrupt:
			WantMacInterrupt = trueblnr;
			MenuClose();
			break;
		case kMenuReset:
			WantMacReset = trueblnr;
			MenuClose();
			break;
		case kMenuQuit:
			MenuClose();
			ForceMacOff = trueblnr;
			break;
		case kMenuResume:
		default:
			MenuClose();
			break;
	}
}

LOCALFUNC int MenuItemAt(int px, int py)
{
	int i;

	if ((px < MenuX) || (px >= MenuX + MenuW) || (py < MenuItemsY)) {
		return -1;
	}
	i = (int)((py - MenuItemsY) / MenuItemH);

	return (i < kNumMenuItems) ? i : -1;
}

LOCALPROC DrawMenu(void)
{
	const f32 h = 40 + kNumMenuItems * MenuItemH + 112;
	char buf[80];
	int i;

	FillRect(MenuX, MenuY, MenuW, h, RGBA(235, 235, 240, 245));
	FrameRect(MenuX, MenuY, MenuW, h, 2, RGBA(0, 0, 0, 255));
	FillRect(MenuX, MenuY, MenuW, 26, RGBA(0, 0, 0, 255));
	DrawTextCentered(VirtW / 2, MenuY + 5, APP_TITLE " " APP_VERSION,
		RGBA(255, 255, 255, 255));

	for (i = 0; i < kNumMenuItems; ++i) {
		f32 y = MenuItemsY + i * MenuItemH;
		blnr sel = (i == MenuSel);

		if (sel) {
			FillRect(MenuX + 8, y, MenuW - 16, MenuItemH - 2,
				RGBA(0, 0, 0, 255));
		}
		MenuItemText(i, buf, sizeof(buf));
		DrawText(MenuX + 24, y + 4, buf,
			sel ? RGBA(255, 255, 255, 255) : RGBA(0, 0, 0, 255));
	}

	{
		f32 y = MenuItemsY + kNumMenuItems * MenuItemH + 10;
		u32 c = RGBA(70, 70, 70, 255);

		DrawText(MenuX + 16, y, "Touch, or point + A/B: mouse", c);
		DrawText(MenuX + 16, y + 18, "D-Pad: arrows  Y: command  X: option", c);
		DrawText(MenuX + 16, y + 36, "+: type text  -: display  Stick: mouse", c);
		DrawText(MenuX + 16, y + 54, "USB keyboard and mouse work too.", c);
		DrawText(MenuX + 16, y + 72, "Alt = command, Windows key = option", c);
	}
}

/* ======================================================================
	Update popup (libwiiuupdate)
   ====================================================================== */

LOCALPROC DrawPopup(void)
{
	const f32 w = 520;
	const f32 h = 150;
	const f32 x = (VirtW - w) / 2;
	const f32 y = (VirtH - h) / 2;

	FillRect(x, y, w, h, RGBA(235, 235, 240, 250));
	FrameRect(x, y, w, h, 2, RGBA(0, 0, 0, 255));
	FillRect(x, y, w, 26, RGBA(0, 0, 0, 255));
	DrawTextCentered(VirtW / 2, y + 5, Popup.title ? Popup.title : "",
		RGBA(255, 255, 255, 255));
	DrawTextCentered(VirtW / 2, y + 40, Popup.text, RGBA(0, 0, 0, 255));
	if ((NULL != Popup.detail) && (0 != Popup.detail[0])) {
		DrawTextCentered(VirtW / 2, y + 62, Popup.detail,
			RGBA(60, 60, 60, 255));
	}
	if (Popup.progress >= 0) {
		f32 bw = w - 60;

		FrameRect(x + 30, y + 86, bw, 18, 2, RGBA(0, 0, 0, 255));
		FillRect(x + 32, y + 88, (bw - 4) * Popup.progress / 100.0f, 14,
			RGBA(40, 90, 200, 255));
	}
	if ((NULL != Popup.hint) && (0 != Popup.hint[0])) {
		DrawTextCentered(VirtW / 2, y + h - 26, Popup.hint,
			RGBA(0, 0, 0, 255));
	}
}

/* ======================================================================
	Frame rendering
   ====================================================================== */

LOCALVAR u32 LastRenderRetrace = 0;

LOCALFUNC blnr OverlayWanted(void)
{
	return MenuOpen || BrowserOpen || OskVisible || PopupOpen || TypeOpen
		|| ((s32)(ToastUntil - RetraceCount) > 0);
}

LOCALPROC RenderFrame(void)
{
	blnr overlay = OverlayWanted();
	SDL_FRect d;

	UpdateScreenTexture();

	SDL_SetRenderDrawColor(MyRenderer, 0, 0, 0, 255);
	SDL_RenderClear(MyRenderer);

	/* the Mac screen */
	d.x = DispX;
	d.y = DispY;
	d.w = DispW;
	d.h = DispH;
	SDL_RenderCopyF(MyRenderer, ScreenTex, NULL, &d);

	if (overlay) {
		if (OskVisible) {
			DrawOsk();
		}
		if (MenuOpen) {
			DrawMenu();
		}
		if (BrowserOpen) {
			DrawBrowser();
		}
		if (TypeOpen) {
			/* libwiiosk draws over the whole screen */
			OSK_Draw(TypeOsk);
		}
		if (PopupOpen) {
			DrawPopup();
		}
		if ((s32)(ToastUntil - RetraceCount) > 0) {
			f32 tw = TextWidth(ToastText) + 24;

			FillRect((VirtW - tw) / 2, 24, tw, 26, RGBA(0, 0, 0, 200));
			DrawTextCentered(VirtW / 2, 29, ToastText,
				RGBA(255, 255, 255, 255));
		}
		if (PointerActive && (MenuOpen || BrowserOpen
			|| PointInOsk(PointerX, PointerY)))
		{
			DrawPointer(PointerX, PointerY);
		}
	}
	OverlayShown = overlay;

	SDL_RenderPresent(MyRenderer);
}

/* Called often; draws at most once per video field. */
LOCALPROC MaybeRenderFrame(void)
{
	u32 r = RetraceCount;

	if (r == LastRenderRetrace) {
		return;
	}

	if ((DirtyBottom > DirtyTop) || OverlayWanted() || OverlayShown
#if vMacScreenDepth > 0
		|| (UseColorMode && ((ClutSum() != LastClutSum)
			|| ColorMappingChanged))
		|| (UseColorMode != LastUseColorMode)
#endif
		)
	{
		LastRenderRetrace = r;
		RenderFrame();
	}
}

/* ======================================================================
	Settings
   ====================================================================== */

#define SettingsFile "settings.txt"

LOCALPROC LoadSettings(void)
{
	FILE *f = fopen(SettingsFile, "r");
	char line[64];

	if (NULL == f) {
		return;
	}

	while (NULL != fgets(line, sizeof(line), f)) {
		int v;

		if (1 == sscanf(line, "display=%d", &v)) {
			ScaleMode = ((v >= 0) && (v < kNumScales)) ? v : kScaleFill;
		} else if (1 == sscanf(line, "smoothing=%d", &v)) {
			Smoothing = (0 != v);
		} else if (1 == sscanf(line, "speed=%d", &v)) {
			SpeedValue = (v < 0) ? (ui3b)-1 : (ui3b)v;
		}
	}
	fclose(f);
}

LOCALPROC SaveSettings(void)
{
	FILE *f;

	if (0 == AppDir[0]) {
		return;
	}
	if (0 != chdir(AppDir)) {
		return;
	}
	f = fopen(SettingsFile, "w");
	if (NULL != f) {
		fprintf(f, "display=%d\nsmoothing=%d\nspeed=%d\n", ScaleMode,
			Smoothing ? 1 : 0,
			(SpeedValue == (ui3b)-1) ? -1 : (int)SpeedValue);
		fclose(f);
	}
}

/* ======================================================================
	Mouse
   ====================================================================== */

LOCALVAR f32 MacMouseX = vMacScreenWidth / 2;
LOCALVAR f32 MacMouseY = vMacScreenHeight / 2;
LOCALVAR blnr MacMouseDown = falseblnr;

LOCALFUNC blnr MyMoveMouse(si4b h, si4b v)
{
	MacMouseX = h;
	MacMouseY = v;

	return trueblnr;
}

#if EnableFSMouseMotion
LOCALPROC MyMouseConstrain(void)
{
	si4b shiftdh;
	si4b shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = falseblnr;
		}
	}
}
#endif

LOCALPROC ClampMacMouse(void)
{
	if (MacMouseX < 0) {
		MacMouseX = 0;
	} else if (MacMouseX > vMacScreenWidth - 1) {
		MacMouseX = vMacScreenWidth - 1;
	}
	if (MacMouseY < 0) {
		MacMouseY = 0;
	} else if (MacMouseY > vMacScreenHeight - 1) {
		MacMouseY = vMacScreenHeight - 1;
	}
}

LOCALPROC CheckMouseState(void)
{
	MyMousePositionSet((int)MacMouseX, (int)MacMouseY);
	MyMouseButtonSet(MacMouseDown);
}

/* ======================================================================
	Input
   ====================================================================== */

#define PadMouseButtons (WPAD_BUTTON_A | WPAD_BUTTON_B | INPUT_BUTTON_Z)

LOCALVAR u32 RepeatButton = 0;
LOCALVAR u32 RepeatAt = 0;

/* D-Pad presses with auto repeat, for the menus */
LOCALFUNC u32 RepeatedDown(u32 down, u32 held)
{
	const u32 dirs = WPAD_BUTTON_UP | WPAD_BUTTON_DOWN
		| WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT;
	u32 r = RetraceCount;
	u32 out = down;

	if (down & dirs) {
		RepeatButton = down & dirs;
		RepeatAt = r + 24;
	} else if ((held & RepeatButton) && ((s32)(r - RepeatAt) >= 0)) {
		out |= RepeatButton;
		RepeatAt = r + 5;
	} else if (! (held & RepeatButton)) {
		RepeatButton = 0;
	}

	return out;
}

LOCALPROC ReleaseEmulatedInputs(void)
{
	MacKeysReleaseSource(kSrcPad);
	MacKeysReleaseSource(kSrcType);
	TypeKeyDown = falseblnr;
	OskRelease();
	MacMouseDown = falseblnr;
}

LOCALPROC PollInput(void)
{
	u32 down;
	u32 held;
	blnr wasPaused = UIPausesEmulation();

	INPUT_Update();

#ifdef TEST_SHOW_TYPE
	{
		/* emulator test runs: open the text keyboard from the start */
		static blnr opened = falseblnr;

		if (! opened) {
			opened = trueblnr;
			TypeOpenKeyboard();
		}
	}
#endif

	down = INPUT_PadsDown();
	held = INPUT_PadsHeld();

	PointerActive = INPUT_CursorActive();
	if (PointerActive) {
		INPUT_CursorPosition(&PointerX, &PointerY);
	}

	/* the update popup takes the controls while it is up */
	{
		u32 vpad = ((down & WPAD_BUTTON_A) ? VPAD_BUTTON_A : 0)
			| ((down & WPAD_BUTTON_B) ? VPAD_BUTTON_B : 0);
		int st = WIIUUPDATE_Popup(vpad, &Popup);

		if (WIIUUPDATE_POPUP_EXIT == st) {
			PopupOpen = falseblnr;
			ForceMacOff = trueblnr;
			return;
		}
		PopupOpen = (WIIUUPDATE_POPUP_OPEN == st);
		if (PopupOpen) {
			down = 0;
			held = 0;
		}
	}

	if (TypeOpen && ! PopupOpen) {
		TypeUpdateKeyboard();
		down = 0;
		held = 0;
	}

	if ((! PopupOpen) && (! TypeOpen)) {
		if (down & WPAD_BUTTON_HOME) {
			if (MenuOpen) {
				MenuClose();
			} else if (BrowserOpen) {
				BrowserOpen = falseblnr;
			} else {
				MenuOpen = trueblnr;
				MenuSel = 0;
			}
			down &= ~WPAD_BUTTON_HOME;
		}
	}

	if (MenuOpen) {
		u32 rd = RepeatedDown(down, held);
		int hover = PointerActive ? MenuItemAt(PointerX, PointerY) : -1;

		if (INPUT_CursorMoved() && (hover >= 0)) {
			MenuSel = hover;
		}
		if (rd & WPAD_BUTTON_UP) {
			MenuSel = (MenuSel + kNumMenuItems - 1) % kNumMenuItems;
		}
		if (rd & WPAD_BUTTON_DOWN) {
			MenuSel = (MenuSel + 1) % kNumMenuItems;
		}
		if (rd & WPAD_BUTTON_LEFT) {
			MenuChange(-1);
		}
		if (rd & WPAD_BUTTON_RIGHT) {
			MenuChange(1);
		}
		if ((down & WPAD_BUTTON_A)
			|| (INPUT_MouseButtonsPressed() & INPUT_MOUSE_LEFT))
		{
			MenuActivate();
		} else if (down & WPAD_BUTTON_B) {
			MenuClose();
		}
	} else if (BrowserOpen) {
		u32 rd = RepeatedDown(down, held);
		int hover = PointerActive ? BrowserLineAt(PointerX, PointerY) : -1;

		if (INPUT_CursorMoved() && (hover >= 0)) {
			BrowserSel = hover;
		}
		if (rd & WPAD_BUTTON_UP) {
			BrowserMoveSel(-1);
		}
		if (rd & WPAD_BUTTON_DOWN) {
			BrowserMoveSel(1);
		}
		if (rd & WPAD_BUTTON_LEFT) {
			BrowserMoveSel(-BrowserLines);
		}
		if (rd & WPAD_BUTTON_RIGHT) {
			BrowserMoveSel(BrowserLines);
		}
		{
			int dx, dy, wheel;

			INPUT_MouseDelta(&dx, &dy, &wheel);
			if (0 != wheel) {
				BrowserMoveSel(wheel > 0 ? 3 : -3);
			}
		}
		if ((down & WPAD_BUTTON_A)
			|| (INPUT_MouseButtonsPressed() & INPUT_MOUSE_LEFT))
		{
			BrowserActivate();
		} else if (down & WPAD_BUTTON_B) {
			BrowserUp();
		} else if (down & WPAD_BUTTON_1) {
			BrowserOpen = falseblnr;   /* Y */
		}
	}

	if (UIPausesEmulation()) {
		if (! wasPaused) {
			ReleaseEmulatedInputs();
		}
		UpdateUSBKeyboard(falseblnr);
		return;
	}

	/* --- normal emulation input --- */

	TypeStep();

	if (down & WPAD_BUTTON_PLUS) {
		if (OskVisible) {
			OskHide();
		} else if (TypePending()) {
			TypeCancelPending();
			ShowToast("Typing stopped");
		} else {
			TypeOpenKeyboard();
		}
	}
	if (down & WPAD_BUTTON_MINUS) {
		char msg[48];

		SetScaleMode(ScaleMode + 1);
		SaveSettings();
		SettingsChanged = falseblnr;
		snprintf(msg, sizeof(msg), "Display: %s", ScaleNames[ScaleMode]);
		ShowToast(msg);
	}

	MacKeySet(MKC_Up, kSrcPad, 0 != (held & WPAD_BUTTON_UP));
	MacKeySet(MKC_Down, kSrcPad, 0 != (held & WPAD_BUTTON_DOWN));
	MacKeySet(MKC_Left, kSrcPad, 0 != (held & WPAD_BUTTON_LEFT));
	MacKeySet(MKC_Right, kSrcPad, 0 != (held & WPAD_BUTTON_RIGHT));
	MacKeySet(MKC_Command, kSrcPad,
		0 != (held & (WPAD_BUTTON_1 | INPUT_BUTTON_C)));
	MacKeySet(MKC_Option, kSrcPad, 0 != (held & WPAD_BUTTON_2));

	UpdateUSBKeyboard(trueblnr);

	/* pointer: Wii Remote IR or USB mouse, absolute */
	{
		blnr onOsk = PointerActive && PointInOsk(PointerX, PointerY);
		blnr clickDown = (0 != (down & PadMouseButtons))
			|| (0 != (INPUT_MouseButtonsPressed()
				& (INPUT_MOUSE_LEFT | INPUT_MOUSE_RIGHT)));
		blnr clickHeld = (0 != (held & PadMouseButtons))
			|| (0 != (INPUT_MouseButtonsHeld()
				& (INPUT_MOUSE_LEFT | INPUT_MOUSE_RIGHT)));

		if (OskVisible) {
			if (onOsk && clickDown && ! MacMouseDown) {
				int k = OskKeyAt(PointerX, PointerY);

				if (k >= 0) {
					OskPress(k);
				}
			}
			if ((OskPressed >= 0) && ! clickHeld) {
				OskRelease();
			}
		}

		if (PointerActive && ! onOsk && (OskPressed < 0)
			&& (INPUT_CursorMoved() || clickDown || MacMouseDown))
		{
			MacMouseX = (PointerX - DispX) * vMacScreenWidth / DispW;
			MacMouseY = (PointerY - DispY) * vMacScreenHeight / DispH;
		}

		if (MacMouseDown) {
			MacMouseDown = clickHeld;
		} else if (clickDown && ! onOsk && (OskPressed < 0)) {
			MacMouseDown = trueblnr;
		}
	}

	/* analog sticks move the mouse relative to where it is */
	{
		f32 sx;
		f32 sy;
		f32 len = INPUT_Stick(INPUT_STICK_LEFT, &sx, &sy, NULL);

		if (len > 0.15f) {
			f32 speed = 2.0f + 8.0f * (len - 0.15f) / 0.85f;

			MacMouseX += sx / len * speed * len;
			MacMouseY -= sy / len * speed * len;
		}
	}

	ClampMacMouse();
}

/* ======================================================================
	Time
   ====================================================================== */

#define dbglog_TimeStuff (0 && dbglog_HAVE)

LOCALVAR ui5b TrueEmulatedTime = 0;

#define MyInvTimeDivPow 16
#define MyInvTimeDiv (1 << MyInvTimeDivPow)
#define MyInvTimeDivMask (MyInvTimeDiv - 1)
#define MyInvTimeStep 1089590 /* 1000 / 60.14742 * MyInvTimeDiv */

LOCALVAR u32 LastTime;
LOCALVAR u32 NextIntTime;
LOCALVAR u32 NextFracTime;

/* seconds since 1904 at MacDateBaseMs */
LOCALVAR ui5b MacDateBase;
LOCALVAR u32 MacDateBaseMs;

LOCALFUNC u32 GetMs(void)
{
	return (u32)OSTicksToMilliseconds(OSGetSystemTime());
}

LOCALPROC IncrNextTime(void)
{
	NextFracTime += MyInvTimeStep;
	NextIntTime += (NextFracTime >> MyInvTimeDivPow);
	NextFracTime &= MyInvTimeDivMask;
}

LOCALPROC InitNextTime(void)
{
	NextIntTime = LastTime;
	NextFracTime = 0;
	IncrNextTime();
}

LOCALVAR ui5b NewMacDateInSeconds;

LOCALFUNC blnr UpdateTrueEmulatedTime(void)
{
	u32 LatestTime;
	si5b TimeDiff;

	LatestTime = GetMs();
	if (LatestTime != LastTime) {

		NewMacDateInSeconds = MacDateBase
			+ (LatestTime - MacDateBaseMs) / 1000;

		LastTime = LatestTime;
		TimeDiff = (LatestTime - NextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 256) {
				/* emulation interrupted, forget it */
				++TrueEmulatedTime;
				InitNextTime();
			} else {
				do {
					++TrueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - NextIntTime);
				} while (TimeDiff >= 0);
			}
			return trueblnr;
		} else {
			if (TimeDiff < -256) {
				/* clock goofed if ever get here, reset */
				InitNextTime();
			}
		}
	}
	return falseblnr;
}

LOCALFUNC blnr CheckDateTime(void)
{
	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return trueblnr;
	} else {
		return falseblnr;
	}
}

LOCALPROC StartUpTimeAdjust(void)
{
	LastTime = GetMs();
	InitNextTime();
}

LOCALFUNC blnr InitLocationDat(void)
{
	/* The console clock keeps local time; Mac time counts from 1904. */
	MacDateBase = (ui5b)time(NULL) + 2082844800UL;
	MacDateBaseMs = GetMs();

	LastTime = MacDateBaseMs;
	InitNextTime();
	NewMacDateInSeconds = MacDateBase;
	CurMacDateInSeconds = NewMacDateInSeconds;

	return trueblnr;
}

/* ======================================================================
	Sound (SDL2)
   ====================================================================== */

#if MySoundEnabled

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + kLn2SoundSampSz - 3)
#define kLnAllBuffSz (kLnAllBuffLen + kLn2SoundSampSz - 3)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

LOCALVAR tpSoundSamp TheSoundBuffer = nullpr;
volatile static ui4b ThePlayOffset;
volatile static ui4b TheFillOffset;
volatile static ui4b MinFilledSoundBuffs;
LOCALVAR ui4b TheWriteOffset;

LOCALPROC MySound_Init0(void)
{
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
}

LOCALPROC MySound_Start0(void)
{
	/* Reset variables */
	MinFilledSoundBuffs = kSoundBuffers + 1;
}

GLOBALOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	ui4b ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	ui4b WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

LOCALFUNC blnr MySound_EndWrite0(ui4r actL)
{
	blnr v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = falseblnr;
	} else {
		/* just finished a block */
		TheFillOffset = TheWriteOffset;

		v = trueblnr;
	}

	return v;
}

LOCALPROC MySound_SecondNotify0(void)
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
			IncrNextTime();
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
			++TrueEmulatedTime;
		}
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

typedef ui4r trSoundTemp;

#define kCenterTempSound 0x8000

#define AudioStepVal 0x0040

#if 3 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) << 8)
#define ConvertTempSoundSampleToNative(v) ((v) >> 8)
#elif 4 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) + kCenterSound)
#define ConvertTempSoundSampleToNative(v) ((v) - kCenterSound)
#else
#error "unsupported kLn2SoundSampSz"
#endif

LOCALPROC SoundRampTo(trSoundTemp *last_val, trSoundTemp dst_val,
	tpSoundSamp *stream, int *len)
{
	trSoundTemp diff;
	tpSoundSamp p = *stream;
	int n = *len;
	trSoundTemp v1 = *last_val;

	while ((v1 != dst_val) && (0 != n)) {
		if (v1 > dst_val) {
			diff = v1 - dst_val;
			if (diff > AudioStepVal) {
				v1 -= AudioStepVal;
			} else {
				v1 = dst_val;
			}
		} else {
			diff = dst_val - v1;
			if (diff > AudioStepVal) {
				v1 += AudioStepVal;
			} else {
				v1 = dst_val;
			}
		}

		--n;
		*p++ = ConvertTempSoundSampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*last_val = v1;
}

struct MySoundR {
	tpSoundSamp fTheSoundBuffer;
	volatile ui4b (*fPlayOffset);
	volatile ui4b (*fFillOffset);
	volatile ui4b (*fMinFilledSoundBuffs);

	volatile trSoundTemp lastv;

	volatile blnr wantplaying;
	volatile blnr HaveStartedPlaying;
};
typedef struct MySoundR MySoundR;

/* Fill len samples of the output stream (called from SDL's audio
   thread). */
static void my_audio_callback(void *udata, ui3p stream, int len)
{
	ui4b ToPlayLen;
	ui4b FilledSoundBuffs;
	int i;
	MySoundR *datp = (MySoundR *)udata;
	tpSoundSamp CurSoundBuffer = datp->fTheSoundBuffer;
	ui4b CurPlayOffset = *datp->fPlayOffset;
	trSoundTemp v0 = datp->lastv;
	trSoundTemp v1 = v0;
	tpSoundSamp dst = (tpSoundSamp)stream;

label_retry:
	ToPlayLen = *datp->fFillOffset - CurPlayOffset;
	FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

	if (! datp->wantplaying) {
		SoundRampTo(&v1, kCenterTempSound, &dst, &len);

		ToPlayLen = 0;
	} else if (! datp->HaveStartedPlaying) {
		if ((ToPlayLen >> kLnOneBuffLen) < 8) {
			ToPlayLen = 0;
		} else {
			tpSoundSamp p = datp->fTheSoundBuffer
				+ (CurPlayOffset & kAllBuffMask);
			trSoundTemp v2 = ConvertTempSoundSampleFromNative(*p);

			SoundRampTo(&v1, v2, &dst, &len);

			if (v1 == v2) {
				datp->HaveStartedPlaying = trueblnr;
			}
		}
	}

	if (0 == len) {
		/* done */

		if (FilledSoundBuffs < *datp->fMinFilledSoundBuffs) {
			*datp->fMinFilledSoundBuffs = FilledSoundBuffs;
		}
	} else if (0 == ToPlayLen) {
		/* under run */
		for (i = 0; i < len; ++i) {
			*dst++ = ConvertTempSoundSampleToNative(v1);
		}
		*datp->fMinFilledSoundBuffs = 0;
	} else {
		ui4b PlayBuffContig = kAllBuffLen
			- (CurPlayOffset & kAllBuffMask);
		tpSoundSamp p = CurSoundBuffer
			+ (CurPlayOffset & kAllBuffMask);

		if (ToPlayLen > PlayBuffContig) {
			ToPlayLen = PlayBuffContig;
		}
		if (ToPlayLen > len) {
			ToPlayLen = len;
		}

		for (i = 0; i < ToPlayLen; ++i) {
			*dst++ = *p++;
		}
		v1 = ConvertTempSoundSampleFromNative(p[-1]);

		CurPlayOffset += ToPlayLen;
		len -= ToPlayLen;

		*datp->fPlayOffset = CurPlayOffset;

		goto label_retry;
	}

	datp->lastv = v1;
}

LOCALVAR MySoundR cur_audio;

LOCALVAR blnr HaveSoundOut = falseblnr;

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

LOCALVAR SDL_AudioDeviceID SndDev = 0;

static void SndCallback(void *udata, Uint8 *stream, int len)
{
	/* one byte per sample (kLn2SoundSampSz 3) */
	my_audio_callback(udata, stream, len >> (kLn2SoundSampSz - 3));
}

LOCALPROC MySound_Stop(void)
{
	if (cur_audio.wantplaying && HaveSoundOut) {
		ui4r retry_limit = 50; /* half of a second */

		cur_audio.wantplaying = falseblnr;

		while ((kCenterTempSound != cur_audio.lastv)
			&& (0 != --retry_limit))
		{
			SDL_Delay(10);
		}
	}
}

LOCALPROC MySound_Start(void)
{
	if ((! cur_audio.wantplaying) && HaveSoundOut) {
		MySound_Start0();
		cur_audio.lastv = kCenterTempSound;
		cur_audio.HaveStartedPlaying = falseblnr;
		cur_audio.wantplaying = trueblnr;
	}
}

LOCALPROC MySound_UnInit(void)
{
	if (HaveSoundOut) {
		SDL_CloseAudioDevice(SndDev);
		SndDev = 0;
		HaveSoundOut = falseblnr;
	}
}

LOCALFUNC blnr MySound_Init(void)
{
	SDL_AudioSpec want;
	SDL_AudioSpec have;

	MySound_Init0();

	cur_audio.fTheSoundBuffer = TheSoundBuffer;
	cur_audio.fPlayOffset = &ThePlayOffset;
	cur_audio.fFillOffset = &TheFillOffset;
	cur_audio.fMinFilledSoundBuffs = &MinFilledSoundBuffs;
	cur_audio.wantplaying = falseblnr;
	cur_audio.lastv = kCenterTempSound;

	SDL_zero(want);
	want.freq = SOUND_SAMPLERATE;
#if 3 == kLn2SoundSampSz
	want.format = AUDIO_U8;
#else
	want.format = AUDIO_S16SYS;
#endif
	want.channels = 1;
	want.samples = 1024;
	want.callback = SndCallback;
	want.userdata = &cur_audio;

	/* SDL converts to the console's own rate and format */
	SndDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (0 != SndDev) {
		HaveSoundOut = trueblnr;
		MySound_Start();
		SDL_PauseAudioDevice(SndDev, 0);
	}

	return trueblnr; /* keep going, even if no sound */
}

GLOBALOSGLUPROC MySound_EndWrite(ui4r actL)
{
	if (MySound_EndWrite0(actL)) {
	}
}

LOCALPROC MySound_SecondNotify(void)
{
	if (HaveSoundOut) {
		MySound_SecondNotify0();
	}
}

#endif

/* --- basic dialogs --- */

LOCALPROC CheckSavedMacMsg(void)
{
	/* called only on quit, if error saved but not yet reported */

	if (nullpr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		fprintf(stderr, "%s\n", briefMsg0);
		fprintf(stderr, "%s\n", longMsg0);

		SavedBriefMsg = nullpr;
	}
}

/* --- main window creation and disposal --- */

LOCALVAR int my_argc;
LOCALVAR char **my_argv;

GLOBALPROC ToggleWantFullScreen(void)
{
	/* always full screen */
}

LOCALPROC ZapWinStateVars(void)
{
}

/* --- SavedTasks --- */

LOCALVAR blnr CurSpeedStopped = trueblnr;

LOCALPROC LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

LOCALPROC EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

LOCALPROC CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = falseblnr;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = falseblnr;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = trueblnr;
		}
	}

	if (ShutdownRequested || ResetRequested) {
		ForceMacOff = trueblnr;
	}

	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped != (SpeedStopped || UIPausesEmulation())) {
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullpr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = falseblnr;
		ScreenChangedAll();
	}
}

LOCALPROC MyDrawChangesAndClear(void)
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

GLOBALOSGLUPROC DoneWithDrawingForTick(void)
{
#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
}

/* --- command line parsing --- */

LOCALFUNC blnr ScanCommandLine(void)
{
	char *pa;
	int i = 1;

label_retry:
	if (i < my_argc) {
		pa = my_argv[i++];
		if ('-' == pa[0]) {
			if ((0 == strcmp(pa, "--rom"))
				|| (0 == strcmp(pa, "-r")))
			{
				if (i < my_argc) {
					rom_path = my_argv[i++];
					goto label_retry;
				}
			}
			/* ignore other options */
			goto label_retry;
		} else {
			(void) Sony_Insert1(pa, falseblnr);
			goto label_retry;
		}
	}

	return trueblnr;
}

/* --- main program flow --- */

GLOBALOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();
	return TrueEmulatedTime == OnTrueTime;
}

LOCALVAR u32 LastPollRetrace = 0xFFFFFFFF;

/* Input and drawing run once per video field. */
LOCALPROC CheckForSystemEvents(void)
{
	u32 r = RetraceCount;
	SDL_Event ev;

	/* SDL runs ProcUI: it reports the power button and the system
	   asking the app to close as SDL_QUIT */
	while (SDL_PollEvent(&ev)) {
		if (SDL_QUIT == ev.type) {
			ShutdownRequested = trueblnr;
		}
	}

	if (r != LastPollRetrace) {
		LastPollRetrace = r;
		PollInput();
	}

	MaybeRenderFrame();
}

GLOBALOSGLUPROC WaitForNextTick(void)
{
label_retry:
	CheckForSystemEvents();
	CheckForSavedTasks();

	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped) {
		DoneWithDrawingForTick();
		SDL_Delay(2);
		goto label_retry;
	}

	if (ExtraTimeNotOver()) {
		SDL_Delay(1);
		goto label_retry;
	}

	if (CheckDateTime()) {
#if MySoundEnabled
		MySound_SecondNotify();
#endif
#if EnableDemoMsg
		DemoModeSecondNotify();
#endif
	}

	CheckMouseState();

	OnTrueTime = TrueEmulatedTime;
}

/* --- platform independent code can be thought of as going here --- */

#include "PROGMAIN.h"

LOCALPROC ZapOSGLUVars(void)
{
	InitDrives();
	ZapWinStateVars();
}

LOCALPROC ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, trueblnr);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, falseblnr);
#endif

#if MySoundEnabled
	ReserveAllocOneBlock((ui3p *)&TheSoundBuffer,
		dbhBufferSize, 5, falseblnr);
#endif

	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	blnr IsOk = falseblnr;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (ui3p)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, trueblnr);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

LOCALPROC UnallocMyMemory(void)
{
	if (nullpr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
	}
}

/* --- Wii U system --- */

LOCALPROC SetAppDir(void)
{
	/* a .wuhb has no folder of its own: settings and disk images live
	   next to it on the SD card */
	snprintf(AppDir, sizeof(AppDir),
		SDRoot "/wiiu/apps/" APP_FOLDER "/");
	(void) mkdir(SDRoot "/wiiu", 0777);
	(void) mkdir(SDRoot "/wiiu/apps", 0777);
	(void) mkdir(AppDir, 0777);
	if (0 != chdir(AppDir)) {
		AppDir[0] = 0;
	}
}

LOCALFUNC blnr InitWiiUSystem(void)
{
	StartTime = OSGetSystemTime();

	SetAppDir();
	LoadSettings();

	/* before SDL: the USB keyboard library takes one client, and the
	   keyboard is libwiiuinput's (SDL carries on without it) */
	INPUT_Init();
	INPUT_SetScreenSize(VirtW, VirtH);
	INPUT_SetCursorMode(INPUT_CURSOR_IR | INPUT_CURSOR_MOUSE
		| INPUT_CURSOR_TOUCH);
	/* a touch on the GamePad clicks where it touches */
	INPUT_SetTouchButton(WPAD_BUTTON_A);
	/* the keyboard is a Mac keyboard here, not a controller */
	INPUT_KeyboardMapClear();
	/* HOME opens the emulator's menu, which can quit */
	OSEnableHomeButtonMenu(FALSE);

	if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		return falseblnr;
	}

	InitHidMap();
	InitKeyCodes();

#ifdef TEST_SHOW_UI
	/* emulator test runs: show the menu and keyboard from the start */
	MenuOpen = trueblnr;
	OskVisible = trueblnr;
#endif

	(void) WIIUUPDATE_Start(UPDATE_OWNER, UPDATE_REPO, APP_VERSION);

	return trueblnr;
}

LOCALFUNC blnr InitOSGLU(void)
{
	if (InitWiiUSystem())
	if (Screen_Init())
	if (AllocMyMemory())
#if dbglog_HAVE
	if (dbglog_open())
#endif
	if (ScanCommandLine())
	if (LoadMacRom())
	if (LoadInitialImages())
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (WaitForRom())
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC UnInitOSGLU(void)
{
	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

#if MySoundEnabled
	MySound_Stop();
	MySound_UnInit();
#endif
#if IncludePbufs
	UnInitPbufs();
#endif
	UnInitDrives();

#if dbglog_HAVE
	dbglog_close();
#endif

	UnallocMyMemory();

	CheckSavedMacMsg();

	WIIUUPDATE_Shutdown();

	Screen_UnInit();
	OSEnableHomeButtonMenu(TRUE);
	INPUT_Shutdown();
}

int main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ZapOSGLUVars();
	if (InitOSGLU()) {
		ProgramMain();
	}
	UnInitOSGLU();

	/* SDL hands the app back to ProcUI and the Wii U Menu */
	SDL_Quit();

	return 0;
}
