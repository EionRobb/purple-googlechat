
PIDGIN_TREE_TOP ?= ../pidgin-2.10.12
PIDGIN3_TREE_TOP ?= ../pidgin-main
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
PROTOBUF_C_DIR ?= $(WIN32_DEV_TOP)/protobuf-c-Release-2.6

WIN32_CC ?= $(WIN32_DEV_TOP)/mingw/bin/gcc

CC ?= gcc
PROTOC_C ?= protoc-c
PKG_CONFIG ?= pkg-config

# Do some nasty OS and purple version detection
ifeq ($(OS),Windows_NT)
  HANGOUTS_TARGET = libhangouts.dll
  HANGOUTS_DEST = "$(PROGRAMFILES)/Pidgin/plugins"
else
  ifeq ($(shell pkg-config --exists purple-3 2>/dev/null && echo "true"),)
    ifeq ($(shell pkg-config --exists purple 2>/dev/null && echo "true"),)
      HANGOUTS_TARGET = FAILNOPURPLE
      HANGOUTS_DEST = 
    else
      HANGOUTS_TARGET = libhangouts.so
      HANGOUTS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple`
    endif
  else
    HANGOUTS_TARGET = libhangouts3.so
    HANGOUTS_DEST = $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple-3`
  endif
endif

WIN32_CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.14/include/json-glib-1.0 -I$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/include -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -fno-strict-aliasing
WIN32_LDFLAGS = -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(PROTOBUF_C_DIR)/bin -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lprotobuf-c-1 -g -ggdb -static-libgcc -lz
WIN32_PIDGIN2_CFLAGS = -I$(PIDGIN_TREE_TOP)/libpurple -I$(PIDGIN_TREE_TOP) $(WIN32_CFLAGS) 
WIN32_PIDGIN3_CFLAGS = -I$(PIDGIN3_TREE_TOP)/libpurple -I$(PIDGIN3_TREE_TOP) -I$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_CFLAGS) 
WIN32_PIDGIN2_LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple $(WIN32_LDFLAGS) 
WIN32_PIDGIN3_LDFLAGS = -L$(PIDGIN3_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/gplugin-dev/gplugin $(WIN32_LDFLAGS) -lgplugin

C_FILES := hangouts.pb-c.c hangouts_json.c hangouts_pblite.c hangouts_connection.c hangouts_auth.c hangouts_events.c hangouts_conversation.c
PURPLE_COMPAT_FILES := purple2compat/http.c purple2compat/purple-socket.c
PURPLE_C_FILES := libhangouts.c $(C_FILES)
TEST_C_FILES := hangouts_test.c $(C_FILES)

.PHONY:	all install install-windows FAILNOPURPLE

all: $(HANGOUTS_TARGET)

hangouts.pb-c.c: hangouts.proto
	$(PROTOC_C) --c_out=. hangouts.proto

libhangouts.so: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(CC) -fPIC -shared -o $@ $^ `$(PKG_CONFIG) purple glib-2.0 json-glib-1.0 libprotobuf-c --libs --cflags` -I/usr/include/protobuf-c -Ipurple2compat -g -ggdb

libhangouts3.so: $(PURPLE_C_FILES)
	$(CC) -fPIC -shared -o $@ $^ `$(PKG_CONFIG) purple-3 glib-2.0 json-glib-1.0 libprotobuf-c --libs --cflags` -I/usr/include/protobuf-c -g -ggdb

libhangouts.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

libhangouts3.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(WIN32_PIDGIN3_CFLAGS) $(WIN32_PIDGIN3_LDFLAGS)

libhangouts.exe: $(TEST_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -o $@ -DDEBUG $^ $(WIN32_PIDGIN2_CFLAGS) $(WIN32_PIDGIN2_LDFLAGS) -Ipurple2compat

install: $(HANGOUTS_TARGET)
	cp $^ $(HANGOUTS_DEST)

install-windows: libhangouts.dll
	cp $^ "$(PROGRAMFILES)/Pidgin/plugins"

FAILNOPURPLE:
	echo "You need libpurple development headers installed to be able to compile this plugin"

