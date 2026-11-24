#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

DEVKITPRO       ?= /opt/devkitpro
CTRULIB         := $(DEVKITPRO)/libctru
PORTLIBS        := $(DEVKITPRO)/portlibs/3ds	
#---------------------------------------------------------------------------------
# Release Targets:
#   make          -> builds PkBank3DS.3dsx + PkBank3DS.smdh (Homebrew Launcher)
#   make cia      -> builds PkBank3DS.cia (Home Menu install via FBI)
#   make release  -> builds release/PkBank3DS.3dsx, release/PkBank3DS.cia, SHA256SUMS.txt
#---------------------------------------------------------------------------------
TARGET          := PkBank3DS
BUILD           := build
SOURCES         := source
INCLUDES        := include
DATA            := data
ROMFS           := romfs
META            := meta
RELEASE_DIR     := release

APP_TITLE       := PkBank 3DS
APP_DESCRIPTION := Strict Legal Gen 1-7 Save & Bank Manager
APP_AUTHOR      := PkBank
APP_PRODUCT_CODE:= CTR-P-PKBK
APP_UNIQUE_ID   := 0xF8A42
APP_ICON        := $(CURDIR)/$(META)/icon.png
APP_BANNER      := $(CURDIR)/$(META)/banner.png
APP_AUDIO       := $(CURDIR)/$(META)/audio.wav
APP_RSF         := $(CURDIR)/$(META)/PkBank3DS.rsf

ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS          := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ $(INCLUDE)
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS         := -g $(ARCH)
LDFLAGS         := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
export LD       := $(CXX)

LIBS            := -lcitro2d -lcitro3d -lctru -lm -lz
LIBDIRS         := $(CTRULIB) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES        := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
export OFILES   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all assets $(BUILD) cia release clean

all: assets $(BUILD)

assets:
	@mkdir -p $(ROMFS) $(META)
	@if [ ! -f "$(APP_ICON)" ] || [ ! -f "$(APP_BANNER)" ] || [ ! -f "$(APP_AUDIO)" ]; then \
		python3 scripts/gen_assets.py; \
	fi

$(BUILD): assets
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

cia: all
	@echo "Packaging $(TARGET).cia (Home Menu / FBI)..."
	@bannertool makebanner -i "$(APP_BANNER)" -a "$(APP_AUDIO)" -o $(BUILD)/banner.bin
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i "$(APP_ICON)" -o $(BUILD)/icon.icn
	@makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf "$(APP_RSF)" \
		-banner $(BUILD)/banner.bin -icon $(BUILD)/icon.icn -exefslogo -target t -DAPP_ROMFS=$(ROMFS)
	@echo "Built $(TARGET).cia successfully."

release: cia
	@mkdir -p $(RELEASE_DIR)
	@cp $(TARGET).3dsx $(TARGET).cia $(RELEASE_DIR)/
	@cp $(BUILD)/icon.icn $(RELEASE_DIR)/$(TARGET).smdh
	@cd $(RELEASE_DIR) && sha256sum $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia > SHA256SUMS.txt
	@echo "Final release artifacts ready in $(RELEASE_DIR)/"

clean:
	@rm -rf $(BUILD) $(RELEASE_DIR) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)

$(OUTPUT).elf : $(OFILES)

-include $(DEPENDS)

endif
