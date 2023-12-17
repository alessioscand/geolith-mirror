SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

AR ?= ar
CC ?= cc
CFLAGS ?= -O2
FLAGS := -std=c11 -Wall -Wextra -Wshadow -Wmissing-declarations -pedantic

DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

PKG_CONFIG ?= pkg-config
CFLAGS_JG := $(shell $(PKG_CONFIG) --cflags jg)

INCLUDES := -I$(SRCDIR)
LIBS := -lm
PIC := -fPIC
SHARED := $(PIC)

NAME := geolith
PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
LIBDIR ?= $(EXEC_PREFIX)/lib
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
DOCDIR ?= $(DATAROOTDIR)/doc/$(NAME)

LIBPATH := $(LIBDIR)/jollygood

DISABLE_MODULE ?= 0
ENABLE_STATIC_JG ?= 0
USE_EXTERNAL_MINIZ ?= 0
USE_VENDORED_SPEEXDSP ?= 0

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
	LIBRARY := $(NAME).dylib
else ifeq ($(OS), Windows_NT)
	LIBRARY := $(NAME).dll
else
	LIBRARY := $(NAME).so
endif

ifeq ($(UNAME), Darwin)
	SHARED += -dynamiclib -Wl,-undefined,error
else
	SHARED += -shared -Wl,--no-undefined
endif

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

ifeq ($(USE_EXTERNAL_MINIZ), 0)
	CFLAGS_MINIZ := -I$(DEPDIR)/miniz
	LIBS_MINIZ :=
	CSRCS += deps/miniz/miniz.c
else
	CFLAGS_MINIZ := $(shell $(PKG_CONFIG) --cflags miniz)
	LIBS_MINIZ := $(shell $(PKG_CONFIG) --libs miniz)
endif

ifneq ($(USE_VENDORED_SPEEXDSP), 0)
	CFLAGS_SPEEXDSP := -I$(DEPDIR)
	LIBS_SPEEXDSP :=
	CSRCS += deps/speex/resample.c
else
	CFLAGS_SPEEXDSP := $(shell $(PKG_CONFIG) --cflags speexdsp)
	LIBS_SPEEXDSP := $(shell $(PKG_CONFIG) --libs speexdsp)
endif

INCLUDES += $(CFLAGS_SPEEXDSP) $(CFLAGS_MINIZ)
LIBS += $(LIBS_SPEEXDSP) $(LIBS_MINIZ)

# Desktop File
DESKTOP := $(NAME).desktop

DESKTOP_TARGET := $(NAME)/$(DESKTOP)

# Icons
ICONS := $(wildcard $(SOURCEDIR)/icons/*.png) $(SOURCEDIR)/icons/$(NAME).svg

ICONS_BASE := $(notdir $(ICONS))
ICONS_TARGET := $(ICONS_BASE:%=$(NAME)/icons/%)

# Object dirs
MKDIRS := deps/miniz deps/speex m68k ymfm z80

OBJDIR := objs

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o))

# Library targets
TARGET :=
TARGET_MODULE := $(NAME)/$(LIBRARY)
TARGET_STATIC_JG := $(NAME)/lib$(NAME)-jg.a

ifeq ($(DISABLE_MODULE), 0)
	TARGET += $(TARGET_MODULE)
endif

ifneq ($(ENABLE_STATIC_JG), 0)
	TARGET += $(DESKTOP_TARGET) $(ICONS_TARGET) $(NAME)/jg-static.mk
endif

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

all: $(TARGET)

# Rules
$(OBJDIR)/%.o: $(DEPDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_MAIN))
	@$(BUILD_MAIN)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_MAIN))
	@$(BUILD_MAIN)

# Shim rules
$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_JG))
	@$(BUILD_JG)

$(OBJDIR)/.tag:
	@mkdir -p -- $(patsubst %,$(OBJDIR)/%,$(MKDIRS))
	@touch $@

$(TARGET_MODULE): $(OBJS)
	@mkdir -p $(NAME)
	$(strip $(CC) -o $@ $^ $(LDFLAGS) $(LIBS) $(SHARED))

$(TARGET_STATIC_JG): $(OBJS)
	@mkdir -p $(NAME)
	$(AR) rcs $@ $^

$(DESKTOP_TARGET): $(SOURCEDIR)/$(DESKTOP)
	@mkdir -p $(NAME)
	@cp $< $@

$(ICONS_TARGET): $(ICONS)
	@mkdir -p $(NAME)/icons
	@cp $(subst $(NAME)/icons,$(SOURCEDIR)/icons,$@) $(NAME)/icons/

$(NAME)/jg-static.mk: $(TARGET_STATIC_JG)
	@printf '%s\n%s\n%s\n%s\n' 'NAME := $(NAME)' 'ASSETS :=' \
		'ICONS := $(ICONS_BASE)' 'LIBS_STATIC := $(strip $(LIBS))' > $@

clean:
	rm -rf $(OBJDIR) $(NAME)

ifeq ($(DISABLE_MODULE), 0)
install: all
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@mkdir -p $(DESTDIR)$(LIBPATH)
	cp $(TARGET_MODULE) $(DESTDIR)$(LIBPATH)/
	cp $(SOURCEDIR)/LICENSE $(DESTDIR)$(DOCDIR)
	cp $(SOURCEDIR)/README $(DESTDIR)$(DOCDIR)
ifeq ($(USE_EXTERNAL_MINIZ), 0)
	cp $(DEPDIR)/miniz/LICENSE $(DESTDIR)$(DOCDIR)/LICENSE-miniz
endif
ifneq ($(USE_VENDORED_SPEEXDSP), 0)
	cp $(DEPDIR)/speex/COPYING $(DESTDIR)$(DOCDIR)/COPYING-speexdsp
endif

install-strip: install
	strip $(DESTDIR)$(LIBPATH)/$(LIBRARY)
else
install: all
	@echo 'Nothing to install'

install-strip: install
endif

uninstall:
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(LIBPATH)/$(LIBRARY)
