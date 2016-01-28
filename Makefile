
PIDGIN_TREE_TOP ?= ../pidgin-2.10.12
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
PROTOBUF_C_DIR ?= $(WIN32_DEV_TOP)/protobuf-c-Release-2.6

WIN32_CC ?= $(WIN32_DEV_TOP)/mingw/bin/gcc

CC ?= gcc
PROTOC-C ?= protoc-c
PKG_CONFIG ?= pkg-config


CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.14/include/json-glib-1.0 -I$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/include -I$(PIDGIN_TREE_TOP)/libpurple -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -I. -fno-strict-aliasing
LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(PROTOBUF_C_DIR)/bin -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lprotobuf-c-1 -ggdb -static-libgcc -lz

C_FILES := hangouts.pb-c.c hangouts_json.c hangouts_pblite.c hangouts_connection.c hangouts_auth.c hangouts_events.c
PURPLE_COMPAT_FILES := purple2compat/http.c purple2compat/purple-socket.c
PURPLE_C_FILES := libhangouts.c $(C_FILES)
TEST_C_FILES := hangouts_test.c $(C_FILES)

all: libhangouts.so

hangouts.pb-c.c: hangouts.proto
	$(PROTOC-C) --c_out=. hangouts.proto

libhangouts.so: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(CC) -fPIC -shared -o $@ $^ `$(PKG_CONFIG) purple glib-2.0 json-glib-1.0 libprotobuf-c --libs --cflags` -I/usr/include/protobuf-c -Ipurple2compat

libhangouts3.so: $(PURPLE_C_FILES)
	$(CC) -fPIC -shared -o $@ $^ `$(PKG_CONFIG) purple-3 glib-2.0 json-glib-1.0 libprotobuf-c --libs --cflags` -I/usr/include/protobuf-c
	
libhangouts.dll: $(PURPLE_C_FILES) $(PURPLE_COMPAT_FILES)
	$(WIN32_CC) -shared -o $@ $^ $(CFLAGS) $(LDFLAGS) -Ipurple2compat
	
libhangouts.exe: $(TEST_C_FILES)
	$(WIN32_CC) -o $@ -DDEBUG $^ $(CFLAGS) $(LDFLAGS)

install: libhangouts.so
	cp $^ $(DESTDIR)`$(PKG_CONFIG) --variable=plugindir purple`
	
install-windows: libhangouts.dll
	cp $^ "$(PROGRAMFILES)/Pidgin/plugins"