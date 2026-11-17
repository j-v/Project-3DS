#---------------------------------------------------------------------------------
# The build runs inside the devkitPro container, which is what build.sh does.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#---------------------------------------------------------------------------------
TARGET   := project-3ds
BUILD    := build
SOURCES  := src/core src/platform
INCLUDES := src src/core src/platform
DATA     := data
GRAPHICS := gfx
FONTS    := fonts
GFXBUILD := $(BUILD)

APP_TITLE       := Project 3DS: MegaSlide CoolMix+
# build.sh resolves the version from the git tag and passes it in; the fallback
# keeps a plain make usable when APP_VERSION is not set
APP_VERSION     ?= 0.0.0-dev
APP_DESCRIPTION := v$(APP_VERSION)
APP_AUTHOR      := Project 3DS contributors, based on Project DS by Hydr8gon

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections -fdata-sections $(ARCH)

# The clock self test prints its numbers with the printf of newlib, which
# needs the floating point support to be told apart explicitly
CFLAGS += -u _printf_float

# The diagnostic build shows which line of text is being drawn when the game
# stops, for finding out where it crashes
ifdef DIAG
CFLAGS += -DP3DS_DIAG
endif
CFLAGS += $(INCLUDE) -D__3DS__ `$(PREFIX)pkg-config vorbisidec --cflags`

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS := -g $(ARCH)

# 3dsx.specs already passes --gc-sections, but it is spelled out here so the
# intent is visible and a different spec file cannot silently change it
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -Wl,--gc-sections

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project (order is important)
#---------------------------------------------------------------------------------
LIBS := -lcitro2d -lcitro3d -lctru -lm `$(PREFIX)pkg-config vorbisidec --libs`

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS := $(PORTLIBS) $(CTRULIB)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

# The icon is found by the tool that builds the metadata, which runs in the
# build directory
export APP_ICON := $(CURDIR)/gfx/icon.png

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
	            $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
	            $(foreach dir,$(FONTS),$(CURDIR)/$(dir)) \
	            $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES       := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES     := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES       := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES  := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES     := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))
FONTFILES    := $(foreach dir,$(FONTS),$(notdir $(wildcard $(dir)/*.ttf)))
BINFILES     := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
    export LD := $(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
    export LD := $(CXX)
#---------------------------------------------------------------------------------
endif

#---------------------------------------------------------------------------------
# The artwork is converted into textures
#---------------------------------------------------------------------------------
export T3XFILES      := $(patsubst %.png, $(BUILD)/%.t3x, $(GFXFILES))
export T3XOBJFILES   := $(GFXFILES:.png=.t3x.o)
export EFONTFILES    := $(patsubst %.ttf, $(BUILD)/%.bcfnt, $(FONTFILES))
export EFONTOBJFILES := $(FONTFILES:.ttf=.bcfnt.o)
#---------------------------------------------------------------------------------

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN := $(addsuffix .o,$(BINFILES)) \
	        $(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
	        $(T3XOBJFILES) $(EFONTOBJFILES)

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
	        $(addsuffix .h,$(subst .,_,$(BINFILES))) \
	        $(GFXFILES:.png=_t3x.h) \
	        $(FONTFILES:.ttf=_bcfnt.h)

export INCLUDE := -I$(CURDIR) $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	        $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	        -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(NO_SMDH)),)
    export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

.PHONY: all clean diag

#---------------------------------------------------------------------------------
all: $(BUILD) $(DEPSDIR) $(T3XFILES) $(EFONTFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# The diagnostic build shows the clock self test and writes a log to the SD
# card. It is built in its own directory so it never replaces the game.
diag:
	@$(MAKE) --no-print-directory TARGET=project-3ds-diag BUILD=build-diag DIAG=1

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) build-diag $(TARGET).3dsx $(TARGET).elf $(TARGET).cia $(OUTPUT).smdh \
	        $(TARGET)-diag.3dsx $(TARGET)-diag.elf $(TARGET)-diag.smdh $(GFXBUILD)

#---------------------------------------------------------------------------------
# The artwork is converted into textures
#---------------------------------------------------------------------------------
$(BUILD)/%.t3x    :    %.png
	@echo $(notdir $<)
	@tex3ds -f rgba -z auto -d $(DEPSDIR)/$*.d -o $@ $<

$(BUILD)/%.bcfnt    :    %.ttf
	@echo $(notdir $<)
	@mkbcfnt -s 16 -w fonts/whitelist.txt -o $@ $<

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx    :    $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf    :    $(OFILES)

#---------------------------------------------------------------------------------
# The SMDH carries APP_VERSION, but make cannot tell that a variable changed.
# The stamp is rewritten only when the version does, which makes a new tag or a
# dirty tree rebuild the metadata without relinking on every build.
#---------------------------------------------------------------------------------
$(DEPSDIR)/.app-version: FORCE
	@mkdir -p $(DEPSDIR)
	@printf '%s\n' '$(APP_VERSION)' | cmp -s - $@ || printf '%s\n' '$(APP_VERSION)' > $@

$(OUTPUT).smdh    :    $(DEPSDIR)/.app-version

.PHONY: FORCE
FORCE:

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o    %_bin.h :    %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS    :    %.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o    %_t3x.h :    %.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(bin2o)

#---------------------------------------------------------------------------------
%.bcfnt.o    %_bcfnt.h :    %.bcfnt
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
