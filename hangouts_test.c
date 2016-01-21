

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "debug.h"
#include "plugin.h"
#include "version.h"
#ifdef _WIN32
#	include "win32/win32dep.h"
#endif

#ifndef DEBUG
#	define DEBUG
#endif

#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"
#include "hangouts_connection.h"


static const gchar *test_string1 = "W1s4ODMsW3sicCI6IntcIjFcIjp7XCIxXCI6e1wiMVwiOntcIjFcIjoxLFwiMlwiOjF9fSxcIjRcIjpcIjE0NTIzMjgxNzMxNDZcIixcIjVcIjpcIlM4NDdcIn0sXCIyXCI6e1wiMVwiOntcIjFcIjpcImJhYmVsXCIsXCIyXCI6XCJjb25zZXJ2ZXIuZ29vZ2xlLmNvbVwifSxcIjJcIjpcIltcXFwiY2J1XFxcIixbW1syLG51bGwsXFxcIjIyOTY4MjM1MDAwMDI1OTEzNDBcXFwiLFtbMF1cXG5dXFxuLDE0NTIzMjgxNzI3ODcwMDAsWzFdXFxuXVxcbixudWxsLFtbW1xcXCJVZ3hHZHBDS19tU3JoQlg4aHJ4NEFhQUJBUVxcXCJdXFxuLFtcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMTQ1MjMyODE3Mjg5NjcyMyxbW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbixcXFwiMTA1MDQ3MTY0XFxcIiwzMF1cXG4sbnVsbCxudWxsLFtudWxsLG51bGwsW1tbMCxcXFwiYnV0IHRvIGxvYWQgZnJvbSBQRU0sIHlvdSBnbyB2aWEgREVSXFxcIl1cXG5dXFxuXVxcbl1cXG4sbnVsbCxudWxsLG51bGwsbnVsbCxcXFwiNy1IMFo3LVI5NVg4OG1TRzlrLUNkQVxcXCIsbnVsbCxudWxsLDEsMiwxLG51bGwsbnVsbCxbMV1cXG4sbnVsbCxudWxsLDEsMTQ1MjMyODE3Mjg5NjcyMyxudWxsLFtcXFwiNy1IMFo3LVI5NVg4OG1TRzlrLUNkQVxcXCIsMjMsbnVsbCwxNDUyMzI4MTcyODk2NzIzXVxcbixudWxsLDJdXFxuXVxcbixudWxsLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxbW1xcXCJVZ3hHZHBDS19tU3JoQlg4aHJ4NEFhQUJBUVxcXCJdXFxuLDEsbnVsbCxbbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLG51bGwsW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMF1cXG4sMiwzMCxbMV1cXG4sW1xcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiLFxcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiXVxcbiwxMzY3NjQ1ODMxNTYyMDAwLDE0NTIzMjgxNzI4OTY3MjMsMTM2NzY0NTgzMTU2MjAwMCxudWxsLG51bGwsW1tbMV1cXG4sMV1cXG5dXFxuXVxcbixudWxsLG51bGwsbnVsbCxbW1tcXFwiMTExNTIzMTUwNjIwMjUwMTY1ODY2XFxcIixcXFwiMTExNTIzMTUwNjIwMjUwMTY1ODY2XFxcIl1cXG4sMF1cXG4sW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMF1cXG5dXFxuLDAsMiwxLG51bGwsW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sW1xcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiLFxcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiXVxcbl1cXG4sW1tbXFxcIjExMTUyMzE1MDYyMDI1MDE2NTg2NlxcXCIsXFxcIjExMTUyMzE1MDYyMDI1MDE2NTg2NlxcXCJdXFxuLFxcXCJNaWtlIFJ1cHJlY2h0XFxcIiwyLG51bGwsMiwyXVxcbixbW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbixcXFwiRWlvbiBSb2JiXFxcIiwyLG51bGwsMiwyXVxcbl1cXG4sbnVsbCxudWxsLG51bGwsWzFdXFxuLDBdXFxuXVxcbixbWzIsbnVsbCxcXFwiMjI5NjgyMzUwMDAwMjU5MTM0MFxcXCIsW1swXVxcbl1cXG4sMTQ1MjMyODE3Mjc4NzAwMCxbMV1cXG5dXFxuLG51bGwsbnVsbCxudWxsLFtbXFxcIlVneEdkcENLX21TcmhCWDhocng0QWFBQkFRXFxcIl1cXG4sW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbiwxNDUyMzI4MTcyNzg3MDAwLDNdXFxuXVxcbixbWzIsbnVsbCxcXFwiMjI5NjgyMzUwMDAwMjU5MTM0MFxcXCIsW1swXVxcbl1cXG4sMTQ1MjMyODE3Mjc4NzAwMCxbMV1cXG5dXFxuLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLFtbXFxcIjExMDE3NDA2NjM3NTA2MTExODcyN1xcXCIsXFxcIjExMDE3NDA2NjM3NTA2MTExODcyN1xcXCJdXFxuLFtcXFwiVWd4R2RwQ0tfbVNyaEJYOGhyeDRBYUFCQVFcXFwiXVxcbiwxNDUyMzI4MTcyODk2NzIzXVxcbl1cXG5dXFxuXVxcblwifX0ifV1dDQpdDQo=";

static const gchar *test_string2 = "[[924,[\"noop\"]\
]\
]";


typedef struct {
	long long a;
} Test;

#include "cipher.h"

void
test_sapisidhash()
{
	PurpleCipherContext *sha1_ctx;
	gchar sha1[41];
	// GTimeVal time;
	// gint64 mstime;
	// gchar *mstime_str = "1447033700279";
	// gchar *sapisid_cookie = "b4qUZKO4943exo9W/AmP2OAZLWGDwTsuh1";
	// gchar *origin = "https://hangouts.google.com";
	// gchar *mstime_str = "1453320676906";
	// gchar *sapisid_cookie = "8U0lau5fPyo3YtYs/A-HOLpZCCmOZq_Dxg";
	// gchar *origin = "https://talkgadget.google.com";
	gchar *mstime_str = "1453320953775";
	gchar *sapisid_cookie = "rO-0tFp4QmEPFXjq/AbNg8GFjmRRFem1SF";
	gchar *origin = "https://hangouts.google.com";
	
	
	// g_get_current_time(&time);
	// mstime = (((gint64) time.tv_sec) * 1000) + (time.tv_usec / 1000);
	// mstime_str = g_strdup_printf("%" G_GINT64_FORMAT, mstime);
	// printf("mstime is %s\nexample:  1447033700279\n%ld %ld\n", mstime_str, time.tv_sec, time.tv_usec);
	
	purple_ciphers_init();
	
	sha1_ctx = purple_cipher_context_new(purple_ciphers_find_cipher("sha1"), NULL);
	purple_cipher_context_append(sha1_ctx, (guchar *) mstime_str, strlen(mstime_str));
	purple_cipher_context_append(sha1_ctx, (guchar *) " ", 1);
	purple_cipher_context_append(sha1_ctx, (guchar *) sapisid_cookie, strlen(sapisid_cookie));
	purple_cipher_context_append(sha1_ctx, (guchar *) " ", 1);
	purple_cipher_context_append(sha1_ctx, (guchar *) origin, strlen(origin));
	purple_cipher_context_digest_to_str(sha1_ctx, 41, sha1, NULL);
	purple_cipher_context_destroy(sha1_ctx);
	
	// printf("SAPISID: %s\nExpected %s\n", sha1, "38cb670a2eaa2aca37edf07293150865121275cd");
	// printf("SAPISID: %s\nExpected %s\n", sha1, "15f3b1c9e8fde2286fb2026bfc7e9788d56a0cec");
	printf("SAPISID: %s\nExpected %s\n", sha1, "c151285107edadea4b3d07f0400ce69291faf4dc");
}

int main (int argc, const char * argv[])
{
	g_type_init();
	
	(void) test_string1;
	gsize len;
	gchar *test_string1_real = (gchar*) g_base64_decode(test_string1, &len);
	
	test_sapisidhash();
	
	hangouts_process_data_chunks(test_string2, -1);
	hangouts_process_data_chunks(test_string1_real, len);
	
#ifndef O_BINARY 
#	define O_BINARY 0 
#endif
#ifndef O_RDONLY 
#	define O_RDONLY 0 
#endif
	printf("reading file test-response-1.txt");
	int fd = open("test-response-1.txt", O_RDONLY | O_BINARY);
	hangouts_process_channel(fd);
	close(fd);
	
	return 0;
}
