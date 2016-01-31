#ifndef _GLIBCOMPAT_H_
#define _GLIBCOMPAT_H_

#if !GLIB_CHECK_VERSION(2, 32, 0)
#define g_hash_table_contains(hash_table, key) g_hash_table_lookup_extended(hash_table, key, NULL, NULL)
#endif

#endif /*_GLIBCOMPAT_H_*/