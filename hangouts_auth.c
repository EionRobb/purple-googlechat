#include "hangouts_auth.h"

#include "http.h"
#include "hangouts_json.h"

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
		//hangouts_channel_set_access_token(ha->channel, ha->access_token);
	} else {
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
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
	g_string_append_printf(postdata, "refresh_token=%s&", purple_url_encode(ha->refresh_token));
	g_string_append(postdata, "grant_type=refresh_token&");
	
	request = purple_http_request_new(HANGOUTS_API_OAUTH2_TOKEN_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(pc, request, hangouts_oauth_refresh_token_cb, ha);
	
	g_string_free(postdata, TRUE);
}


static void
hangouts_oauth_with_code_cb (PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
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
		
		//hangouts_channel_set_access_token(ha->channel, ha->access_token);
		purple_account_set_remember_password(account, TRUE);
		purple_account_set_password(account, ha->refresh_token);
	} else {
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, 
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
