SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

CC ?= cc
CFLAGS ?= -O2
FLAGS := -std=c11 -Wall -Wextra -Wshadow -Wmissing-declarations -pedantic

DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

PKG_CONFIG ?= pkg-config
CFLAGS_JG := $(shell $(PKG_CONFIG) --cflags jg)

INCLUDES := -I$(SRCDIR)
PIC := -fPIC
SHARED := $(PIC)

NAME := gngeo
PREFIX ?= /usr/local
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
DOCDIR ?= $(DATAROOTDIR)/doc/$(NAME)
LIBDIR ?= $(PREFIX)/lib

USE_EXTERNAL_MINIZ ?= 0
USE_VENDORED_SPEEXDSP ?= 0

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
	SHARED += -dynamiclib
	TARGET := $(NAME).dylib
else ifeq ($(OS), Windows_NT)
	SHARED += -shared
	TARGET := $(NAME).dll
else
	LIBS += -lm
	SHARED += -shared
	TARGET := $(NAME).so
endif

ifeq ($(UNAME), Linux)
	LIBS += -Wl,--no-undefined
endif

CSRCS := m68k/m68kcpu.c \
	m68k/m68kops.c \
	ymfm/ymfm_adpcm.c \
	ymfm/ymfm_opn.c \
	ymfm/ymfm_shim.c \
	ymfm/ymfm_ssg.c \
	z80/z80.c \
	gngeo.c \
	gngeo_m68k.c \
	gngeo_mixer.c \
	gngeo_neo.c \
	gngeo_z80.c \
	roms.c \
	video.c \
	jg.c

ifeq ($(USE_EXTERNAL_MINIZ), 0)
	Q_MINIZ :=
	CFLAGS_MINIZ := -I$(DEPDIR)/miniz
	LIBS_MINIZ :=
	CSRCS += deps/miniz/miniz.c
else
	Q_MINIZ := @
	CFLAGS_MINIZ := $(shell $(PKG_CONFIG) --cflags miniz)
	LIBS_MINIZ := $(shell $(PKG_CONFIG) --libs miniz)
endif

ifneq ($(USE_VENDORED_SPEEXDSP), 0)
	Q_SPEEXDSP :=
	CFLAGS_SPEEXDSP := -I$(DEPDIR)
	LIBS_SPEEXDSP :=
	CSRCS += deps/speex/resample.c
else
	Q_SPEEXDSP := @
	CFLAGS_SPEEXDSP := $(shell $(PKG_CONFIG) --cflags speexdsp)
	LIBS_SPEEXDSP := $(shell $(PKG_CONFIG) --libs speexdsp)
endif

INCLUDES += $(CFLAGS_SPEEXDSP) $(CFLAGS_MINIZ)
LIBS += $(LIBS_SPEEXDSP) $(LIBS_MINIZ)

# Object dirs
MKDIRS := deps/miniz deps/speex m68k ymfm z80

OBJDIR := objs

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o))

# Compiler command
COMPILE = $(strip $(1) $(CPPFLAGS) $(PIC) $(2) -c $< -o $@)
COMPILE_C = $(call COMPILE, $(CC) $(CFLAGS), $(1))

# Info command
COMPILE_INFO = $(info $(subst $(SOURCEDIR)/,,$(1)))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))

-include config.mak
.PHONY: all clean install install-strip uninstall

all: $(NAME)/$(TARGET)

# Rules
$(OBJDIR)/%.o: $(DEPDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_MAIN))
	@$(BUILD_MAIN)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_MAIN))
	@$(BUILD_MAIN)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_JG))
	@$(BUILD_JG)

$(OBJDIR)/.tag:
	@mkdir -p -- $(patsubst %,$(OBJDIR)/%,$(MKDIRS))
	@touch $@

$(NAME)/$(TARGET): $(OBJS)
	@mkdir -p $(NAME)
	$(CC) $^ $(LDFLAGS) $(LIBS) $(SHARED) -o $@

clean:
	rm -rf $(OBJDIR) $(NAME)

install: all
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@mkdir -p $(DESTDIR)$(LIBDIR)/jollygood
	cp $(NAME)/$(TARGET) $(DESTDIR)$(LIBDIR)/jollygood/
	cp $(SOURCEDIR)/COPYING $(DESTDIR)$(DOCDIR)
	cp $(SOURCEDIR)/README $(DESTDIR)$(DOCDIR)
	$(Q_MINIZ)if test $(USE_EXTERNAL_MINIZ) = 0; then \
		cp $(DEPDIR)/miniz/LICENSE \
			$(DESTDIR)$(DOCDIR)/LICENSE-miniz; \
	fi
	$(Q_SPEEXDSP)if test $(USE_VENDORED_SPEEXDSP) != 0; then \
		cp $(DEPDIR)/speex/COPYING \
			$(DESTDIR)$(DOCDIR)/COPYING-speexdsp; \
	fi

install-strip: install
	strip $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)

uninstall:
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)
