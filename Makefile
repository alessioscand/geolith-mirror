SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

CC ?= cc
CFLAGS ?= -O2
FLAGS := -Wno-incompatible-pointer-types #-Wall -Wextra -Wshadow -Wmissing-prototypes -pedantic
DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

PKGCONF ?= pkg-config
CFLAGS_JG := $(shell $(PKGCONF) --cflags jg)

LIBS_ZLIB := $(shell $(PKGCONF) --libs zlib)

LIBS := $(LIBS_ZLIB)

INCLUDES := -I$(SRCDIR) -I$(SRCDIR)/generator68k -I$(SRCDIR)/mamez80
PIC := -fPIC
SHARED := $(PIC)

NAME := gngeo
PREFIX ?= /usr/local
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
DOCDIR ?= $(DATAROOTDIR)/doc/$(NAME)
LIBDIR ?= $(PREFIX)/lib

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

CSRCS := generator68k/cpu68k.c \
	generator68k/def68k.c \
	generator68k/diss68k.c \
	generator68k/reg68k.c \
	generator68k/tab68k2.c \
	generator68k/cpu68k-0.c \
	generator68k/cpu68k-1.c \
	generator68k/cpu68k-2.c \
	generator68k/cpu68k-3.c \
	generator68k/cpu68k-4.c \
	generator68k/cpu68k-5.c \
	generator68k/cpu68k-6.c \
	generator68k/cpu68k-7.c \
	generator68k/cpu68k-8.c \
	generator68k/cpu68k-9.c \
	generator68k/cpu68k-a.c \
	generator68k/cpu68k-b.c \
	generator68k/cpu68k-c.c \
	generator68k/cpu68k-d.c \
	generator68k/cpu68k-e.c \
	generator68k/cpu68k-f.c \
	mamez80/z80.c \
	ym2610/ym2610.c \
	ym2610/2610intf.c \
	emu.c \
	generator68k_interf.c \
	mamez80_interf.c \
	mame_layer.c \
	memory.c \
	neoboot.c \
	neocrypt.c \
	pd4990a.c \
	resfile.c \
	roms.c \
	timer.c \
	unzip.c \
	video.c \
	jg.c

# Object dirs
MKDIRS := generator68k mamez80 ym2610

OBJDIR := objs

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o))

# Compiler command
COMPILE = $(strip $(1) $(CPPFLAGS) $(PIC) $(2) -c $< -o $@)
COMPILE_C = $(call COMPILE, $(CC) $(CFLAGS) $(CFLAGS_JG), $(1))

# Info command
COMPILE_INFO = $(info $(subst $(SOURCEDIR)/,,$(1)))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))

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

install-strip: install
	strip $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)

uninstall:
	rm -rf $(DESTDIR)$(DATADIR)/jollygood/$(NAME)
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(DESTDIR)$(LIBDIR)/jollygood/$(TARGET)
