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

#include "hangouts_auth.h"

#include "core.h"
#include "debug.h"
#include "http.h"
#include "hangouts_json.h"
#include "hangouts_connection.h"
#include "hangouts_conversation.h"


typedef struct {
	gpointer unused1;
	gpointer unused2;
	gpointer unused3;
	gpointer unused4;
	gpointer unused5;
	int unused6;
	int unused7;
	int unused8;
	int unused9;
	
	gpointer set;
} bitlbee_account_t;

typedef struct {
	bitlbee_account_t *acc;
} bitlbee_im_connection;

static gpointer bitlbee_module;
static bitlbee_im_connection *(*bitlbee_purple_ic_by_pa)(PurpleAccount *);
static int (*bitlbee_set_setstr)(gpointer *, const char *, const char *);
static gboolean bitlbee_password_funcs_loaded = FALSE;

#ifdef _WIN32
#	include <windows.h>
#	define dlopen(filename, flag)  GetModuleHandleA(filename)
#	define dlsym(handle, symbol)   GetProcAddress(handle, symbol)
#	define dlclose(handle)         FreeLibrary(handle)
#else
#	include <dlfcn.h>
#endif

static void
save_bitlbee_password(PurpleAccount *account, const gchar *password)
{
	bitlbee_account_t *acc;
	bitlbee_im_connection *imconn;
	
	if (bitlbee_password_funcs_loaded == FALSE) {
		bitlbee_module = dlopen(NULL, 0);
		g_return_if_fail(bitlbee_module);
		
		bitlbee_purple_ic_by_pa = (gpointer) dlsym(bitlbee_module, "purple_ic_by_pa");
		bitlbee_set_setstr = (gpointer) dlsym(bitlbee_module, "set_setstr");
		
		bitlbee_password_funcs_loaded = TRUE;
	}
	
	imconn = bitlbee_purple_ic_by_pa(account);
	acc = imconn->acc;
	bitlbee_set_setstr(&acc->set, "password", password ? password : "");
}

static void
hangouts_save_refresh_token_password(PurpleAccount *account, const gchar *password)
{
	purple_account_set_password(account, password, NULL, NULL);
	
	if (g_strcmp0(purple_core_get_ui(), "BitlBee") == 0) {
		save_bitlbee_password(account, password);
	}
}


static void
hangouts_oauth_refresh_token_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	JsonObject *obj;
	const gchar *raw_response;
	gsize response_len;

	raw_response = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(raw_response, response_len);

	if (purple_http_response_is_successful(response) && obj)
	{
		ha->access_token = g_strdup(json_object_get_string_member(obj, "access_token"));
		hangouts_auth_get_session_cookies(ha);
	} else {
		if (obj != NULL) {
			if (json_object_has_member(obj, "error")) {
				if (g_strcmp0(json_object_get_string_member(obj, "error"), "invalid_grant") == 0) {
					hangouts_save_refresh_token_password(ha->account, NULL);
					purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						json_object_get_string_member(obj, "error_description"));
				} else {
					purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						json_object_get_string_member(obj, "error_description"));
				}
			} else {
				purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
					_("Invalid response"));
			}
		}
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response"));
	}

	json_object_unref(obj);
}

void
hangouts_oauth_refresh_token(HangoutsAccount *ha)
{
	PurpleHttpRequest *request;
	PurpleConnection *pc;
	GString *postdata;

	pc = ha->pc;

	postdata = g_string_new(NULL);
	g_string_append_printf(postdata, "client_id=%s&", purple_url_encode(GOOGLE_CLIENT_ID));
	g_string_append_printf(postdata, "client_secret=%s&", purple_url_encode(GOOGLE_CLIENT_SECRET));
	g_string_append_printf(postdata, "refresh_token=%s&", purple_url_encode(ha->refresh_token));
	g_string_append(postdata, "grant_type=refresh_token&");
	
	request = purple_http_request_new(HANGOUTS_API_OAUTH2_TOKEN_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(pc, request, hangouts_oauth_refresh_token_cb, ha);
	
	purple_debug_info("hangouts", "Postdata: %s\n", postdata->str);
	
	g_string_free(postdata, TRUE);
}


static void
hangouts_oauth_with_code_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	JsonObject *obj;
	const gchar *raw_response;
	gsize response_len;
	PurpleAccount *account = ha->account;

	raw_response = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(raw_response, response_len);

	if (purple_http_response_is_successful(response) && obj)
	{
		ha->access_token = g_strdup(json_object_get_string_member(obj, "access_token"));
		ha->refresh_token = g_strdup(json_object_get_string_member(obj, "refresh_token"));
		
		purple_account_set_remember_password(account, TRUE);
		hangouts_save_refresh_token_password(account, ha->refresh_token);
		
		hangouts_auth_get_session_cookies(ha);
	} else {
		if (obj != NULL) {
			if (json_object_has_member(obj, "error")) {
				if (g_strcmp0(json_object_get_string_member(obj, "error"), "invalid_grant") == 0) {
					hangouts_save_refresh_token_password(ha->account, NULL);
					purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						json_object_get_string_member(obj, "error_description"));
				} else {
					purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						json_object_get_string_member(obj, "error_description"));
				}
			} else {
				purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
					_("Invalid response"));
			}
		}
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Invalid response"));
	}

	json_object_unref(obj);
}

void
hangouts_oauth_with_code(HangoutsAccount *ha, const gchar *auth_code)
{
	PurpleHttpRequest *request;
	PurpleConnection *pc;
	GString *postdata;

	pc = ha->pc;

	postdata = g_string_new(NULL);
	g_string_append_printf(postdata, "client_id=%s&", purple_url_encode(GOOGLE_CLIENT_ID));
	g_string_append_printf(postdata, "client_secret=%s&", purple_url_encode(GOOGLE_CLIENT_SECRET));
	g_string_append_printf(postdata, "code=%s&", purple_url_encode(auth_code));
	g_string_append_printf(postdata, "redirect_uri=%s&", purple_url_encode(HANGOUTS_API_OAUTH2_REDIRECT_URI));
	g_string_append(postdata, "grant_type=authorization_code&");

	request = purple_http_request_new(HANGOUTS_API_OAUTH2_TOKEN_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(pc, request, hangouts_oauth_with_code_cb, ha);
	
	g_string_free(postdata, TRUE);
}


/*****************************************************************************/


void
hangouts_auth_get_session_cookies_got_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	guint64 last_event_timestamp;
	
	gchar *sapisid_cookie = purple_http_cookie_jar_get(ha->cookie_jar, "SAPISID");
	
	if (sapisid_cookie == NULL) {
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
			_("SAPISID Cookie not recieved"));
		return;
	}
	
	//Restore the last_event_timestamp before it gets overridden by new events
	last_event_timestamp = purple_account_get_int(ha->account, "last_event_timestamp_high", 0);
	if (last_event_timestamp != 0) {
		last_event_timestamp = (last_event_timestamp << 32) | ((guint64) purple_account_get_int(ha->account, "last_event_timestamp_low", 0) & 0xFFFFFFFF);
		ha->last_event_timestamp = last_event_timestamp;
	}
	
	// SOUND THE TRUMPETS
	hangouts_fetch_channel_sid(ha);
	purple_connection_set_state(ha->pc, PURPLE_CONNECTION_CONNECTED);
	//TODO trigger event instead
	hangouts_get_self_info(ha);
	hangouts_get_conversation_list(ha);
}

static void
hangouts_auth_get_session_cookies_uberauth_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	PurpleHttpRequest *request;
	const gchar *uberauth;

	uberauth = purple_http_response_get_data(response, NULL);

	if (purple_http_response_get_error(response) != NULL) {
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
			_("Auth error"));
		return;
	}
	
	purple_debug_misc("hangouts-prpl", "uberauth: %s", uberauth);

	request = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(request, "https://accounts.google.com/MergeSession" "?service=mail&continue=http://www.google.com&uberauth=%s", purple_url_encode(uberauth));
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->access_token);
	purple_http_request_set_max_redirects(request, 0);
	
	purple_http_request(ha->pc, request, hangouts_auth_get_session_cookies_got_cb, ha);
}

void
hangouts_auth_get_session_cookies(HangoutsAccount *ha)
{
	PurpleHttpRequest *request;

	request = purple_http_request_new("https://accounts.google.com/accounts/OAuthLogin"
	                                  "?source=pidgin&issueuberauth=1");
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->access_token);

	purple_http_request(ha->pc, request, hangouts_auth_get_session_cookies_uberauth_cb, ha);
}
