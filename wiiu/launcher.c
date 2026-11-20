/*
	launcher.c

	Entry point of the all-in-one Mini vMac Wii U app. Every Mac in models.h
	is the whole Mini vMac (emulator and Wii U glue) built for that model,
	with its global symbols renamed mv_<id>_... so they can share one RPX.
	The launcher picks a model (the last one used, after a short countdown,
	or whichever the user selects), runs its main and, when the model asks
	for it from its HOME menu, shows the picker again.

	The launcher owns what must outlive a model: the controllers, SDL, the
	window and the renderer. A model's data and bss are gathered at link
	time between mv_<id>_data_start/_end and mv_<id>_bss_start/_end, so the
	launcher can put them back to their initial state before starting a
	model again.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

#include <wut.h>
#include <coreinit/foreground.h>
#include <SDL2/SDL.h>

#include <input_wiiu.h>

#include "launcher.h"

#ifndef APP_VERSION
#define APP_VERSION "1.1.0"
#endif
#ifndef APP_FOLDER
#define APP_FOLDER "MinivMacWiiU"
#endif
#ifndef APP_TITLE
#define APP_TITLE "Mini vMac Wii U"
#endif

#define SDRoot "fs:/vol/external01"
#define MODEL_FILE "model.txt"
#define COUNTDOWN_SECS 3

/* the same picture size as the models (OSGLUWIU.c) */
#define VirtW 854
#define VirtH 480
#define RGBA(r, g, b, a) \
	(((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) \
	| (uint32_t)(a))

typedef uint8_t ui3b;
#include "WIIUFONT.h"

#define MODEL(id, name, rom) \
	extern int mv_##id##_main(int argc, char **argv); \
	extern char mv_##id##_data_start[], mv_##id##_data_end[]; \
	extern char mv_##id##_bss_start[], mv_##id##_bss_end[];
#include "models.h"
#undef MODEL

typedef struct {
	const char *id;
	const char *name;
	const char *rom;
	int (*main)(int argc, char **argv);
	char *data_start, *data_end;
	char *bss_start, *bss_end;
	void *initial_data; /* copy of the data before the first run */
	int have_rom;
} Model;

static Model Models[] = {
#define MODEL(id, name, rom) \
	{ #id, name, rom, mv_##id##_main, \
	  mv_##id##_data_start, mv_##id##_data_end, \
	  mv_##id##_bss_start, mv_##id##_bss_end, NULL, 0 },
#include "models.h"
#undef MODEL
};
#define NumModels ((int)(sizeof(Models) / sizeof(Models[0])))

/* the model chosen when nothing was saved: the most capable one with a ROM */
static const char *const PreferredOrder[] = {
	"II", "IIx", "Classic", "SE", "Plus", "512Ke", "128K"
};

static char AppDir[256] = "";
static volatile int SwitchRequested = 0;
static volatile int QuitRequested = 0;

static SDL_Window *Window = NULL;
static SDL_Renderer *Renderer = NULL;
static SDL_Texture *FontTex = NULL;

SDL_Window *MVL_Window(void)
{
	return Window;
}

SDL_Renderer *MVL_Renderer(void)
{
	return Renderer;
}

void MVL_RequestSwitch(void)
{
	SwitchRequested = 1;
}

void MVL_RequestQuit(void)
{
	QuitRequested = 1;
}

/* --- files --- */

static void SetAppDir(void)
{
	/* a .wuhb has no folder of its own: settings, ROMs and disk images
	   live next to it on the SD card */
	snprintf(AppDir, sizeof(AppDir), SDRoot "/wiiu/apps/" APP_FOLDER "/");
	(void) mkdir(SDRoot "/wiiu", 0777);
	(void) mkdir(SDRoot "/wiiu/apps", 0777);
	(void) mkdir(AppDir, 0777);
	if (0 != chdir(AppDir)) {
		AppDir[0] = 0;
	}
}

static int FileExists(const char *path)
{
	struct stat st;

	return (0 == stat(path, &st)) && S_ISREG(st.st_mode);
}

/* Same places the models look in (see LoadMacRom in OSGLUWIU.c). */
static void FindRoms(void)
{
	int i;
	unsigned int j;
	char path[256];

	for (i = 0; i < NumModels; ++i) {
		Model *m = &Models[i];
		const char *dirs[4];
		char modeldir[32];

		snprintf(modeldir, sizeof(modeldir), "%s/", m->id);
		dirs[0] = modeldir;
		dirs[1] = "";
		dirs[2] = SDRoot "/minivmac/";
		dirs[3] = SDRoot "/";

		m->have_rom = 0;
		for (j = 0; j < sizeof(dirs) / sizeof(dirs[0]); ++j) {
			snprintf(path, sizeof(path), "%s%s", dirs[j], m->rom);
			if (FileExists(path)) {
				m->have_rom = 1;
				break;
			}
		}
	}
}

static int ModelIndex(const char *id)
{
	int i;

	for (i = 0; i < NumModels; ++i) {
		if (0 == strcmp(Models[i].id, id)) {
			return i;
		}
	}
	return -1;
}

static int LoadSavedModel(void)
{
	char line[32];
	FILE *f = fopen(MODEL_FILE, "r");
	int i = -1;

	if (NULL != f) {
		if (NULL != fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\r\n ")] = 0;
			i = ModelIndex(line);
		}
		fclose(f);
	}
	return i;
}

static void SaveModel(int i)
{
	FILE *f;

	if ((0 == AppDir[0]) || (0 != chdir(AppDir))) {
		return;
	}
	f = fopen(MODEL_FILE, "w");
	if (NULL != f) {
		fprintf(f, "%s\n", Models[i].id);
		fclose(f);
	}
}

/* --- drawing --- */

static int BuildFontTexture(void)
{
	static uint32_t pix[256][128];
	int c, gx, gy;

	for (c = 0; c < 256; ++c) {
		int col = c & 15;
		int row = c >> 4;

		for (gy = 0; gy < 16; ++gy) {
			uint8_t bits = console_font_8x16[c * 16 + gy];

			for (gx = 0; gx < 8; ++gx) {
				pix[row * 16 + gy][col * 8 + gx] =
					(bits & (0x80 >> gx)) ? RGBA(255, 255, 255, 255) : 0;
			}
		}
	}

	FontTex = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STATIC, 128, 256);
	if (NULL == FontTex) {
		return 0;
	}
	SDL_UpdateTexture(FontTex, NULL, pix, 128 * sizeof(uint32_t));
	SDL_SetTextureBlendMode(FontTex, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(FontTex, SDL_ScaleModeNearest);
	return 1;
}

static void FillRect(float x, float y, float w, float h, uint32_t color)
{
	SDL_FRect r;

	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	SDL_SetRenderDrawBlendMode(Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(Renderer, (color >> 24) & 0xFF,
		(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
	SDL_RenderFillRectF(Renderer, &r);
}

static void DrawText(float x, float y, const char *s, uint32_t color)
{
	SDL_SetTextureColorMod(FontTex, (color >> 24) & 0xFF,
		(color >> 16) & 0xFF, (color >> 8) & 0xFF);
	SDL_SetTextureAlphaMod(FontTex, color & 0xFF);

	for (; *s; ++s, x += 8) {
		uint8_t c = (uint8_t)*s;

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
			SDL_RenderCopyF(Renderer, FontTex, &src, &dst);
		}
	}
}

static void DrawTextCentered(float cx, float y, const char *s, uint32_t color)
{
	DrawText(cx - 4.0f * strlen(s), y, s, color);
}

#define PickerW 520.0f
#define PickerX ((VirtW - PickerW) / 2)
#define PickerY 40.0f
#define RowsY (PickerY + 70)
#define RowH 26.0f

static int RowAt(int px, int py)
{
	int i;

	if ((px < PickerX) || (px >= PickerX + PickerW) || (py < RowsY)) {
		return -1;
	}
	i = (int)((py - RowsY) / RowH);
	return (i < NumModels) ? i : -1;
}

static void DrawPicker(int sel, int countdown, int pointer, int px, int py)
{
	const float h = 70 + NumModels * RowH + 110;
	char buf[80];
	int i;

	SDL_SetRenderDrawColor(Renderer, 40, 40, 48, 255);
	SDL_RenderClear(Renderer);

	FillRect(PickerX, PickerY, PickerW, h, RGBA(235, 235, 240, 255));
	FillRect(PickerX, PickerY, PickerW, 26, RGBA(0, 0, 0, 255));
	snprintf(buf, sizeof(buf), APP_TITLE " %s", APP_VERSION);
	DrawTextCentered(VirtW / 2, PickerY + 5, buf, RGBA(255, 255, 255, 255));
	DrawTextCentered(VirtW / 2, PickerY + 40, "Choose the Mac to start:",
		RGBA(0, 0, 0, 255));

	for (i = 0; i < NumModels; ++i) {
		Model *m = &Models[i];
		float y = RowsY + i * RowH;
		int on = (i == sel);

		if (on) {
			FillRect(PickerX + 8, y, PickerW - 16, RowH - 2,
				RGBA(0, 0, 0, 255));
		}
		snprintf(buf, sizeof(buf), "%-20s %-12s", m->name, m->rom);
		DrawText(PickerX + 24, y + 4, buf,
			on ? RGBA(255, 255, 255, 255) : RGBA(0, 0, 0, 255));
		DrawText(PickerX + PickerW - 24 - 8 * 9, y + 4,
			m->have_rom ? "ROM found" : "no ROM",
			m->have_rom ? (on ? RGBA(120, 255, 120, 255) : RGBA(0, 120, 0, 255))
				: RGBA(140, 140, 140, 255));
	}

	{
		float y = RowsY + NumModels * RowH + 10;
		uint32_t c = RGBA(70, 70, 70, 255);

		if (countdown > 0) {
			snprintf(buf, sizeof(buf), "Starting the %s in %d...",
				Models[sel].name, countdown);
			DrawTextCentered(VirtW / 2, y, buf, RGBA(0, 0, 0, 255));
		}
		DrawText(PickerX + 16, y + 26,
			"D-Pad or point, then A or touch: start", c);
		DrawText(PickerX + 16, y + 44,
			"HOME: quit to the Wii U Menu", c);
		DrawText(PickerX + 16, y + 62,
			"ROMs and disk images: sd:/wiiu/apps/" APP_FOLDER "/", c);
		DrawText(PickerX + 16, y + 80,
			"or in the Mac's folder there, e.g. II/MacII.ROM", c);
	}

	if (pointer) {
		float x = (float)px;
		float y = (float)py;
		SDL_Vertex v[3];
		int k;

		memset(v, 0, sizeof(v));
		v[0].position.x = x;     v[0].position.y = y;
		v[1].position.x = x;     v[1].position.y = y + 16;
		v[2].position.x = x + 11; v[2].position.y = y + 12;
		for (k = 0; k < 3; ++k) {
			v[k].color.r = v[k].color.g = v[k].color.b = 255;
			v[k].color.a = 255;
		}
		SDL_RenderGeometry(Renderer, NULL, v, 3, NULL, 0);
	}

	SDL_RenderPresent(Renderer);
}

/*
	Returns the model to start, or -1 to quit. With autostart set, the
	selected model starts by itself after a countdown unless a button is
	pressed.
*/
static int RunPicker(int sel, int autostart)
{
	uint32_t start = SDL_GetTicks();
	int countdown = autostart ? COUNTDOWN_SECS : 0;
	int result = -2;
	SDL_Event ev;

	while (-2 == result) {
		uint32_t down;
		int pointer, px = 0, py = 0;

		while (SDL_PollEvent(&ev)) {
			if (SDL_QUIT == ev.type) {
				QuitRequested = 1;
			}
		}
		if (QuitRequested) {
			result = -1;
			break;
		}

		INPUT_Update();
		down = INPUT_PadsDown();
		pointer = INPUT_CursorActive();
		if (pointer) {
			INPUT_CursorPosition(&px, &py);
			if (INPUT_CursorMoved() && (RowAt(px, py) >= 0)) {
				sel = RowAt(px, py);
				countdown = 0;
			}
		}

		if (0 != down) {
			countdown = 0;
		}
		if (down & WPAD_BUTTON_UP) {
			sel = (sel + NumModels - 1) % NumModels;
		}
		if (down & WPAD_BUTTON_DOWN) {
			sel = (sel + 1) % NumModels;
		}
		if (down & (WPAD_BUTTON_A | WPAD_BUTTON_PLUS)) {
			/* a touch or a click starts the Mac it is on only */
			if ((! pointer) || (down & WPAD_BUTTON_PLUS)
				|| (RowAt(px, py) == sel))
			{
				result = sel;
			}
		}
		if (down & WPAD_BUTTON_HOME) {
			result = -1;
		}

		if (countdown > 0) {
			int left = COUNTDOWN_SECS - (int)((SDL_GetTicks() - start) / 1000);

			if (left <= 0) {
				result = sel;
			} else {
				countdown = left;
			}
		}

		DrawPicker(sel, countdown, pointer, px, py);
		SDL_Delay(8);
	}

	return result;
}

/* --- running a model --- */

static int RunModel(int i, int argc, char **argv)
{
	Model *m = &Models[i];
	size_t dsz = (size_t)(m->data_end - m->data_start);

	if (NULL == m->initial_data) {
		m->initial_data = malloc(dsz ? dsz : 1);
		if (NULL == m->initial_data) {
			return 0;
		}
		memcpy(m->initial_data, m->data_start, dsz);
	} else {
		/* started before: back to a fresh program */
		memcpy(m->data_start, m->initial_data, dsz);
		memset(m->bss_start, 0, (size_t)(m->bss_end - m->bss_start));
	}

	SaveModel(i);
	SwitchRequested = 0;

	(void) m->main(argc, argv);

	if (0 != AppDir[0]) {
		(void) chdir(AppDir);
	}

	return SwitchRequested && ! QuitRequested;
}

int main(int argc, char **argv)
{
	int sel;
	int autostart = 1;
	int avail = 0;
	int only = -1;
	int i;

	SetAppDir();
	FindRoms();

	/* before SDL: the USB keyboard library takes one client, and the
	   keyboard is libwiiuinput's (SDL carries on without it) */
	INPUT_Init();
	INPUT_SetScreenSize(VirtW, VirtH);
	INPUT_SetCursorMode(INPUT_CURSOR_IR | INPUT_CURSOR_MOUSE
		| INPUT_CURSOR_TOUCH);
	/* a touch on the GamePad clicks where it touches */
	INPUT_SetTouchButton(WPAD_BUTTON_A);
	/* HOME opens the emulator's menu, which can quit */
	OSEnableHomeButtonMenu(FALSE);

	if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
		return 1;
	}
	/* One window: SDL shows it on the TV and on the GamePad. */
	Window = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
	if (NULL != Window) {
		Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);
	}
	if ((NULL == Renderer) || ! (SDL_RenderSetLogicalSize(Renderer, VirtW, VirtH),
		BuildFontTexture()))
	{
		SDL_Quit();
		return 1;
	}

	sel = LoadSavedModel();
	for (i = 0; i < NumModels; ++i) {
		if (Models[i].have_rom) {
			++avail;
			only = i;
		}
	}
	if ((sel < 0) || ! Models[sel].have_rom) {
		unsigned int j;

		sel = -1;
		for (j = 0; (sel < 0)
			&& (j < sizeof(PreferredOrder) / sizeof(PreferredOrder[0])); ++j)
		{
			int k = ModelIndex(PreferredOrder[j]);

			if ((k >= 0) && Models[k].have_rom) {
				sel = k;
			}
		}
	}
	if (1 == avail) {
		/* nothing to choose from */
		sel = only;
		autostart = -1;
	} else if (sel < 0) {
		/* no ROM at all: let the user pick; the model's HOME menu can
		   load a ROM from the card */
		sel = ModelIndex("Plus");
		autostart = 0;
	}

	for (;;) {
		if (autostart >= 0) {
			sel = RunPicker(sel, autostart);
			if (sel < 0) {
				break;
			}
		}
		autostart = 0;

		if (! RunModel(sel, argc, argv)) {
			break;
		}
		FindRoms();
	}

	SDL_DestroyTexture(FontTex);
	SDL_DestroyRenderer(Renderer);
	SDL_DestroyWindow(Window);
	OSEnableHomeButtonMenu(TRUE);
	INPUT_Shutdown();

	/* SDL hands the app back to ProcUI and the Wii U Menu */
	SDL_Quit();

	return 0;
}
