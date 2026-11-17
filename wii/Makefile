#---------------------------------------------------------------------------------
# Mini vMac for the Nintendo Wii
#
#   make                  both models (Macintosh Plus and Macintosh II)
#   make MODEL=Plus       Macintosh Plus, 512x342 black and white (vMac.ROM)
#   make MODEL=II         Macintosh II, 640x480 256 colours (MacII.ROM)
#   make dist             Homebrew Channel folders and zip in dist/
#
# Needs devkitPPC and libogc. libwiiupdate and libwiiinput are git
# submodules: git submodule update --init
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

VERSION       ?= 1.0.0
UPDATE_OWNER  ?= quatric
# libwiiupdate installs the first apps/<Name>/ folder of a release zip, so
# each model gets its own zip and checks its own repository's releases.
UPDATE_REPO   ?= minivmac-wii
UPDATE_REPO_II ?= $(UPDATE_REPO)-ii

MODELS        := Plus II
MODEL         ?=

SRC           := ../src
WIIINPUT      := libwiiinput
WIIUPDATE     := libwiiupdate

ifeq ($(MODEL),)
#---------------------------------------------------------------------------------
# top level: build every model
#---------------------------------------------------------------------------------
all: $(MODELS)

$(MODELS):
	@$(MAKE) --no-print-directory MODEL=$@

dist: $(MODELS)
	@for m in $(MODELS); do $(MAKE) --no-print-directory MODEL=$$m dist-model || exit 1; done

clean:
	@rm -rf build dist *.elf *.dol *.map
	@$(MAKE) --no-print-directory -C $(WIIUPDATE) clean

.PHONY: all dist clean $(MODELS)

else
#---------------------------------------------------------------------------------
# one model
#---------------------------------------------------------------------------------
ifeq ($(MODEL),Plus)
APP_FOLDER    := MinivMacWii
APP_TITLE     := Mini vMac Wii
EMU_OBJS      := MINEM68K GLOBGLUE M68KITAB VIAEMDEV IWMEMDEV SCCEMDEV \
                 RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV MOUSEMDV \
                 KBRDEMDV SNDEMDEV PROGMAIN
ROMNAME       := vMac.ROM
MACNAME       := Macintosh Plus
MODEL_REPO    := $(UPDATE_REPO)
else ifeq ($(MODEL),II)
APP_FOLDER    := MinivMacWiiII
APP_TITLE     := Mini vMac Wii II
EMU_OBJS      := MINEM68K GLOBGLUE M68KITAB VIAEMDEV VIA2EMDV IWMEMDEV \
                 SCCEMDEV RTCEMDEV ROMEMDEV SCSIEMDV SONYEMDV SCRNEMDV \
                 VIDEMDEV MOUSEMDV ADBEMDEV ASCEMDEV PROGMAIN
ROMNAME       := MacII.ROM
MACNAME       := Macintosh II
MODEL_REPO    := $(UPDATE_REPO_II)
else
$(error MODEL must be one of: $(MODELS))
endif

TARGET        := $(APP_FOLDER)
BUILD         := build/$(MODEL)
CFG           := cfg/$(MODEL)

INPUT_SRCS    := $(wildcard $(WIIINPUT)/source/*.c)
INPUT_OBJS    := $(patsubst $(WIIINPUT)/source/%.c,$(BUILD)/wiiinput/%.o,$(INPUT_SRCS))
OBJS          := $(addprefix $(BUILD)/,$(addsuffix .o,$(EMU_OBJS))) \
                 $(BUILD)/OSGLUWII.o $(INPUT_OBJS)

INCLUDE       := -I$(CFG) -I$(SRC) -I$(WIIINPUT)/include -I$(WIIUPDATE)/include \
                 -I$(LIBOGC_INC) $(foreach dir,$(PORTLIBS),-I$(dir)/include)

CFLAGS        := -O2 -Wall -Wno-unused-function -Wno-unused-variable \
                 -fno-strict-aliasing $(MACHDEP) $(INCLUDE) $(EXTRA_CFLAGS)
GLUEFLAGS     := -DAPP_VERSION=\"$(VERSION)\" -DUPDATE_OWNER=\"$(UPDATE_OWNER)\" \
                 -DUPDATE_REPO=\"$(MODEL_REPO)\" -DAPP_FOLDER=\"$(APP_FOLDER)\" \
                 -DAPP_TITLE="\"$(APP_TITLE)\""
LDFLAGS       := $(MACHDEP) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS          := -L$(WIIUPDATE)/lib -L$(LIBOGC_LIB) \
                 $(foreach dir,$(PORTLIBS),-L$(dir)/lib) \
                 -lwiiupdate -lwiikeyboard -lasnd -lfat -lwiiuse -lbte -logc -lm

all: $(TARGET).dol

$(TARGET).dol: $(BUILD)/$(TARGET).elf
	@echo output ... $@
	@elf2dol $< $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(WIIUPDATE)/lib/libwiiupdate.a
	@echo linking ... $(notdir $@)
	@$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(WIIUPDATE)/lib/libwiiupdate.a:
	@$(MAKE) --no-print-directory -C $(WIIUPDATE)

# Rebuild the glue when VERSION, the update repository or EXTRA_CFLAGS change.
GLUESTAMP := $(BUILD)/glueflags
$(shell mkdir -p $(BUILD); echo '$(CFLAGS) $(GLUEFLAGS)' | cmp -s - $(GLUESTAMP) \
	|| echo '$(CFLAGS) $(GLUEFLAGS)' > $(GLUESTAMP))

$(BUILD)/OSGLUWII.o: $(SRC)/OSGLUWII.c $(wildcard $(CFG)/*.h) $(wildcard $(SRC)/*.h) Makefile $(GLUESTAMP)
	@mkdir -p $(dir $@)
	@echo $(MODEL): $(notdir $<)
	@$(CC) $(CFLAGS) $(GLUEFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.c $(wildcard $(CFG)/*.h)
	@mkdir -p $(dir $@)
	@echo $(MODEL): $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/wiiinput/%.o: $(WIIINPUT)/source/%.c
	@mkdir -p $(dir $@)
	@echo $(MODEL): $(notdir $<)
	@$(CC) $(CFLAGS) -w -c $< -o $@

DISTDIR := dist/apps/$(APP_FOLDER)

dist-model: $(TARGET).dol
	@mkdir -p $(DISTDIR)
	@cp $(TARGET).dol $(DISTDIR)/boot.dol
	@cp icon.png $(DISTDIR)/icon.png
	@sed -e 's/@NAME@/$(APP_TITLE)/' -e 's/@VERSION@/$(VERSION)/' \
	     -e 's/@MAC@/$(MACNAME)/' -e 's/@ROM@/$(ROMNAME)/' \
	     -e 's/@FOLDER@/$(APP_FOLDER)/' \
	     -e "s/@DATE@/$$(date +%Y%m%d%H%M%S)/" meta.xml.in > $(DISTDIR)/meta.xml
	@cd dist && zip -qFSr $(APP_FOLDER)-$(VERSION)-hbc.zip apps/$(APP_FOLDER)
	@echo built dist/$(APP_FOLDER)-$(VERSION)-hbc.zip

.PHONY: all dist-model
endif
