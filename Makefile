LD = ld
CC = cc
PKG_CONFIG = pkg-config
INSTALL = install
CFLAGS = -g -O2 -Wall -Wextra
LDFLAGS =
LIBS =
VLC_PLUGIN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vlc-plugin)
VLC_PLUGIN_LIBS := $(shell $(PKG_CONFIG) --libs vlc-plugin)
VLC_PLUGIN_DIR := $(shell $(PKG_CONFIG) --variable=pluginsdir vlc-plugin)

plugindir := $(VLC_PLUGIN_DIR)/control

override CC += -std=gnu11
override CPPFLAGS += -DPIC -I. -Isrc
override CFLAGS += -fPIC

override CPPFLAGS += -DMODULE_STRING=\"TIP\"
override CFLAGS += $(VLC_PLUGIN_CFLAGS)
override LIBS += $(VLC_PLUGIN_LIBS)


ifeq ($(strip $(OS)),)
  OS=$(shell uname -s)
endif

ifeq ($(OS),Windows_NT)
  SUFFIX := dll
  override LDFLAGS += -Wl,-no-undefined
else ifeq ($(OS),Linux)
  SUFFIX := so
  override LDFLAGS += -Wl,-no-undefined
else ifeq ($(OS),Darwin)
  SUFFIX := dylib
  override LDFLAGS += -Wl
else
  $(error Unknown OS '$(OS)', the plugin can be built on Windows, Linux or macOS)
endif


TARGETS = libtip_plugin.$(SUFFIX)

all: libtip_plugin.$(SUFFIX)

install: all
	mkdir -p -- $(DESTDIR)$(plugindir)
	$(INSTALL) -m 0755 libtip_plugin.$(SUFFIX) $(DESTDIR)$(plugindir)

install-strip:
	$(MAKE) install INSTALL="$(INSTALL) -s"

uninstall:
	rm -f $(plugindir)/libtip_plugin.$(SUFFIX)

clean:
	rm -f -- libtip_plugin.$(SUFFIX) src/*.o

mostlyclean: clean

SOURCES = tip.c

$(SOURCES:%.c=src/%.o): %: src/tip.c

libtip_plugin.$(SUFFIX): $(SOURCES:%.c=src/%.o)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

.PHONY: all install install-strip uninstall clean mostlyclean
