SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

NAME := geolith
JGNAME := $(NAME)

SRCDIR := $(SOURCEDIR)/src

INCLUDES = -I$(SRCDIR) -I$(DEPDIR)/dr
INCLUDES_JG = -I$(SRCDIR)

LINKER = $(CC)

LIBS = -lm
LIBS_STATIC =
LIBS_REQUIRES := miniz speexdsp

DOCS := ChangeLog LICENSE README

ENABLE_CHDR ?= 0

override INSTALL_DATA := 0
override INSTALL_EXAMPLE := 0
override INSTALL_SHARED := 0

include $(SOURCEDIR)/version.h
include $(SOURCEDIR)/mk/jg.mk

INCLUDES += $(CFLAGS_SPEEXDSP) $(CFLAGS_MINIZ)
LIBS += $(LIBS_SPEEXDSP) $(LIBS_MINIZ)

EXT := c
FLAGS := -std=c11 $(WARNINGS_DEF_C)
DEFS :=

CSRCS := src/m68k/m68kcpu.c \
	src/m68k/m68kops.c \
	src/ymfm/ymfm_adpcm.c \
	src/ymfm/ymfm_opn.c \
	src/ymfm/ymfm_ssg.c \
	src/z80/z80.c \
	src/geo.c \
	src/geo_cd.c \
	src/geo_cue.c \
	src/geo_disc.c \
	src/geo_lc8951.c \
	src/geo_lspc.c \
	src/geo_m68k.c \
	src/geo_memcard.c \
	src/geo_mixer.c \
	src/geo_neo.c \
	src/geo_rtc.c \
	src/geo_serial.c \
	src/geo_vfs.c \
	src/geo_ymfm.c \
	src/geo_z80.c

JGSRCS := jg.c

ifneq ($(ENABLE_CHDR), 0)
	CSRCS += src/geo_chd.c
	DEFS += -DHAVE_CHDR
	INCLUDES += $(CFLAGS_CHDR)
	LIBS += $(LIBS_CHDR)
endif

override OBJS += $(OBJS_MINIZ) $(OBJS_SPEEXDSP)

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(DEFS) $(INCLUDES_JG) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(DEFS) $(INCLUDES))

.PHONY: $(PHONY)

all: $(TARGET)

include $(SOURCEDIR)/mk/rules.mk
