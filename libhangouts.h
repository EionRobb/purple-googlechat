


#ifndef _LIBHANGOUTS_H_
#define _LIBHANGOUTS_H_

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "account.h"
#include "connection.h"
#include "circbuffer.h"
#include "http.h"

#define HANGOUTS_PLUGIN_ID "prpl-hangouts"

#ifndef N_
#	define N_(a) (a)
#endif
#ifndef _
#	define _(a) (a)
#endif

#define HANGOUTS_API_OAUTH2_TOKEN_URL "https://www.googleapis.com/oauth2/v3/token"

// #define GOOGLE_CLIENT_ID "1055179169992-mvb9smig5gflo8bq6m5pao05jqmov76h.apps.googleusercontent.com"
// #define GOOGLE_CLIENT_SECRET "Hj5Cv38ZM__uO1bTQxOtWwkT"

#define GOOGLE_CLIENT_ID "936475272427.apps.googleusercontent.com"
#define GOOGLE_CLIENT_SECRET "KWsJlkaMn1jGLxQpWxMnOox-"

#define MINIFIED_OAUTH_URL "https://goo.gl/eJHvDX"
#define HANGOUTS_API_OAUTH2_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
// #define HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL MINIFIED_OAUTH_URL

#define HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL "https://accounts.google.com/o/oauth2/auth?client_id=" GOOGLE_CLIENT_ID "&scope=https://www.google.com/accounts/OAuthLogin&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code"


#define purple_request_cpar_from_connection(a)  purple_connection_get_account(a), NULL, NULL
#define purple_connection_error                 purple_connection_error_reason

#define PurpleCircularBuffer                    PurpleCircBuffer
#define purple_circular_buffer_new              purple_circ_buffer_new
#define purple_circular_buffer_destroy          purple_circ_buffer_destroy
#define purple_circular_buffer_append           purple_circ_buffer_append
#define purple_circular_buffer_get_max_read     purple_circ_buffer_get_max_read
#define purple_circular_buffer_mark_read        purple_circ_buffer_mark_read
#define purple_circular_buffer_get_output(buf)  ((const gchar *) (buf)->outptr)

typedef struct {
	PurpleAccount *account;
	PurpleConnection *pc;
	
	PurpleHttpCookieJar *cookie_jar;
	gchar *refresh_token;
	gchar *access_token;
	gchar *gsessionid_param;
	gchar *sid_param;
	
	PurpleCircularBuffer *channel_buffer;
	PurpleHttpKeepalivePool *channel_keepalive_pool;
} HangoutsAccount;


#endif /*_LIBHANGOUTS_H_*/