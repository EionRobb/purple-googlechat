


#ifndef _LIBHANGOUTS_H_
#define _LIBHANGOUTS_H_

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "account.h"
#include "connection.h"
#include "version.h"
#if PURPLE_VERSION_CHECK(3, 0, 0)
#include "circularbuffer.h"
#include "ciphers/sha1hash.h"
#else
#include "circbuffer.h"
#endif
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

#define purple_circular_buffer_destroy		g_object_unref
#define purple_hash_destroy			g_object_unref

#else

#define PURPLE_TYPE_CONNECTION	purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION)
#define PURPLE_IS_CONNECTION	PURPLE_CONNECTION_IS_VALID

#define PURPLE_CONNECTION_CONNECTED		PURPLE_CONNECTED

#define purple_request_cpar_from_connection(a)  purple_connection_get_account(a), NULL, NULL
#define purple_connection_get_protocol		purple_connection_get_prpl
#define purple_connection_error                 purple_connection_error_reason

#define purple_protocol_got_user_status		purple_prpl_got_user_status

#define purple_account_set_password(account, password, dummy1, dummy2) \
		purple_account_set_password(account, password);

#define PurpleCircularBuffer                    PurpleCircBuffer
#define purple_circular_buffer_new              purple_circ_buffer_new
#define purple_circular_buffer_destroy          purple_circ_buffer_destroy
#define purple_circular_buffer_append           purple_circ_buffer_append
#define purple_circular_buffer_get_max_read     purple_circ_buffer_get_max_read
#define purple_circular_buffer_mark_read        purple_circ_buffer_mark_read
#define purple_circular_buffer_get_output(buf)  ((const gchar *) (buf)->outptr)

#define PurpleHash		PurpleCipherContext
#define purple_sha1_hash_new()	purple_cipher_context_new(purple_ciphers_find_cipher("sha1"), NULL)
#define purple_hash_append	purple_cipher_context_append
#define purple_hash_digest_to_str(ctx, data, size) \
				purple_cipher_context_digest_to_str(ctx, size, data, NULL)
#define purple_hash_destroy	purple_cipher_context_destroy
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