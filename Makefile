SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

NAME := geolith
JGNAME := $(NAME)

CFLAGS ?= -O2

SRCDIR := $(SOURCEDIR)/src

INCLUDES = -I$(SRCDIR)
INCLUDES_JG = $(INCLUDES)

LIBS = -lm

LIBS_REQUIRES := miniz speexdsp

DOCS := LICENSE README

# Object dirs
MKDIRS := m68k ymfm z80

override INSTALL_DATA := 0
override INSTALL_EXAMPLE := 0
override INSTALL_SHARED := 0

include $(SOURCEDIR)/version.h
include $(SOURCEDIR)/mk/jg.mk

INCLUDES += $(CFLAGS_SPEEXDSP) $(CFLAGS_MINIZ)
LIBS += $(LIBS_SPEEXDSP) $(LIBS_MINIZ)

FLAGS := -std=c11 $(WARNINGS_DEF_C)
LINKER := $(CC)

CSRCS := m68k/m68kcpu.c \
	m68k/m68kops.c \
	ymfm/ymfm_adpcm.c \
	ymfm/ymfm_opn.c \
	ymfm/ymfm_ssg.c \
	z80/z80.c \
	geo.c \
	geo_lspc.c \
	geo_m68k.c \
	geo_memcard.c \
	geo_mixer.c \
	geo_neo.c \
	geo_rtc.c \
	geo_serial.c \
	geo_ymfm.c \
	geo_z80.c

JGSRCS := jg.c

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o) $(OBJS_MINIZ) $(OBJS_SPEEXDSP))
OBJS_JG := $(patsubst %,$(OBJDIR)/%,$(JGSRCS:.c=.o))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES_JG) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))

-include config.mk

.PHONY: $(PHONY)

all: $(TARGET)

include $(SOURCEDIR)/mk/rules.mk

# Rules
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO,$(BUILD_MAIN))
	@$(BUILD_MAIN)

# Shim rules
$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO,$(BUILD_JG))
	@$(BUILD_JG)
