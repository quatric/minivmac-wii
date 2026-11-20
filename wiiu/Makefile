#---------------------------------------------------------------------------------
# Mini vMac for the Nintendo Wii U (Aroma): one app with every Mac in models.h
#
#   make                  MinivMacWiiU.wuhb (the launcher and every model)
#   make dist             wiiu/apps/... folder and zip in dist/
#   make cfg              regenerate cfg/<model>/ (mkcfg.sh)
#
# Each model is the whole of Mini vMac (emulator and Wii U glue) compiled with
# its own configuration, partially linked to build/<model>/core.o, and its
# global symbols renamed mv_<model>_... (main becomes mv_<model>_main).
# core.ld gathers the model's data and bss so the launcher can reset them.
#
# Needs devkitPPC, wut and wiiu-sdl2. libwiiuinput and libwiiuupdate are git
# submodules here, libwiiosk is the Wii build's (../wii/libwiiosk):
# git submodule update --init
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitPro")
endif
export DEVKITPPC ?= $(DEVKITPRO)/devkitPPC

include $(DEVKITPRO)/wut/share/wut_rules

VERSION       ?= 1.1.0
UPDATE_OWNER  ?= quatric
# libwiiuupdate installs the release's Wii U zip over the running app
UPDATE_REPO   ?= minivmac-wii

APP_FOLDER    := MinivMacWiiU
APP_TITLE     := Mini vMac Wii U
TARGET        := $(APP_FOLDER)

MODELS        := 128K 512Ke Plus SE Classic II IIx

# emulated hardware per model (the setup tool's file lists)
EMU_COMPACT   := MINEM68K GLOBGLUE M68KITAB VIAEMDEV IWMEMDEV SCCEMDEV \
                 RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV MOUSEMDV \
                 SNDEMDEV PROGMAIN
EMU_OBJS_128K    := $(EMU_COMPACT) KBRDEMDV
EMU_OBJS_512Ke   := $(EMU_COMPACT) KBRDEMDV
EMU_OBJS_Plus    := $(EMU_COMPACT) KBRDEMDV
EMU_OBJS_SE      := $(EMU_COMPACT) ADBEMDEV
EMU_OBJS_Classic := $(EMU_COMPACT) ADBEMDEV
EMU_OBJS_II      := MINEM68K GLOBGLUE M68KITAB VIAEMDEV VIA2EMDV IWMEMDEV \
                    SCCEMDEV RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV \
                    VIDEMDEV MOUSEMDV ADBEMDEV ASCEMDEV PROGMAIN
EMU_OBJS_IIx     := $(EMU_OBJS_II)

MACNAME_128K     := Macintosh 128K
MACNAME_512Ke    := Macintosh 512Ke
MACNAME_Plus     := Macintosh Plus
MACNAME_SE       := Macintosh SE
MACNAME_Classic  := Macintosh Classic
MACNAME_II       := Macintosh II
MACNAME_IIx      := Macintosh IIx

SRC           := ../src
WIIUINPUT     := libwiiuinput
WIIUUPDATE    := libwiiuupdate
WIIOSK        := ../wii/libwiiosk
CROSS         := $(DEVKITPPC)/bin/powerpc-eabi-

# the controller library without the microphones and pen tablets
INPUT_OPTS    := WITH_MIC=0 WITH_TABLET=0

APP_NAME      := $(APP_TITLE)
APP_SHORTNAME := Mini vMac
APP_AUTHOR    := quatric
APP_ICON      := icon.png
APP_TV_SPLASH := splash-tv.png
APP_DRC_SPLASH := splash-drc.png
# wut_rules read these when it was included; set the options again here
WUHB_OPTIONS  := --name "$(APP_NAME)" --short-name "$(APP_SHORTNAME)" \
                 --author "$(APP_AUTHOR)" --icon=$(APP_ICON) \
                 --tv-image=$(APP_TV_SPLASH) --drc-image=$(APP_DRC_SPLASH)

INCLUDE       := -I. -I$(SRC) -I$(WIIUINPUT)/include \
                 -I$(WIIUUPDATE)/include -I$(WIIOSK)/include \
                 -I$(WUT_ROOT)/include \
                 $(foreach dir,$(PORTLIBS),-I$(dir)/include)

CFLAGS        := -O2 -Wall -Wno-unused-function -Wno-unused-variable \
                 -fno-strict-aliasing -ffunction-sections $(MACHDEP) \
                 -D__WIIU__ -D__WUT__ $(EXTRA_CFLAGS)
GLUEFLAGS     := -DAPP_VERSION=\"$(VERSION)\" -DUPDATE_OWNER=\"$(UPDATE_OWNER)\" \
                 -DUPDATE_REPO=\"$(UPDATE_REPO)\" -DAPP_FOLDER=\"$(APP_FOLDER)\" \
                 -DAPP_TITLE="\"$(APP_TITLE)\""
LDFLAGS       := $(MACHDEP) $(RPXSPECS) -Wl,-Map,build/$(TARGET).map
LIBS          := -L$(WIIUINPUT)/lib -L$(WIIUUPDATE)/lib -L$(WIIOSK)/lib \
                 $(foreach dir,$(PORTLIBS),-L$(dir)/lib) -L$(WUT_ROOT)/lib \
                 -lwiiuupdate -lwiiuosk -linput-wiiu -lSDL2 -lwut -lm

INPUT_LIB     := $(WIIUINPUT)/lib/libinput-wiiu.a
UPDATE_LIB    := $(WIIUUPDATE)/lib/libwiiuupdate.a
OSK_LIB       := $(WIIOSK)/lib/libwiiuosk.a

CORES         := $(foreach m,$(MODELS),build/$(m)/core.o)
LAUNCHER_OBJ  := build/launcher.o

all: $(TARGET).wuhb

$(TARGET).wuhb: $(TARGET).rpx $(APP_ICON) $(APP_TV_SPLASH) $(APP_DRC_SPLASH)
	@wuhbtool $< $@ $(WUHB_OPTIONS)
	@echo built ... $@

$(TARGET).rpx: build/$(TARGET).elf
	@elf2rpl $< $@
	@echo built ... $@

build/$(TARGET).elf: $(LAUNCHER_OBJ) $(CORES) $(INPUT_LIB) $(UPDATE_LIB) $(OSK_LIB)
	@echo linking ... $(notdir $@)
	@$(CC) $(LAUNCHER_OBJ) $(CORES) $(LDFLAGS) $(LIBS) -o $@

$(INPUT_LIB):
	@$(MAKE) --no-print-directory -C $(WIIUINPUT) $(INPUT_OPTS)

$(UPDATE_LIB):
	@$(MAKE) --no-print-directory -C $(WIIUUPDATE)

$(OSK_LIB):
	@$(MAKE) --no-print-directory -C $(WIIOSK) -f Makefile.wiiu

libs: $(INPUT_LIB) $(UPDATE_LIB) $(OSK_LIB)

# Rebuild what depends on VERSION, the update repository or EXTRA_CFLAGS
# when they change.
FLAGSTAMP := build/flags
$(shell mkdir -p build; echo '$(CFLAGS) $(GLUEFLAGS)' | cmp -s - $(FLAGSTAMP) \
	|| echo '$(CFLAGS) $(GLUEFLAGS)' > $(FLAGSTAMP))

$(LAUNCHER_OBJ): launcher.c launcher.h models.h $(SRC)/WIIUFONT.h Makefile $(FLAGSTAMP)
	@mkdir -p $(dir $@)
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) $(GLUEFLAGS) $(INCLUDE) -c $< -o $@

#---------------------------------------------------------------------------------
# one model: build/<model>/core.o
#
# -G0 keeps every variable out of the small data sections (which are reached
# through r13 and could not be moved), -fno-common puts them all in .bss.
#---------------------------------------------------------------------------------
define model_rules
OBJS_$(1)  := $$(addprefix build/$(1)/,$$(addsuffix .o,$$(EMU_OBJS_$(1)) OSGLUWIU))
CFLAGS_$(1) := $$(CFLAGS) -Wno-maybe-uninitialized -G0 -fno-common -Icfg/$(1) $$(INCLUDE) -DMV_LAUNCHER \
               -DMODEL_DIR=\"$(1)\" -DMAC_NAME="\"$$(MACNAME_$(1))\""

build/$(1)/OSGLUWIU.o: $$(SRC)/OSGLUWIU.c $$(SRC)/WIIUFONT.h launcher.h $$(wildcard cfg/$(1)/*.h) \
		$$(wildcard $$(SRC)/*.h) Makefile $$(FLAGSTAMP)
	@mkdir -p $$(dir $$@)
	@echo $(1): $$(notdir $$<)
	@$$(CC) $$(CFLAGS_$(1)) $$(GLUEFLAGS) -c $$< -o $$@

build/$(1)/%.o: $$(SRC)/%.c $$(wildcard cfg/$(1)/*.h) Makefile
	@mkdir -p $$(dir $$@)
	@echo $(1): $$(notdir $$<)
	@$$(CC) $$(CFLAGS_$(1)) -c $$< -o $$@

build/$(1)/core.o: $$(OBJS_$(1)) core.ld
	@echo $(1): $$(notdir $$@)
	@$$(CROSS)ld -r -T core.ld $$(OBJS_$(1)) -o build/$(1)/core-r.o
	@if $$(CROSS)objdump -h build/$(1)/core-r.o | grep -qE ' \.s(data|bss) '; then \
		echo "$(1): small data sections left, check -G0" >&2; exit 1; fi
	@$$(CROSS)nm -g --defined-only build/$(1)/core-r.o \
		| awk '{ print $$$$3, "mv_$(1)_" $$$$3 }' > build/$(1)/core.syms
	@$$(CROSS)objcopy --redefine-syms=build/$(1)/core.syms \
		build/$(1)/core-r.o $$@
endef
$(foreach m,$(MODELS),$(eval $(call model_rules,$(m))))

#---------------------------------------------------------------------------------
# packaging
#---------------------------------------------------------------------------------
DISTDIR := dist/wiiu/apps/$(APP_FOLDER)

# libwiiuupdate picks a release's zip with "wiiu" in its name, and
# libwiiupdate (the Wii build) never takes one.
dist: $(TARGET).wuhb
	@rm -rf $(DISTDIR) && mkdir -p $(DISTDIR)
	@cp $(TARGET).wuhb $(DISTDIR)/
	@for m in $(MODELS); do mkdir -p $(DISTDIR)/$$m; done
	@cd dist && rm -f $(APP_FOLDER)-$(VERSION)-wiiu.zip \
		&& zip -qr $(APP_FOLDER)-$(VERSION)-wiiu.zip wiiu/apps/$(APP_FOLDER)
	@echo built dist/$(APP_FOLDER)-$(VERSION)-wiiu.zip

cfg:
	@./mkcfg.sh $(MODELS)

clean:
	@rm -rf build dist *.elf *.rpx *.wuhb *.lst
	@$(MAKE) --no-print-directory -C $(WIIUINPUT) clean
	@$(MAKE) --no-print-directory -C $(WIIUUPDATE) clean
	@$(MAKE) --no-print-directory -C $(WIIOSK) -f Makefile.wiiu clean

.PHONY: all libs dist cfg clean
