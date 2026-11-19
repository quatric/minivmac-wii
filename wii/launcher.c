/*
	launcher.c

	Entry point of the all-in-one Mini vMac Wii app. Every Mac in models.h
	is the whole Mini vMac (emulator and Wii glue) built for that model,
	with its global symbols renamed mv_<id>_... so they can share one DOL.
	The launcher picks a model (the last one used, after a short countdown,
	or whichever the user selects), runs its main and, when the model asks
	for it from its HOME menu, shows the picker again.

	A model's data and bss are gathered at link time between
	mv_<id>_data_start/_end and mv_<id>_bss_start/_end, so the launcher can
	put them back to their initial state before starting a model again.
*/

#include <gccore.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

#include <input_wii.h>
#include <wiiupdate.h>

#include "launcher.h"

#ifndef APP_VERSION
#define APP_VERSION "1.1.0"
#endif
#ifndef UPDATE_OWNER
#define UPDATE_OWNER "quatric"
#endif
#ifndef UPDATE_REPO
#define UPDATE_REPO "minivmac-wii"
#endif
#define APP_FOLDER "MinivMacWii"
#define MODEL_FILE "model.txt"
#define COUNTDOWN_SECS 3

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
	char rom_where[64];
} Model;

static Model Models[] = {
#define MODEL(id, name, rom) \
	{ #id, name, rom, mv_##id##_main, \
	  mv_##id##_data_start, mv_##id##_data_end, \
	  mv_##id##_bss_start, mv_##id##_bss_end, NULL, 0, "" },
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
static volatile int PowerOff = 0;
static volatile int ResetPressed = 0;

static GXRModeObj *rmode = NULL;
static void *ConXfb = NULL;

void MVL_RequestSwitch(void)
{
	SwitchRequested = 1;
}

static void PowerCallback(void)
{
	PowerOff = 1;
}

static void WiimotePowerCallback(s32 chan)
{
	(void)chan;
	PowerOff = 1;
}

static void ResetCallback(u32 irq, void *ctx)
{
	(void)irq;
	(void)ctx;
	ResetPressed = 1;
}

/* --- files --- */

static void SetAppDir(int argc, char **argv)
{
	if ((argc > 0) && (NULL != argv[0]) && (NULL != strchr(argv[0], ':'))) {
		char *slash;

		snprintf(AppDir, sizeof(AppDir), "%s", argv[0]);
		slash = strrchr(AppDir, '/');
		if (NULL != slash) {
			slash[1] = 0;
		}
	} else {
		snprintf(AppDir, sizeof(AppDir), "sd:/apps/" APP_FOLDER "/");
	}

	if (0 != chdir(AppDir)) {
		snprintf(AppDir, sizeof(AppDir), "usb:/apps/" APP_FOLDER "/");
		if (0 != chdir(AppDir)) {
			AppDir[0] = 0;
		}
	}
}

static int FileExists(const char *path)
{
	struct stat st;

	return (0 == stat(path, &st)) && S_ISREG(st.st_mode);
}

/* Same places the models look in (see LoadMacRom in OSGLUWII.c). */
static void FindRoms(void)
{
	int i;
	unsigned int j;
	char path[256];

	for (i = 0; i < NumModels; ++i) {
		Model *m = &Models[i];
		const char *dirs[6];
		char modeldir[32];

		snprintf(modeldir, sizeof(modeldir), "%s/", m->id);
		dirs[0] = modeldir;
		dirs[1] = "";
		dirs[2] = "sd:/minivmac/";
		dirs[3] = "usb:/minivmac/";
		dirs[4] = "sd:/";
		dirs[5] = "usb:/";

		m->have_rom = 0;
		for (j = 0; j < sizeof(dirs) / sizeof(dirs[0]); ++j) {
			snprintf(path, sizeof(path), "%s%s", dirs[j], m->rom);
			if (FileExists(path)) {
				m->have_rom = 1;
				snprintf(m->rom_where, sizeof(m->rom_where), "%s",
					('\0' == dirs[j][0]) ? "app folder" : dirs[j]);
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

/* --- screen (libogc console) --- */

static void ScreenOn(void)
{
	if (NULL == rmode) {
		VIDEO_Init();
		rmode = VIDEO_GetPreferredMode(NULL);
		ConXfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	}
	/* a model may have widened the video line; the console wants the
	   standard picture */
	rmode = VIDEO_GetPreferredMode(NULL);
	VIDEO_Configure(rmode);
	VIDEO_ClearFrameBuffer(rmode, ConXfb, COLOR_BLACK);
	console_init(ConXfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
		rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_SetNextFramebuffer(ConXfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
}

static void ScreenOff(void)
{
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

#define C_RESET  "\x1b[0m"
#define C_TITLE  "\x1b[37;1m"
#define C_DIM    "\x1b[30;1m"
#define C_OK     "\x1b[32;1m"
#define C_SEL    "\x1b[47m\x1b[30m"

static void DrawPicker(int sel, int countdown)
{
	int i;

	printf("\x1b[2;0H");
	printf(C_TITLE "  Mini vMac Wii %s" C_RESET "\x1b[K\n\n", APP_VERSION);
	printf("  Choose the Mac to start:\x1b[K\n\n");

	for (i = 0; i < NumModels; ++i) {
		Model *m = &Models[i];
		char label[40];

		snprintf(label, sizeof(label), "%-20s %-13s", m->name, m->rom);
		if (i == sel) {
			printf("  " C_SEL " %s " C_RESET, label);
		} else {
			printf("   %s ", label);
		}
		if (m->have_rom) {
			printf(C_OK " ROM found" C_RESET "\x1b[K\n");
		} else {
			printf(C_DIM " no ROM" C_RESET "\x1b[K\n");
		}
	}

	printf("\x1b[K\n");
	if (countdown > 0) {
		printf("  Starting the %s in %d...\x1b[K\n", Models[sel].name,
			countdown);
	} else {
		printf("\x1b[K\n");
	}
	printf("\x1b[K\n");
	printf("  Up/Down: choose   A: start   HOME: quit to loader\x1b[K\n\n");
	printf(C_DIM "  Put each ROM in sd:/apps/" APP_FOLDER "/ (or in its\x1b[K\n");
	printf("  model's folder there, e.g. II/). Disk images go next to it:\x1b[K\n");
	printf("  <model>/disk1.dsk is used before disk1.dsk." C_RESET "\x1b[K\n");
}

/*
	Returns the model to start, or -1 to quit. With autostart set, the
	selected model starts by itself after a countdown unless a button is
	pressed.
*/
static int RunPicker(int sel, int autostart)
{
	u32 frames = 0;
	int countdown = autostart ? COUNTDOWN_SECS : 0;
	int result = -2;

	ScreenOn();
	printf("\x1b[2J");
	INPUT_Init();
	WPAD_SetPowerButtonCallback(WiimotePowerCallback);

	while (-2 == result) {
		u32 down;

		INPUT_Update();
		down = INPUT_ButtonsDown();

		if (PowerOff || ResetPressed) {
			result = -1;
			break;
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
			result = sel;
		}
		if (down & WPAD_BUTTON_HOME) {
			result = -1;
		}

		if ((countdown > 0) && (++frames % 60 == 0)) {
			if (0 == --countdown) {
				result = sel;
			}
		}

		DrawPicker(sel, countdown);
		VIDEO_WaitVSync();
	}

	INPUT_Shutdown();
	ScreenOff();
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
	DCFlushRange(m->data_start, dsz);

	SaveModel(i);
	SwitchRequested = 0;

	(void) m->main(argc, argv);

	/* the model set its own handlers */
	SYS_SetPowerCallback(PowerCallback);
	SYS_SetResetCallback(ResetCallback);
	if (0 != AppDir[0]) {
		(void) chdir(AppDir);
	}

	return SwitchRequested;
}

int main(int argc, char **argv)
{
	static const char *const updateFiles[] = {
		"boot.dol", "meta.xml", "icon.png", NULL
	};
	int sel = -1;
	int autostart = 1;
	int nargc = 0;
	char **nargv;
	int i;

	SYS_SetPowerCallback(PowerCallback);
	SYS_SetResetCallback(ResetCallback);

	(void) fatInitDefault();
	SetAppDir(argc, argv);

	/* --model <id> (from meta.xml <arguments>) starts that model at once;
	   the other arguments go to the model */
	nargv = (char **)calloc((size_t)argc + 1, sizeof(char *));
	for (i = 0; i < argc; ++i) {
		if ((0 == strcmp(argv[i], "--model")) && (i + 1 < argc)) {
			int k = ModelIndex(argv[++i]);

			if (k >= 0) {
				sel = k;
				autostart = -1;
			}
		} else if (NULL != nargv) {
			nargv[nargc++] = argv[i];
		}
	}
	if (NULL == nargv) {
		nargv = argv;
		nargc = argc;
	}

	FindRoms();

	if (sel < 0) {
		int avail = 0;
		int only = -1;

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
				&& (j < sizeof(PreferredOrder) / sizeof(PreferredOrder[0]));
				++j)
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
			/* no ROM at all: let the user pick; the model's HOME menu
			   can load a ROM from anywhere */
			sel = ModelIndex("Plus");
			autostart = 0;
		}
	}

	WIIUPDATE_SetAppPath((argc > 0) ? argv[0] : NULL);
	WIIUPDATE_SetUpdateFiles(updateFiles);
	(void) WIIUPDATE_Start(UPDATE_OWNER, UPDATE_REPO, APP_VERSION);

	for (;;) {
		if (autostart >= 0) {
			sel = RunPicker(sel, autostart);
			if (sel < 0) {
				break;
			}
		}
		autostart = 0;

		if (! RunModel(sel, nargc, nargv)) {
			break;
		}
		if (PowerOff) {
			break;
		}
		FindRoms();
#ifdef TEST_AUTOSWITCH
		/* emulator test runs: on to the next model with a ROM */
		for (i = 1; i <= NumModels; ++i) {
			if (Models[(sel + i) % NumModels].have_rom) {
				sel = (sel + i) % NumModels;
				break;
			}
		}
		autostart = 1;
#endif
	}

	if (WIIUPDATE_INSTALL_DOWNLOADING != WIIUPDATE_InstallState()) {
		WIIUPDATE_Cancel();
	}

	if (PowerOff) {
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	return 0;
}
