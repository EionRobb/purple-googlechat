


#ifndef _LIBHANGOUTS_H_
#define _LIBHANGOUTS_H_

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "http.h"

#define HANGOUTS_PLUGIN_ID "prpl-hangouts"

#ifndef N_
#	define N_(a) (a)
#endif
#ifndef _
#	define _(a) (a)
#endif

#define HANGOUTS_API_OAUTH2_TOKEN_URL "https://www.googleapis.com/oauth2/v3/token"

#define GOOGLE_CLIENT_ID "1055179169992-mvb9smig5gflo8bq6m5pao05jqmov76h.apps.googleusercontent.com"
#define GOOGLE_CLIENT_SECRET "Hj5Cv38ZM__uO1bTQxOtWwkT"

#define MINIFIED_OAUTH_URL "https://goo.gl/eJHvDX"
#define HANGOUTS_API_OAUTH2_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
#define HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL MINIFIED_OAUTH_URL

typedef struct {
	PurpleAccount *account;
	PurpleConnection *pc;
	
	PurpleHttpCookieJar *cookie_jar;
	gchar *refresh_token;
	gchar *access_token;
} HangoutsAccount;


#include "hangouts_json.h"
#include "hangouts_pblite.h"
#include "hangouts_connection.h"


#define purple_request_cpar_from_connection(a)  purple_connection_get_account(a), NULL, NULL
#define purple_connection_error                 purple_connection_error_reason


#endif /*_LIBHANGOUTS_H_*/