/*
 * GoogleChat Plugin for libpurple/Pidgin
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




#ifndef _LIBGOOGLECHAT_H_
#define _LIBGOOGLECHAT_H_

#include <purple.h>

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#define PROTOBUF_C_UNPACK_ERROR(...) purple_debug_error("googlechat-protobuf", __VA_ARGS__)

#include "purplecompat.h"
#include "http.h"

#include "googlechat.pb-c.h"

#define GOOGLECHAT_PLUGIN_ID "prpl-googlechat"
#define GOOGLECHAT_PLUGIN_VERSION "0.1"

#define GOOGLECHAT_BUFFER_DEFAULT_SIZE 4096

#ifndef N_
#	define N_(a) (a)
#endif
#ifndef _
#	define _(a) (a)
#endif

#define GOOGLECHAT_API_OAUTH2_TOKEN_URL "https://www.googleapis.com/oauth2/v4/token"

// #define GOOGLE_CLIENT_ID "1055179169992-mvb9smig5gflo8bq6m5pao05jqmov76h.apps.googleusercontent.com"
// #define GOOGLE_CLIENT_SECRET "Hj5Cv38ZM__uO1bTQxOtWwkT"

#define GOOGLE_CLIENT_ID "936475272427.apps.googleusercontent.com"
#define GOOGLE_CLIENT_SECRET "KWsJlkaMn1jGLxQpWxMnOox-"
#define GOOGLE_GPLUS_KEY "AIzaSyAfFJCeph-euFSwtmqFZi0kaKk-cZ5wufM"

#define MINIFIED_OAUTH_URL "https://goo.gl/eJHvDX"
#define GOOGLECHAT_API_OAUTH2_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
// #define GOOGLECHAT_API_OAUTH2_AUTHORIZATION_CODE_URL MINIFIED_OAUTH_URL

#define GOOGLECHAT_API_OAUTH2_AUTHORIZATION_CODE_URL "https://accounts.google.com/o/oauth2/auth?client_id=" GOOGLE_CLIENT_ID "&scope=https://www.google.com/accounts/OAuthLogin&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code&device_name=purple-googlechat"

#define GOOGLECHAT_IMAGE_UPLOAD_URL "https://docs.google.com/upload/photos/resumable?authuser=0"

#define GOOGLECHAT_ACTIVE_CLIENT_TIMEOUT 120

#define GOOGLECHAT_MAGIC_HALF_EIGHT_SLASH_ME_TYPE 4

typedef struct {
	PurpleAccount *account;
	PurpleConnection *pc;
	
	PurpleHttpCookieJar *cookie_jar;
	gchar *refresh_token;
	gchar *access_token;
	gchar *id_token;
	gchar *csessionid_param;
	gchar *sid_param;
	gchar *client_id;
	gchar *self_gaia_id;
	gchar *self_phone;
	
	gint64 last_event_timestamp;
	PurpleConversation *last_conversation_focused;
	guint poll_buddy_status_timeout;
	gint server_time_offset;
	gint64 last_aid;
	gint64 last_ofs;
	
	GByteArray *channel_buffer;
	guint channel_watchdog;
	PurpleHttpConnection *channel_connection;
	PurpleHttpKeepalivePool *channel_keepalive_pool;
	PurpleHttpKeepalivePool *icons_keepalive_pool;
	PurpleHttpKeepalivePool *api_keepalive_pool;
	gint idle_time;
	gint active_client_timeout;
	gint last_data_received; // A timestamp of when we last received data from the stream
	
	GHashTable *one_to_ones;     // A store of known conv_id's->gaia_id's
	GHashTable *one_to_ones_rev; // A store of known gaia_id's->conv_id's
	GHashTable *group_chats;     // A store of known conv_id's
	GHashTable *sent_message_ids;// A store of message id's that we generated from this instance
	
	guint refresh_token_timeout;
	guint dynamite_token_timeout;
} GoogleChatAccount;


typedef enum
{
    GOOGLECHAT_DEVICE_TYPE_UNKNOWN = 0x0000,
    GOOGLECHAT_DEVICE_TYPE_MOBILE  = 0x0001,
    GOOGLECHAT_DEVICE_TYPE_DESKTOP = 0x0002,
    GOOGLECHAT_DEVICE_TYPE_TABLET  = 0x0004
} GoogleChatDeviceTypeFlags;

typedef struct {
	PurpleBuddy *buddy;
	gboolean in_call;
	gint64 last_seen;
	GoogleChatDeviceTypeFlags device_type;
} GoogleChatBuddy;


gboolean googlechat_is_valid_id(const gchar *id);

#endif /*_LIBGOOGLECHAT_H_*/