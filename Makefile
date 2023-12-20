SOURCEDIR := $(abspath $(patsubst %/,%,$(dir $(abspath $(lastword \
	$(MAKEFILE_LIST))))))

NAME := geolith
JGNAME := $(NAME)

CFLAGS ?= -O2
FLAGS := -std=c11 -Wall -Wextra -Wshadow -Wmissing-declarations -pedantic

DEPDIR := $(SOURCEDIR)/deps
SRCDIR := $(SOURCEDIR)/src

INCLUDES := -I$(SRCDIR)
LIBS := -lm

USE_EXTERNAL_MINIZ ?= 0
USE_VENDORED_SPEEXDSP ?= 0

override ENABLE_SHARED := 0
override ENABLE_STATIC := 0

include $(SOURCEDIR)/jg.mk

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

# Object dirs
MKDIRS := deps/miniz deps/speex m68k ymfm z80

# List of object files
OBJS := $(patsubst %,$(OBJDIR)/%,$(CSRCS:.c=.o))

# Core commands
BUILD_JG = $(call COMPILE_C, $(FLAGS) $(INCLUDES) $(CFLAGS_JG))
BUILD_MAIN = $(call COMPILE_C, $(FLAGS) $(INCLUDES))

-include config.mk

.PHONY: all clean install install-strip uninstall

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
	@printf '%s\n%s\n%s\n%s\n' 'NAME := $(JGNAME)' 'ASSETS :=' \
		'ICONS := $(ICONS_BASE)' 'LIBS_STATIC := $(strip $(LIBS))' > $@

clean:
	rm -rf $(OBJDIR) $(NAME)

ifneq ($(ENABLE_LIB), 0)
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
