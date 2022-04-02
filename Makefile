SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

CC ?= cc
CFLAGS ?= -O2
FLAGS := -std=c11 -Wall -Wextra -Wshadow -pedantic

CXX ?= c++
CXXFLAGS ?= -O2 -fPIC -std=c++14

DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

PKG_CONFIG ?= pkg-config
CFLAGS_JG := $(shell $(PKG_CONFIG) --cflags jg)

LIBS_ZLIB := $(shell $(PKG_CONFIG) --libs zlib)

LIBS := $(LIBS_ZLIB) -lstdc++

INCLUDES := -I$(SRCDIR) -I$(SRCDIR)/m68k -I$(SRCDIR)/ymfm -I$(SRCDIR)/z80
PIC := -fPIC
SHARED := $(PIC)

INCLUDES += $(CFLAGS_SPEEXDSP)

NAME := gngeo
PREFIX ?= /usr/local
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
DOCDIR ?= $(DATAROOTDIR)/doc/$(NAME)
LIBDIR ?= $(PREFIX)/lib

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
	ymfm/ymfm_ssg.c \
	z80/z80.c \
	emu.c \
	gngeo_m68k.c \
	gngeo_mixer.c \
	gngeo_z80.c \
	mame_layer.c \
	memory.c \
	neoboot.c \
	neocrypt.c \
	pd4990a.c \
	resfile.c \
	roms.c \
	unzip.c \
	video.c \
	jg.c

CXXSRCS := ymfm/ymfm_opn.cpp \
	ymfm/ymfm_shim.cpp

ifneq ($(USE_VENDORED_SPEEXDSP), 0)
	Q_SPEEXDSP :=
	CFLAGS_SPEEXDSP := -I$(DEPDIR)
	LIBS_SPEEXDSP :=
	CSRCS += speex/resample.c
else
	Q_SPEEXDSP := @
	CFLAGS_SPEEXDSP := $(shell $(PKG_CONFIG) --cflags speexdsp)
	LIBS_SPEEXDSP := $(shell $(PKG_CONFIG) --libs speexdsp)
endif

INCLUDES += $(CFLAGS_SPEEXDSP)
LIBS += $(LIBS_SPEEXDSP)

# Object dirs
MKDIRS := m68k speex ymfm z80

OBJDIR := objs

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o)) \
	$(patsubst %,$(OBJDIR)/%,$(CXXSRCS:.cpp=.o))

# Compiler command
COMPILE = $(strip $(1) $(CPPFLAGS) $(PIC) $(2) -c $< -o $@)
COMPILE_C = $(call COMPILE, $(CC) $(CFLAGS) $(CFLAGS_JG), $(1))
COMPILE_CXX = $(call COMPILE, $(CXX) $(CXXFLAGS), $(1))

# Info command
COMPILE_INFO = $(info $(subst $(SOURCEDIR)/,,$(1)))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))
BUILD_YMFM = $(call COMPILE_CXX, $(INCLUDES))

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

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_YMFM))
	@$(BUILD_YMFM)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(OBJDIR)/.tag
	$(call COMPILE_INFO, $(BUILD_JG))
	@$(BUILD_JG)

$(OBJDIR)/.tag:
	@mkdir -p -- $(patsubst %,$(OBJDIR)/%,$(MKDIRS))
	@touch $@

$(NAME)/$(TARGET): $(OBJS)
	@mkdir -p $(NAME)
	$(CC) $^ $(LDFLAGS) $(LIBS) $(SHARED) -o $@
	@cp $(SOURCEDIR)/$(NAME)_data.zip $(NAME)/

clean:
	rm -rf $(OBJDIR)/ $(NAME)/

install: all
	@mkdir -p $(DESTDIR)$(DATADIR)/jollygood/$(NAME)
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@mkdir -p $(DESTDIR)$(LIBDIR)/jollygood
	cp $(NAME)/$(TARGET) $(DESTDIR)$(LIBDIR)/jollygood/
	cp $(NAME)/$(NAME)_data.zip $(DESTDIR)$(DATADIR)/jollygood/$(NAME)/
	cp $(SOURCEDIR)/COPYING $(DESTDIR)$(DOCDIR)
	cp $(SOURCEDIR)/README $(DESTDIR)$(DOCDIR)
	$(Q_SPEEXDSP)if test $(USE_VENDORED_SPEEXDSP) != 0; then \
		cp $(DEPDIR)/speex/COPYING \
			$(DESTDIR)$(DOCDIR)/COPYING-speexdsp; \
	fi

install-strip: install
	strip $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)

uninstall:
	rm -rf $(DESTDIR)$(DATADIR)/jollygood/$(NAME)
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)
