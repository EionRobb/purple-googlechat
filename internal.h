
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#include "version.h"
#if !PURPLE_VERSION_CHECK(3, 0, 0)
#define purple_connection_is_disconnecting(c)   FALSE
#define purple_proxy_info_get_proxy_type        purple_proxy_info_get_type
#endif

#ifndef N_
#	define N_(a) (a)
#endif

#ifndef _
#	define _(a) (a)
#endif
