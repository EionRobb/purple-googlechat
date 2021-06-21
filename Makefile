
PIDGIN_TREE_TOP ?= ../pidgin-2.10.11
PIDGIN3_TREE_TOP ?= ../pidgin-main
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
PROTOBUF_C_DIR ?= $(WIN32_DEV_TOP)/protobuf-c-Release-2.6

WIN32_CC ?= $(WIN32_DEV_TOP)/mingw-4.7.2/bin/gcc
MAKENSIS ?= makensis

PROTOC_C ?= protoc-c
PKG_CONFIG ?= pkg-config

CFLAGS	?= -O2 -g -pipe
LDFLAGS ?= 

# Do some nasty OS and purple version detection
ifeq ($(OS),Windows_NT)
  #only defined on 64-bit windows
  PROGFILES32 = ${ProgramFiles(x86)}
  ifndef PROGFILES32
    PROGFILES32 = $(PROGRAMFILES)
  endif
  PLUGIN_TARGET = libgooglechat.dll
  PLUGIN_DEST = "$(PROGFILES32)/Pidgin/plugins"
  PLUGIN_ICONS_DEST = "$(PROGFILES32)/Pidgin/pixmaps/pidgin/protocols"
  MAKENSIS = "$(PROGFILES32)/NSIS/makensis.exe"
else

  UNAME_S := $(shell uname -s)

  #.. There are special flags we need for OSX
  ifeq ($(UNAME_S), Darwin)
    #
    #.. /opt/local/include and subdirs are included here to ensure this compiles
    #   for folks using Macports.  I believe Homebrew uses /usr/local/include
    #   so things should "just work".  You *must* make sure your packages are
    #   all up to date or you will most likely get compilation errors.
    #
    INCLUDES = -I/opt/local/include/protobuf-c  -I/opt/local/include -lz $(OS)

    CC = gcc
  else
    INCLUDES = -I/usr/include/protobuf-c
    CC ?= gcc
  endif

  ifeq ($(shell $(PKG_CONFIG) --exists libprotobuf-c && echo "true"),true)
    PROTOBUF_OPTS := $(shell $(PKG_CONFIG) --cflags --libs libprotobuf-c)
  else
    PROTOBUF_OPTS := -I/usr/include/google -I/usr/include/google/protobuf-c -lprotobuf-c
  endif

  ifeq ($(shell $(PKG_CONFIG) --exists purple-3 2>/dev/null && echo "true"),)
    ifeq ($(shell $(PKG_CONFIG) --exists purple 2>/dev/null && echo "true"),)
      PLUGIN_TARGET = FAILNOPURPLE
      PLUGIN_DEST =
	  PLUGIN_ICONS_DEST =
    else
      PLUGIN_TARGET = libgooglechat.so
      PLUGIN_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple`
	  PLUGIN_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple`/pixmaps/pidgin/protocols
    endif
  else
    PLUGIN_TARGET = libgooglechat3.so
    PLUGIN_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple-3`
	PLUGIN_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple-3`/pixmaps/pidgin/protocols
  endif
endif

WIN32_CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.14/include/json-glib-1.0 -I$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/include -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -fno-strict-aliasing -Wformat
WIN32_LDFLAGS = -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(PROTOBUF_C_DIR)/bin -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lprotobuf-c-1 -g -ggdb -static-libgcc -lz
WIN32_PIDGIN2_CFLAGS = -I$(PIDGIN_TREE_TOP)/libpurple -I$(PIDGIN_TREE_TOP) $(WIN32_CFLAGS)
WIN32_PIDGIN3_CFLAGS = -I$(PIDGIN3_TREE_TOP)/libpurple -I$(PIDGIN3_TREE_TOP) -I$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_CFLAGS)
WIN32_PIDGIN2_LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple $(WIN32_LDFLAGS)
WIN32_PIDGIN3_LDFLAGS = -L$(PIDGIN3_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_LDFLAGS) -lgplugin

C_FILES := googlechat.pb-c.c googlechat_json.c googlechat_pblite.c googlechat_connection.c googlechat_auth.c googlechat_events.c googlechat_conversation.c
PURPLE_COMPAT_FILES := purple2compat/http.c purple2compat/purple-socket.c
PURPLE_C_FILES := libgooglechat.c $(C_FILES)



.PHONY:	all install FAILNOPURPLE clean

all: $(PLUGIN_TARGET)

googlechat.pb-c.c: googlechat.proto
	$(PROTOC_C) --c_out=. googlechat.proto

libgooglechat.so: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(CC) -fPIC $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(PROTOBUF_OPTS) `$(PKG_CONFIG) purple glib-2.0 json-glib-1.0 zlib --libs --cflags` -ldl $(INCLUDES) -Ipurple2compat -g -ggdb

libgooglechat3.so: $(PURPLE_C_FILES)
	$(CC) -fPIC $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) $(PROTOBUF_OPTS) `$(PKG_CONFIG) purple-3 glib-2.0 json-glib-1.0 zlib --libs --cflags` -ldl $(INCLUDES) -g -ggdb

libgooglechat.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

libgooglechat3.dll: $(PURPLE_C_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN3_CFLAGS) $(WIN32_PIDGIN3_LDFLAGS)

install: $(PLUGIN_TARGET) install-icons
	mkdir -p $(PLUGIN_DEST)
	install -p $(PLUGIN_TARGET) $(PLUGIN_DEST)

install-icons: googlechat16.png googlechat22.png googlechat48.png
	mkdir -p $(PLUGIN_ICONS_DEST)/16
	mkdir -p $(PLUGIN_ICONS_DEST)/22
	mkdir -p $(PLUGIN_ICONS_DEST)/48
	install googlechat16.png $(PLUGIN_ICONS_DEST)/16/googlechat.png
	install googlechat22.png $(PLUGIN_ICONS_DEST)/22/googlechat.png
	install googlechat48.png $(PLUGIN_ICONS_DEST)/48/googlechat.png

FAILNOPURPLE:
	echo "You need libpurple development headers installed to be able to compile this plugin"

clean:
	rm -f $(PLUGIN_TARGET) googlechat.pb-c.h googlechat.pb-c.c


installer: purple-googlechat.nsi libgooglechat.dll
	$(MAKENSIS) purple-googlechat.nsi
