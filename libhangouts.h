


#ifndef _LIBHANGOUTS_H_
#define _LIBHANGOUTS_H_

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "purplecompat.h"

#include "account.h"
#include "connection.h"
#include "version.h"
#include "circularbuffer.h"
#include "ciphers/sha1hash.h"
#include "http.h"

#define HANGOUTS_PLUGIN_ID "prpl-hangouts"
#define HANGOUTS_PLUGIN_VERSION "0.1"

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


#if PURPLE_VERSION_CHECK(3, 0, 0)
typedef struct _HangoutsProtocol
{
	PurpleProtocol parent;
} HangoutsProtocol;

typedef struct _HangoutsProtocolClass
{
	PurpleProtocolClass parent_class;
} HangoutsProtocolClass;

G_MODULE_EXPORT GType hangouts_protocol_get_type(void);
#define HANGOUTS_TYPE_PROTOCOL			(hangouts_protocol_get_type())
#define HANGOUTS_PROTOCOL(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocol))
#define HANGOUTS_PROTOCOL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocolClass))
#define HANGOUTS_IS_PROTOCOL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), HANGOUTS_TYPE_PROTOCOL))
#define HANGOUTS_IS_PROTOCOL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), HANGOUTS_TYPE_PROTOCOL))
#define HANGOUTS_PROTOCOL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocolClass))

#endif

typedef struct {
	PurpleAccount *account;
	PurpleConnection *pc;
	
	PurpleHttpCookieJar *cookie_jar;
	gchar *refresh_token;
	gchar *access_token;
	gchar *gsessionid_param;
	gchar *sid_param;
	gchar *client_id;
	
	PurpleCircularBuffer *channel_buffer;
	PurpleHttpKeepalivePool *channel_keepalive_pool;
} HangoutsAccount;


#endif /*_LIBHANGOUTS_H_*/