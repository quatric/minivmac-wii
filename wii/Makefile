#---------------------------------------------------------------------------------
# Mini vMac for the Nintendo Wii: one app with every Mac in models.h
#
#   make                  MinivMacWii.dol (the launcher and every model)
#   make dist             Homebrew Channel folder and zip in dist/
#   make cfg              regenerate cfg/<model>/ (mkcfg.sh)
#
# Each model is the whole of Mini vMac (emulator and Wii glue) compiled with
# its own configuration, partially linked to build/<model>/core.o, and its
# global symbols renamed mv_<model>_... (main becomes mv_<model>_main).
# core.ld gathers the model's data and bss so the launcher can reset them.
#
# Needs devkitPPC and libogc. libwiiupdate, libwiiinput and libwiiosk are
# git submodules: git submodule update --init
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

VERSION       ?= 1.0.0
UPDATE_OWNER  ?= quatric
UPDATE_REPO   ?= minivmac-wii

APP_FOLDER    := MinivMacWii
APP_TITLE     := Mini vMac Wii
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
WIIINPUT      := libwiiinput
WIIUPDATE     := libwiiupdate
WIIOSK        := libwiiosk
CROSS         := $(DEVKITPPC)/bin/powerpc-eabi-

INCLUDE       := -I. -I$(SRC) -I$(WIIINPUT)/include -I$(WIIUPDATE)/include \
                 -I$(WIIOSK)/include \
                 -I$(LIBOGC_INC) $(foreach dir,$(PORTLIBS),-I$(dir)/include)
CFLAGS        := -O2 -Wall -Wno-unused-function -Wno-unused-variable \
                 -fno-strict-aliasing $(MACHDEP) $(EXTRA_CFLAGS)
APPFLAGS      := -DAPP_VERSION=\"$(VERSION)\" -DUPDATE_OWNER=\"$(UPDATE_OWNER)\" \
                 -DUPDATE_REPO=\"$(UPDATE_REPO)\" -DAPP_FOLDER=\"$(APP_FOLDER)\" \
                 -DAPP_TITLE="\"$(APP_TITLE)\""
LDFLAGS       := $(MACHDEP) -Wl,-Map,build/$(TARGET).map
LIBS          := -L$(WIIUPDATE)/lib -L$(WIIOSK)/lib -L$(LIBOGC_LIB) \
                 $(foreach dir,$(PORTLIBS),-L$(dir)/lib) \
                 -lwiiupdate -lwiiosk -lwiikeyboard -lasnd -lfat -lwiiuse -lbte -logc -lm

CORES         := $(foreach m,$(MODELS),build/$(m)/core.o)
INPUT_SRCS    := $(wildcard $(WIIINPUT)/source/*.c)
INPUT_OBJS    := $(patsubst $(WIIINPUT)/source/%.c,build/wiiinput/%.o,$(INPUT_SRCS))
LAUNCHER_OBJ  := build/launcher.o

all: $(TARGET).dol

$(TARGET).dol: build/$(TARGET).elf
	@echo output ... $@
	@elf2dol $< $@

build/$(TARGET).elf: $(LAUNCHER_OBJ) $(CORES) $(INPUT_OBJS) \
		$(WIIUPDATE)/lib/libwiiupdate.a $(WIIOSK)/lib/libwiiosk.a
	@echo linking ... $(notdir $@)
	@$(CC) $(LAUNCHER_OBJ) $(CORES) $(INPUT_OBJS) $(LDFLAGS) $(LIBS) -o $@

$(WIIUPDATE)/lib/libwiiupdate.a:
	@$(MAKE) --no-print-directory -C $(WIIUPDATE)

$(WIIOSK)/lib/libwiiosk.a:
	@$(MAKE) --no-print-directory -C $(WIIOSK)

libs: $(WIIUPDATE)/lib/libwiiupdate.a $(WIIOSK)/lib/libwiiosk.a

# Rebuild what depends on VERSION, the update repository or EXTRA_CFLAGS
# when they change.
FLAGSTAMP := build/flags
$(shell mkdir -p build; echo '$(CFLAGS) $(APPFLAGS)' | cmp -s - $(FLAGSTAMP) \
	|| echo '$(CFLAGS) $(APPFLAGS)' > $(FLAGSTAMP))

$(LAUNCHER_OBJ): launcher.c launcher.h models.h Makefile $(FLAGSTAMP)
	@mkdir -p $(dir $@)
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) $(APPFLAGS) $(INCLUDE) -c $< -o $@

build/wiiinput/%.o: $(WIIINPUT)/source/%.c
	@mkdir -p $(dir $@)
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) $(INCLUDE) -w -c $< -o $@

#---------------------------------------------------------------------------------
# one model: build/<model>/core.o
#
# -G0 keeps every variable out of the small data sections (which are reached
# through r13 and could not be moved), -fno-common puts them all in .bss.
#---------------------------------------------------------------------------------
define model_rules
OBJS_$(1)  := $$(addprefix build/$(1)/,$$(addsuffix .o,$$(EMU_OBJS_$(1)) OSGLUWII))
CFLAGS_$(1) := $$(CFLAGS) -Wno-maybe-uninitialized -G0 -fno-common -Icfg/$(1) $$(INCLUDE) -DMV_LAUNCHER \
               -DMODEL_DIR=\"$(1)\" -DMAC_NAME="\"$$(MACNAME_$(1))\""

build/$(1)/OSGLUWII.o: $$(SRC)/OSGLUWII.c launcher.h $$(wildcard cfg/$(1)/*.h) \
		$$(wildcard $$(SRC)/*.h) Makefile $$(FLAGSTAMP)
	@mkdir -p $$(dir $$@)
	@echo $(1): $$(notdir $$<)
	@$$(CC) $$(CFLAGS_$(1)) $$(APPFLAGS) -c $$< -o $$@

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
DISTDIR := dist/apps/$(APP_FOLDER)

dist: $(TARGET).dol
	@rm -rf $(DISTDIR)
	@mkdir -p $(DISTDIR)
	@cp $(TARGET).dol $(DISTDIR)/boot.dol
	@cp icon.png $(DISTDIR)/icon.png
	@for m in $(MODELS); do mkdir -p $(DISTDIR)/$$m; done
	@sed -e 's/@NAME@/$(APP_TITLE)/' -e 's/@VERSION@/$(VERSION)/' \
	     -e 's/@FOLDER@/$(APP_FOLDER)/' \
	     -e "s/@DATE@/$$(date +%Y%m%d%H%M%S)/" meta.xml.in > $(DISTDIR)/meta.xml
	@rm -f dist/$(APP_FOLDER)-$(VERSION)-hbc.zip
	@cd dist && zip -qr $(APP_FOLDER)-$(VERSION)-hbc.zip apps/$(APP_FOLDER)
	@echo built dist/$(APP_FOLDER)-$(VERSION)-hbc.zip

cfg:
	@./mkcfg.sh $(MODELS)

clean:
	@rm -rf build dist *.elf *.dol *.map
	@$(MAKE) --no-print-directory -C $(WIIUPDATE) clean
	@$(MAKE) --no-print-directory -C $(WIIOSK) clean

.PHONY: all libs dist cfg clean
