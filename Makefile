SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

NAME := geolith
JGNAME := $(NAME)

CFLAGS ?= -O2
FLAGS := -std=c11 -Wall -Wextra -Wshadow -Wmissing-declarations -pedantic

DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

INCLUDES := -I$(SRCDIR)
INCLUDES_JG := $(INCLUDES)

LIBS := -lm

DOCS := LICENSE README

override INSTALL_DATA := 0
override INSTALL_SHARED := 0

include $(SOURCEDIR)/mk/jg.mk

# Object dirs
MKDIRS := m68k ymfm z80

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
	geo_z80.c \
	jg.c

include $(SOURCEDIR)/mk/miniz.mk
include $(SOURCEDIR)/mk/speexdsp.mk

INCLUDES += $(CFLAGS_SPEEXDSP) $(CFLAGS_MINIZ)
LIBS += $(LIBS_SPEEXDSP) $(LIBS_MINIZ)

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES_JG) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))

-include config.mk

.PHONY: $(PHONY)

all: $(TARGET)

# Rules
$(OBJDIR)/%.o: $(DEPDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO,$(BUILD_MAIN))
	@$(BUILD_MAIN)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO,$(BUILD_MAIN))
	@$(BUILD_MAIN)

# Shim rules
$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO,$(BUILD_JG))
	@$(BUILD_JG)

$(TARGET_MODULE): $(OBJS)
	@mkdir -p $(NAME)
	$(strip $(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(SHARED))

$(TARGET_STATIC_JG): $(OBJS)
	@mkdir -p $(NAME)
	$(AR) rcs $@ $^

$(TARGET_STATIC_MK): $(TARGET_STATIC_JG)
	@printf '%s\n%s\n%s\n%s\n' 'NAME := $(JGNAME)' 'ASSETS :=' \
		'ICONS := $(ICONS_BASE)' 'LIBS_STATIC := $(strip $(LIBS))' > $@
