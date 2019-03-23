LD = ld
CC = cc
PKG_CONFIG = pkg-config
INSTALL = install
CFLAGS = -g -O2 -Wall -Wextra
LDFLAGS =
LIBS =
VLC_PLUGIN_CFLAGS := $(shell $(PKG_CONFIG) --cflags vlc-plugin)
VLC_PLUGIN_LIBS := $(shell $(PKG_CONFIG) --libs vlc-plugin)
VLC_PLUGIN_DIR := $(shell pkg-config vlc-plugin --variable=pluginsdir)
 
plugindir = $(VLC_PLUGIN_DIR)/control
 
override CC += -std=gnu99
override CPPFLAGS += -DPIC -I. -Isrc
override CFLAGS += -fPIC
override LDFLAGS += -Wl,-no-undefined,-z,defs
 
override CPPFLAGS += -DMODULE_STRING=\"TIP\"
override CFLAGS += $(VLC_PLUGIN_CFLAGS)
override LIBS += $(VLC_PLUGIN_LIBS)
 
TARGETS = libtip_plugin.so
 
all: libtip_plugin.so
 
install: all
	mkdir -p -- $(DESTDIR)$(plugindir)
	$(INSTALL) --mode 0644 libtip_plugin.so $(DESTDIR)$(plugindir)

install-strip:
	$(MAKE) install INSTALL="$(INSTALL) -s"

uninstall:
	rm -f $(plugindir)/libtip_plugin.so

clean:
	rm -f -- libtip_plugin.so src/*.o

mostlyclean: clean
 
SOURCES = tip.c
 
$(SOURCES:%.c=src/%.o): %: src/tip.c
 
libtip_plugin.so: $(SOURCES:%.c=src/%.o)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)
 
.PHONY: all install install-strip uninstall clean mostlyclean
