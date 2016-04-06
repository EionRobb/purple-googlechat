/*
 * Hangouts Plugin for libpurple/Pidgin
 * Copyright (c) 2015-2016 Eion Robb, Mike Ruprecht
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */




#ifndef _LIBHANGOUTS_H_
#define _LIBHANGOUTS_H_

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#define PROTOBUF_C_UNPACK_ERROR(...) purple_debug_error("hangouts-protobuf", __VA_ARGS__)

#include "purplecompat.h"

#include "account.h"
#include "connection.h"
#include "ciphers/sha1hash.h"
#include "http.h"

#include "hangouts.pb-c.h"

#define HANGOUTS_PLUGIN_ID "prpl-hangouts"
#define HANGOUTS_PLUGIN_VERSION "0.1"

#define HANGOUTS_BUFFER_DEFAULT_SIZE 4096

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
#define GOOGLE_GPLUS_KEY "AIzaSyAfFJCeph-euFSwtmqFZi0kaKk-cZ5wufM"

#define MINIFIED_OAUTH_URL "https://goo.gl/eJHvDX"
#define HANGOUTS_API_OAUTH2_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
// #define HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL MINIFIED_OAUTH_URL

#define HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL "https://accounts.google.com/o/oauth2/auth?client_id=" GOOGLE_CLIENT_ID "&scope=https://www.google.com/accounts/OAuthLogin&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code"

#define HANGOUTS_IMAGE_UPLOAD_URL "https://docs.google.com/upload/photos/resumable?authuser=0"

#define HANGOUTS_ACTIVE_CLIENT_TIMEOUT 120

typedef struct {
	PurpleAccount *account;
	PurpleConnection *pc;
	
	PurpleHttpCookieJar *cookie_jar;
	gchar *refresh_token;
	gchar *access_token;
	gchar *gsessionid_param;
	gchar *sid_param;
	gchar *client_id;
	gchar *self_gaia_id;
	gchar *self_phone;
	ActiveClientState active_client_state;
	gint64 last_event_timestamp;
	
	GByteArray *channel_buffer;
	guint channel_watchdog;
	PurpleHttpConnection *channel_connection;
	PurpleHttpKeepalivePool *channel_keepalive_pool;
	PurpleHttpKeepalivePool *icons_keepalive_pool;
	gint idle_time;
	
	GHashTable *one_to_ones;     // A store of known conv_id's->gaia_id's
	GHashTable *one_to_ones_rev; // A store of known gaia_id's->conv_id's
	GHashTable *group_chats;     // A store of known conv_id's
	GHashTable *sent_message_ids;// A store of message id's that we generated from this instance
	GHashTable *google_voice_conversations; // A store of known SMS conv_id's
} HangoutsAccount;


#endif /*_LIBHANGOUTS_H_*/