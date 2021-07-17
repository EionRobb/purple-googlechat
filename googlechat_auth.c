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

#include "googlechat_auth.h"

#include <glib.h>
#include <purple.h>

#include "http.h"
#include "googlechat_json.h"
#include "googlechat_connection.h"
#include "googlechat_conversation.h"


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
static gchar *last_dlopen_error = NULL;
#	define dlerror()               (g_free(last_dlopen_error),last_dlopen_error=g_win32_error_message(GetLastError()))
#	define RTLD_LAZY               0x0001
#else
#	include <dlfcn.h>
#endif

static void
save_bitlbee_password(PurpleAccount *account, const gchar *password)
{
	bitlbee_account_t *acc;
	bitlbee_im_connection *imconn;

	gboolean result = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_accounts_get_handle(), "bitlbee-set-account-password", account, password));

	if (result) {
		return;
	}
	
	if (bitlbee_password_funcs_loaded == FALSE) {
		bitlbee_module = dlopen(NULL, RTLD_LAZY);
		if (bitlbee_module == NULL) {
			purple_debug_error("googlechat", "Couldn't acquire address of bitlbee handle: %s\n", dlerror());
			g_return_if_fail(bitlbee_module);
		}
		
		bitlbee_purple_ic_by_pa = (gpointer) dlsym(bitlbee_module, "purple_ic_by_pa");
		bitlbee_set_setstr = (gpointer) dlsym(bitlbee_module, "set_setstr");
		
		bitlbee_password_funcs_loaded = TRUE;
	}
	
	imconn = bitlbee_purple_ic_by_pa(account);
	acc = imconn->acc;
	bitlbee_set_setstr(&acc->set, "password", password ? password : "");
}

static void
googlechat_save_refresh_token_password(PurpleAccount *account, const gchar *password)
{
	purple_account_set_password(account, password, NULL, NULL);
	
	if (g_strcmp0(purple_core_get_ui(), "BitlBee") == 0) {
		save_bitlbee_password(account, password);
	}
}


gboolean googlechat_auth_get_dynamite_token(GoogleChatAccount *ha);

static void
googlechat_oauth_refresh_token_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
	JsonObject *obj;
	const gchar *raw_response;
	gsize response_len;

	raw_response = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(raw_response, response_len);

	if (purple_http_response_is_successful(response) && obj)
	{
		gchar *id_token = g_strdup(json_object_get_string_member(obj, "access_token"));
		gint expires_in = json_object_get_int_member(obj, "expires_in");
		
		if (ha->id_token) {
			g_free(ha->id_token);
		}
		ha->id_token = id_token;
		googlechat_auth_get_dynamite_token(ha);
		
		if (expires_in > 30) {
			if (ha->refresh_token_timeout) {
				g_source_remove(ha->refresh_token_timeout);
			}
			ha->refresh_token_timeout = g_timeout_add_seconds(expires_in - 30, (GSourceFunc) googlechat_oauth_refresh_token, ha);
		}
	} else {
		if (obj != NULL) {
			if (json_object_has_member(obj, "error")) {
				if (g_strcmp0(json_object_get_string_member(obj, "error"), "invalid_grant") == 0) {
					googlechat_save_refresh_token_password(ha->account, NULL);
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

gboolean
googlechat_oauth_refresh_token(GoogleChatAccount *ha)
{
	PurpleHttpRequest *request;
	PurpleConnection *pc;
	GString *postdata;

	pc = ha->pc;
	if (!PURPLE_IS_CONNECTION(pc)) {
		return FALSE;
	}

	postdata = g_string_new(NULL);
	g_string_append_printf(postdata, "client_id=%s&", purple_url_encode(GOOGLE_CLIENT_ID));
	//g_string_append_printf(postdata, "client_secret=%s&", purple_url_encode(GOOGLE_CLIENT_SECRET));
	g_string_append_printf(postdata, "refresh_token=%s&", purple_url_encode(ha->refresh_token));
	g_string_append(postdata, "grant_type=refresh_token&");
	
	request = purple_http_request_new(GOOGLECHAT_API_OAUTH2_TOKEN_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(pc, request, googlechat_oauth_refresh_token_cb, ha);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
	
	return FALSE;
}


static void
googlechat_oauth_with_code_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
	JsonObject *obj;
	const gchar *raw_response;
	gsize response_len;
	PurpleAccount *account = ha->account;

	raw_response = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(raw_response, response_len);

	if (purple_http_response_is_successful(response) && obj)
	{
		gchar *id_token = g_strdup(json_object_get_string_member(obj, "id_token"));
		if (ha->id_token) {
			g_free(ha->id_token);
		}
		ha->id_token = id_token;
		ha->refresh_token = g_strdup(json_object_get_string_member(obj, "refresh_token"));
		
		purple_account_set_remember_password(account, TRUE);
		googlechat_save_refresh_token_password(account, ha->refresh_token);
		
		googlechat_auth_get_dynamite_token(ha);
	} else {
		if (obj != NULL) {
			if (json_object_has_member(obj, "error")) {
				if (g_strcmp0(json_object_get_string_member(obj, "error"), "invalid_grant") == 0) {
					googlechat_save_refresh_token_password(ha->account, NULL);
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
googlechat_oauth_with_code(GoogleChatAccount *ha, const gchar *auth_code)
{
	PurpleHttpRequest *request;
	PurpleConnection *pc;
	GString *postdata;

	pc = ha->pc;

	postdata = g_string_new(NULL);
	g_string_append_printf(postdata, "client_id=%s&", purple_url_encode(GOOGLE_CLIENT_ID));
	g_string_append_printf(postdata, "client_secret=%s&", purple_url_encode(GOOGLE_CLIENT_SECRET));
	g_string_append_printf(postdata, "code=%s&", purple_url_encode(auth_code));
	g_string_append_printf(postdata, "redirect_uri=%s&", purple_url_encode(GOOGLECHAT_API_OAUTH2_REDIRECT_URI));
	g_string_append(postdata, "grant_type=authorization_code&");

	request = purple_http_request_new(GOOGLECHAT_API_OAUTH2_TOKEN_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(pc, request, googlechat_oauth_with_code_cb, ha);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
}


/*****************************************************************************/


void
googlechat_auth_get_dynamite_token_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
	guint64 last_event_timestamp;
	JsonObject *obj;
	const gchar *raw_response;
	gsize response_len;
	
	if (!purple_http_response_is_successful(response)) {
		int error_code = purple_http_response_get_code(response);
		if (error_code == 401 || error_code == 403) {
			purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
				_("Auth error"));
		} else {
			purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				purple_http_response_get_error(response));
		}
		return;
	}

	raw_response = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(raw_response, response_len);
	
	g_free(ha->access_token);
	ha->access_token = g_strdup(json_object_get_string_member(obj, "token"));
	
	//Restore the last_event_timestamp before it gets overridden by new events
	last_event_timestamp = purple_account_get_int(ha->account, "last_event_timestamp_high", 0);
	if (last_event_timestamp != 0) {
		last_event_timestamp = (last_event_timestamp << 32) | ((guint64) purple_account_get_int(ha->account, "last_event_timestamp_low", 0) & 0xFFFFFFFF);
		ha->last_event_timestamp = last_event_timestamp;
	}
	
	// SOUND THE TRUMPETS
	//googlechat_fetch_channel_sid(ha);
	googlechat_register_webchannel(ha);
	purple_connection_set_state(ha->pc, PURPLE_CONNECTION_CONNECTED);
	
	//TODO trigger event instead
	googlechat_get_self_user_status(ha);
	googlechat_get_conversation_list(ha);
	
	if (ha->poll_buddy_status_timeout) {
		g_source_remove(ha->poll_buddy_status_timeout);
	}
	ha->poll_buddy_status_timeout = g_timeout_add_seconds(120, googlechat_poll_buddy_status, ha);
	
	gint expires_in = atoi(json_object_get_string_member(obj, "expiresIn"));
	if (expires_in > 30) {
		if (ha->dynamite_token_timeout) {
			g_source_remove(ha->dynamite_token_timeout);
		}
		ha->dynamite_token_timeout = g_timeout_add_seconds(expires_in - 30, (GSourceFunc) googlechat_auth_get_dynamite_token, ha);
	}
}

gboolean
googlechat_auth_get_dynamite_token(GoogleChatAccount *ha)
{
	GString *postdata;
	PurpleHttpRequest *request;

	if (!PURPLE_IS_CONNECTION(ha->pc)) {
		return FALSE;
	}

	request = purple_http_request_new("https://oauthaccountmanager.googleapis.com/v1/issuetoken");
	purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->id_token);

	postdata = g_string_new(NULL);
	g_string_append_printf(postdata, "app_id=%s&", purple_url_encode("com.google.Dynamite"));
	g_string_append_printf(postdata, "client_id=%s&", purple_url_encode("576267593750-sbi1m7khesgfh1e0f2nv5vqlfa4qr72m.apps.googleusercontent.com"));
	g_string_append(postdata, "passcode_present=YES&");
	g_string_append(postdata, "response_type=token&");
	g_string_append_printf(postdata, "scope=%s&", purple_url_encode("https://www.googleapis.com/auth/dynamite https://www.googleapis.com/auth/drive https://www.googleapis.com/auth/mobiledevicemanagement https://www.googleapis.com/auth/notifications https://www.googleapis.com/auth/supportcontent https://www.googleapis.com/auth/chat.integration https://www.googleapis.com/auth/peopleapi.readonly"));
	
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(ha->pc, request, googlechat_auth_get_dynamite_token_cb, ha);
	purple_http_request_unref(request);
	
	g_string_free(postdata, TRUE);
	
	return FALSE;
}


static void
googlechat_cache_ssl_certs_cb(gpointer userdata, PurpleSslConnection *psc, PurpleInputCondition cond)
{
	// What a senseless waste of human life
	purple_ssl_close(psc);
}

static void
googlechat_tls_cached_verified(PurpleCertificateVerificationStatus st, gpointer userdata)
{
	PurpleCertificateVerificationRequest *vrq = userdata;
	
	if (st == PURPLE_CERTIFICATE_VALID) {
		GList *certs = vrq->cert_chain, *cur;
		PurpleCertificatePool *ca, *tls_peers;
		const gchar *cert_id;
		
		ca = purple_certificate_find_pool("x509", "ca");
		tls_peers = purple_certificate_find_pool("x509", "tls_peers");
		
		if (certs && certs->next) {
			PurpleCertificate *cert = certs->data;
			cert_id = purple_certificate_get_subject_name(cert);
			if (!purple_certificate_pool_contains(tls_peers, cert_id)) {
				purple_certificate_pool_store(tls_peers, cert_id, cert);
			}
			
			for (cur = certs->next; cur; cur = cur->next) {
				cert = cur->data;
				cert_id = purple_certificate_get_subject_name(cert);
				
				if (!purple_certificate_pool_contains(ca, cert_id)) {
					purple_certificate_pool_store(ca, cert_id, cert);
				}
				if (!purple_certificate_pool_contains(tls_peers, cert_id)) {
					purple_certificate_pool_store(tls_peers, cert_id, cert);
				}
			}
		}
	}
	
	purple_certificate_verify_complete(vrq, st);
}
		 
static void
googlechat_tls_cached_start_verify(PurpleCertificateVerificationRequest *vrq)
{
	PurpleCertificateVerifier *x509_tls_cached = purple_certificate_find_verifier("x509", "tls_cached");
	
	if (x509_tls_cached) {
		purple_certificate_verify(x509_tls_cached, vrq->subject_name, vrq->cert_chain, googlechat_tls_cached_verified, vrq);
		
	} else {
		purple_certificate_verify_complete(vrq, PURPLE_CERTIFICATE_INVALID);
	}
}

static void
googlechat_tls_cached_destroy_request(PurpleCertificateVerificationRequest *vrq)
{
	// Neat
}

static PurpleCertificateVerifier googlechat_tls_cached = {
	"x509",                         /* Scheme name */
	"googlechat",                   /* Verifier name */
	googlechat_tls_cached_start_verify,   /* Verification begin */
	googlechat_tls_cached_destroy_request,/* Request cleanup */

	NULL,
	NULL,
	NULL,
	NULL

};

void
googlechat_cache_ssl_certs(GoogleChatAccount *ha)
{
	PurpleSslConnection *ssl_conn;
	
	ssl_conn = purple_ssl_connect(ha->account, "chat.google.com", 443, googlechat_cache_ssl_certs_cb, NULL, ha);
	ssl_conn->verifier = &googlechat_tls_cached;
}
