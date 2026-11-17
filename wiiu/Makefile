#---------------------------------------------------------------------------------
# Mini vMac for the Nintendo Wii U (Aroma)
#
#   make                  both models (Macintosh Plus and Macintosh II)
#   make MODEL=Plus       Macintosh Plus, 512x342 black and white (vMac.ROM)
#   make MODEL=II         Macintosh II, 640x480 256 colours (MacII.ROM)
#   make dist             wiiu/apps/... folders and one zip per model in dist/
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

VERSION       ?= 1.0.0
UPDATE_OWNER  ?= quatric
# libwiiuupdate installs the release's Wii U zip over the running app, so
# each model checks its own repository's releases, as on the Wii.
UPDATE_REPO   ?= minivmac-wii
UPDATE_REPO_II ?= $(UPDATE_REPO)-ii

MODELS        := Plus II
MODEL         ?=

SRC           := ../src
WIIUINPUT     := libwiiuinput
WIIUUPDATE    := libwiiuupdate
WIIOSK        := ../wii/libwiiosk

# the controller library without the microphones and pen tablets
INPUT_OPTS    := WITH_MIC=0 WITH_TABLET=0

ifeq ($(MODEL),)
#---------------------------------------------------------------------------------
# top level: build every model
#---------------------------------------------------------------------------------
all: $(MODELS)

# the libraries are built once, before the models (which build in parallel)
libs:
	@$(MAKE) --no-print-directory -C $(WIIUINPUT) $(INPUT_OPTS)
	@$(MAKE) --no-print-directory -C $(WIIUUPDATE)
	@$(MAKE) --no-print-directory -C $(WIIOSK) -f Makefile.wiiu

$(MODELS): libs
	@$(MAKE) --no-print-directory MODEL=$@

dist: $(MODELS)
	@for m in $(MODELS); do $(MAKE) --no-print-directory MODEL=$$m dist-model || exit 1; done

clean:
	@rm -rf build dist *.elf *.rpx *.wuhb *.lst
	@$(MAKE) --no-print-directory -C $(WIIUINPUT) clean
	@$(MAKE) --no-print-directory -C $(WIIUUPDATE) clean
	@$(MAKE) --no-print-directory -C $(WIIOSK) -f Makefile.wiiu clean

.PHONY: all libs dist clean $(MODELS)

else
#---------------------------------------------------------------------------------
# one model
#---------------------------------------------------------------------------------
ifeq ($(MODEL),Plus)
APP_FOLDER    := MinivMacWiiU
APP_TITLE     := Mini vMac Wii U
APP_SHORT     := Mini vMac
EMU_OBJS      := MINEM68K GLOBGLUE M68KITAB VIAEMDEV IWMEMDEV SCCEMDEV \
                 RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV MOUSEMDV \
                 KBRDEMDV SNDEMDEV PROGMAIN
MODEL_REPO    := $(UPDATE_REPO)
else ifeq ($(MODEL),II)
APP_FOLDER    := MinivMacWiiUII
APP_TITLE     := Mini vMac Wii U II
APP_SHORT     := Mini vMac II
EMU_OBJS      := MINEM68K GLOBGLUE M68KITAB VIAEMDEV VIA2EMDV IWMEMDEV \
                 SCCEMDEV RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV \
                 VIDEMDEV MOUSEMDV ADBEMDEV ASCEMDEV PROGMAIN
MODEL_REPO    := $(UPDATE_REPO_II)
else
$(error MODEL must be one of: $(MODELS))
endif

TARGET        := $(APP_FOLDER)
BUILD         := build/$(MODEL)
CFG           := cfg/$(MODEL)

APP_NAME      := $(APP_TITLE)
APP_SHORTNAME := $(APP_SHORT)
APP_AUTHOR    := quatric
APP_ICON      := icon.png
APP_TV_SPLASH := splash-tv.png
APP_DRC_SPLASH := splash-drc.png
# wut_rules read these when it was included; set the options again here
WUHB_OPTIONS  := --name "$(APP_NAME)" --short-name "$(APP_SHORTNAME)" \
                 --author "$(APP_AUTHOR)" --icon=$(APP_ICON) \
                 --tv-image=$(APP_TV_SPLASH) --drc-image=$(APP_DRC_SPLASH)

OBJS          := $(addprefix $(BUILD)/,$(addsuffix .o,$(EMU_OBJS))) \
                 $(BUILD)/OSGLUWIU.o

INCLUDE       := -I$(CFG) -I$(SRC) -I$(WIIUINPUT)/include \
                 -I$(WIIUUPDATE)/include -I$(WIIOSK)/include \
                 -I$(WUT_ROOT)/include \
                 $(foreach dir,$(PORTLIBS),-I$(dir)/include)

CFLAGS        := -O2 -Wall -Wno-unused-function -Wno-unused-variable \
                 -fno-strict-aliasing -ffunction-sections $(MACHDEP) \
                 -D__WIIU__ -D__WUT__ $(INCLUDE) $(EXTRA_CFLAGS)
GLUEFLAGS     := -DAPP_VERSION=\"$(VERSION)\" -DUPDATE_OWNER=\"$(UPDATE_OWNER)\" \
                 -DUPDATE_REPO=\"$(MODEL_REPO)\" -DAPP_FOLDER=\"$(APP_FOLDER)\" \
                 -DAPP_TITLE="\"$(APP_TITLE)\""
LDFLAGS       := $(MACHDEP) $(RPXSPECS) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS          := -L$(WIIUINPUT)/lib -L$(WIIUUPDATE)/lib -L$(WIIOSK)/lib \
                 $(foreach dir,$(PORTLIBS),-L$(dir)/lib) -L$(WUT_ROOT)/lib \
                 -lwiiuupdate -lwiiuosk -linput-wiiu -lSDL2 -lwut -lm

INPUT_LIB     := $(WIIUINPUT)/lib/libinput-wiiu.a
UPDATE_LIB    := $(WIIUUPDATE)/lib/libwiiuupdate.a
OSK_LIB       := $(WIIOSK)/lib/libwiiuosk.a

all: $(TARGET).wuhb

$(TARGET).wuhb: $(TARGET).rpx $(APP_ICON) $(APP_TV_SPLASH) $(APP_DRC_SPLASH)
	@wuhbtool $< $@ $(WUHB_OPTIONS)
	@echo built ... $@

$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	@elf2rpl $< $@
	@echo built ... $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(INPUT_LIB) $(UPDATE_LIB) $(OSK_LIB)
	@echo linking ... $(notdir $@)
	@$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(INPUT_LIB):
	@$(MAKE) --no-print-directory -C $(WIIUINPUT) $(INPUT_OPTS)

$(UPDATE_LIB):
	@$(MAKE) --no-print-directory -C $(WIIUUPDATE)

$(OSK_LIB):
	@$(MAKE) --no-print-directory -C $(WIIOSK) -f Makefile.wiiu

# Rebuild the glue when VERSION, the update repository or EXTRA_CFLAGS change.
GLUESTAMP := $(BUILD)/glueflags
$(shell mkdir -p $(BUILD); echo '$(CFLAGS) $(GLUEFLAGS)' | cmp -s - $(GLUESTAMP) \
	|| echo '$(CFLAGS) $(GLUEFLAGS)' > $(GLUESTAMP))

$(BUILD)/OSGLUWIU.o: $(SRC)/OSGLUWIU.c $(SRC)/WIIUFONT.h $(wildcard $(CFG)/*.h) $(wildcard $(SRC)/*.h) Makefile $(GLUESTAMP)
	@mkdir -p $(dir $@)
	@echo $(MODEL): $(notdir $<)
	@$(CC) $(CFLAGS) $(GLUEFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.c $(wildcard $(CFG)/*.h)
	@mkdir -p $(dir $@)
	@echo $(MODEL): $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

DISTDIR := dist/wiiu/apps/$(APP_FOLDER)

# libwiiuupdate picks a release's zip with "wiiu" in its name, and
# libwiiupdate (the Wii build) never takes one.
dist-model: $(TARGET).wuhb
	@rm -rf $(DISTDIR) && mkdir -p $(DISTDIR)
	@cp $(TARGET).wuhb $(DISTDIR)/
	@cd dist && rm -f $(APP_FOLDER)-$(VERSION)-wiiu.zip \
		&& zip -qr $(APP_FOLDER)-$(VERSION)-wiiu.zip wiiu/apps/$(APP_FOLDER)
	@echo built dist/$(APP_FOLDER)-$(VERSION)-wiiu.zip

.PHONY: all dist-model
endif
