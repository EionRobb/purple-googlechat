
PIDGIN_TREE_TOP ?= ../pidgin-2.10.12
PIDGIN3_TREE_TOP ?= ../pidgin-main
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
PROTOBUF_C_DIR ?= $(WIN32_DEV_TOP)/protobuf-c-Release-2.6

WIN32_CC ?= $(WIN32_DEV_TOP)/mingw/bin/gcc

PROTOC_C ?= protoc-c
PKG_CONFIG ?= pkg-config


# Do some nasty OS and purple version detection
ifeq ($(OS),Windows_NT)
  HANGOUTS_TARGET = libhangouts.dll
  HANGOUTS_DEST = "$(PROGRAMFILES)/Pidgin/plugins"
  HANGOUTS_ICONS_DEST = "$(PROGRAMFILES)/Pidgin/pixmaps/pidgin/protocols"
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
      HANGOUTS_TARGET = FAILNOPURPLE
      HANGOUTS_DEST =
	  HANGOUTS_ICONS_DEST =
    else
      HANGOUTS_TARGET = libhangouts.so
      HANGOUTS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple`
	  HANGOUTS_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple`/pixmaps/pidgin/protocols
    endif
  else
    HANGOUTS_TARGET = libhangouts3.so
    HANGOUTS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple-3`
	HANGOUTS_ICONS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=datadir purple-3`/pixmaps/pidgin/protocols
  endif
endif

WIN32_CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.14/include/json-glib-1.0 -I$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/include -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -fno-strict-aliasing -Wformat
WIN32_LDFLAGS = -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(PROTOBUF_C_DIR)/bin -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lprotobuf-c-1 -g -ggdb -static-libgcc -lz
WIN32_PIDGIN2_CFLAGS = -I$(PIDGIN_TREE_TOP)/libpurple -I$(PIDGIN_TREE_TOP) $(WIN32_CFLAGS)
WIN32_PIDGIN3_CFLAGS = -I$(PIDGIN3_TREE_TOP)/libpurple -I$(PIDGIN3_TREE_TOP) -I$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_CFLAGS)
WIN32_PIDGIN2_LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple $(WIN32_LDFLAGS)
WIN32_PIDGIN3_LDFLAGS = -L$(PIDGIN3_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_LDFLAGS) -lgplugin

C_FILES := hangouts.pb-c.c hangout_media.pb-c.c hangouts_json.c hangouts_pblite.c hangouts_connection.c hangouts_auth.c hangouts_events.c hangouts_conversation.c hangouts_media.c
PURPLE_COMPAT_FILES := purple2compat/http.c purple2compat/purple-socket.c
PURPLE_C_FILES := libhangouts.c $(C_FILES)
TEST_C_FILES := hangouts_test.c $(C_FILES)



.PHONY:	all install FAILNOPURPLE clean

all: $(HANGOUTS_TARGET)

hangouts.pb-c.c: hangouts.proto
	$(PROTOC_C) --c_out=. hangouts.proto
	
hangout_media.pb-c.c: hangout_media.proto
	$(PROTOC_C) --c_out=. hangout_media.proto

libhangouts.so: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(CC) -fPIC -shared -o $@ $^ $(PROTOBUF_OPTS) `$(PKG_CONFIG) purple glib-2.0 json-glib-1.0 --libs --cflags`  $(INCLUDES) -Ipurple2compat -g -ggdb

libhangouts3.so: $(PURPLE_C_FILES)
	$(CC) -fPIC -shared -o $@ $^ $(PROTOBUF_OPTS) `$(PKG_CONFIG) purple-3 glib-2.0 json-glib-1.0 --libs --cflags` $(INCLUDES)  -g -ggdb

libhangouts.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

libhangouts3.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN3_CFLAGS) $(WIN32_PIDGIN3_LDFLAGS)

hangouts-test.exe: $(TEST_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -o $@ -DDEBUG $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

install: $(HANGOUTS_TARGET) install-icons
	install -p $(HANGOUTS_TARGET) $(HANGOUTS_DEST)

install-icons: hangouts16.png hangouts22.png hangouts48.png
	install hangouts16.png $(HANGOUTS_ICONS_DEST)/16/hangouts.png
	install hangouts22.png $(HANGOUTS_ICONS_DEST)/22/hangouts.png
	install hangouts48.png $(HANGOUTS_ICONS_DEST)/48/hangouts.png

FAILNOPURPLE:
	echo "You need libpurple development headers installed to be able to compile this plugin"

clean:
	rm -f $(HANGOUTS_TARGET) hangouts.pb-c.h hangouts.pb-c.c hangout_media.pb-c.h hangout_media.pb-c.c

