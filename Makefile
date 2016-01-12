
PIDGIN_TREE_TOP ?= ../pidgin-2.10.12
LIBPURPLE_DIR ?= $(PIDGIN_TREE_TOP)/libpurple
WIN32_DEV_TOP ?= $(PIDGIN_TREE_TOP)/../win32-dev
PROTOBUF_C_DIR ?= $(WIN32_DEV_TOP)/protobuf-c-Release-2.6

CC = $(WIN32_DEV_TOP)/mingw/bin/gcc


CFLAGS = -I$(WIN32_DEV_TOP)/glib-2.28.8/include -I$(WIN32_DEV_TOP)/glib-2.28.8/include/glib-2.0 -I$(WIN32_DEV_TOP)/glib-2.28.8/lib/glib-2.0/include -I$(WIN32_DEV_TOP)/json-glib-0.8.0 -I$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/include -I$(PIDGIN_TREE_TOP)/libpurple -DENABLE_NLS -DPACKAGE_VERSION='"$(PLUGIN_VERSION)"' -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-unused-parameter -I. -fno-strict-aliasing
LDFLAGS = -L$(PIDGIN_TREE_TOP)/libpurple -L$(WIN32_DEV_TOP)/glib-2.28.8/lib -L$(WIN32_DEV_TOP)/protobuf-c-Release-2.6/bin -L$(WIN32_DEV_TOP)/json-glib-0.14/lib -lpurple -lintl -lglib-2.0 -lgobject-2.0 -ljson-glib-1.0 -lprotobuf-c-1 -ggdb -static-libgcc

hangouts.pb-c.c: hangouts.proto
	$(PROTOBUF_C_DIR)/bin/protoc-c.exe --c_out=. hangouts.proto

libhangouts.dll: libhangouts.c hangouts.pb-c.c
	$(CC) -shared -o $@ hangouts.pb-c.c libhangouts.c $(CFLAGS) $(LDFLAGS)
	
libhangouts.exe: libhangouts.c hangouts.pb-c.c
	$(CC) -o $@ -DDEBUG libhangouts.c hangouts.pb-c.c $(CFLAGS) $(LDFLAGS)

all: libhangouts.dll

install: libhangouts.dll
	cp libhangouts.dll "$(PROGRAMFILES)/Pidgin/plugins"