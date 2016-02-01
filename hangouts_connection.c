#define PURPLE_PLUGINS


#include "hangouts_connection.h"


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "ciphers/sha1hash.h"
#include "debug.h"

#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"


void
hangouts_process_data_chunks(HangoutsAccount *ha, const gchar *data, gsize len)
{
	JsonArray *chunks;
	guint i, num_chunks;
	
	chunks = json_decode_array(data, len);
	
	for (i = 0, num_chunks = json_array_get_length(chunks); i < num_chunks; i++) {
		JsonArray *chunk;
		JsonArray *array;
		JsonNode *array0;
		
		chunk = json_array_get_array_element(chunks, i);
		
		array = json_array_get_array_element(chunk, 1);
		array0 = json_array_get_element(array, 0);
		if (JSON_NODE_HOLDS_VALUE(array0)) {
			//probably a nooooop
			if (g_strcmp0(json_node_get_string(array0), "noop") == 0) {
				//A nope ninja delivers a wicked dragon kick
#ifdef DEBUG
				printf("noop\n");
#endif
			}
		} else {
			const gchar *p = json_object_get_string_member(json_node_get_object(array0), "p");
			JsonObject *wrapper = json_decode_object(p, -1);
			
			if (wrapper == NULL) {
				continue;
			}
			
			if (json_object_has_member(wrapper, "3")) {
				const gchar *new_client_id = json_object_get_string_member(json_object_get_object_member(wrapper, "3"), "2");
				purple_debug_info("hangouts", "Received new client_id: %s\n", new_client_id);
				
				g_free(ha->client_id);
				ha->client_id = g_strdup(new_client_id);
				
				hangouts_add_channel_services(ha);
			}
			if (json_object_has_member(wrapper, "2")) {
				const gchar *wrapper22 = json_object_get_string_member(json_object_get_object_member(wrapper, "2"), "2");
				JsonArray *pblite_message = json_decode_array(wrapper22, -1);

				if (pblite_message == NULL) {
#ifdef DEBUG
					printf("bad wrapper22 %s\n", wrapper22);
#endif
					json_object_unref(wrapper);
					continue;
				}
				
				//cbu == ClientBatchUpdate
				if (g_strcmp0(json_array_get_string_element(pblite_message, 0), "cbu") == 0) {
					BatchUpdate batch_update = BATCH_UPDATE__INIT;
					guint j;
					
#ifdef DEBUG
					printf("----------------------\n");
#endif
					pblite_decode((ProtobufCMessage *) &batch_update, pblite_message, TRUE);
#ifdef DEBUG
					printf("======================\n");
					printf("Is valid? %s\n", protobuf_c_message_check((ProtobufCMessage *) &batch_update) ? "Yes" : "No");
					printf("======================\n");
					JsonArray *debug = pblite_encode((ProtobufCMessage *) &batch_update);
					JsonNode *node = json_node_new(JSON_NODE_ARRAY);
					json_node_take_array(node, debug);
					gchar *json = json_encode(node, NULL);
					printf("Old: %s\nNew: %s\n", wrapper22, json);
					
					
					pblite_decode((ProtobufCMessage *) &batch_update, debug, TRUE);
					debug = pblite_encode((ProtobufCMessage *) &batch_update);
					json_node_take_array(node, debug);
					gchar *json2 = json_encode(node, NULL);
					printf("Mine1: %s\nMine2: %s\n", json, json2);
					
					g_free(json);
					g_free(json2);
					printf("----------------------\n");
#endif
					for(j = 0; j < batch_update.n_state_update; j++) {
						purple_signal_emit(purple_connection_get_protocol(ha->pc), "hangouts-received-stateupdate", ha->pc, batch_update.state_update[j]);
					}
				}
				
				json_array_unref(pblite_message);
			}
			
			json_object_unref(wrapper);
		}
	}
	
	json_array_unref(chunks);
}

static int
read_all(int fd, void *buf, size_t len)
{
	unsigned int rs = 0;
	while(rs < len)
	{
		int rval = read(fd, buf + rs, len - rs);
		if (rval == 0)
			break;
		if (rval < 0)
			return rval;

		rs += rval;
	}
	return rs;
}

void
hangouts_process_channel(int fd)
{
	gsize len, lenpos = 0;
	gchar len_str[256];
	gchar *chunk;
	
	while(read(fd, len_str + lenpos, 1) > 0) {
		//read up until \n
		if (len_str[lenpos] == '\n') {
			//convert to int, use as length of string to read
			len_str[lenpos] = '\0';
#ifdef DEBUG
			printf("len_str is %s\n", len_str);
#endif
			len = atoi(len_str);
			
			chunk = g_new(gchar, len * 2);
			//XX - could be a utf-16 length*2 though, so read up until \n????
			
			if (read_all(fd, chunk, len) > 0) {
				//throw chunk to hangouts_process_data_chunks
				hangouts_process_data_chunks(NULL, chunk, len);
			}
			
			g_free(chunk);
			
			lenpos = 0;
		} else {
			lenpos = lenpos + 1;
		}
	}
}

void
hangouts_process_channel_buffer(HangoutsAccount *ha)
{
	const gchar *bufdata;
	gchar *chunk, *chunk_pos;
	gsize remaining, bufsize;
	gchar *len_end;
	gchar *len_str;
	guint len_len; //len len len len len len len len len
	gsize len;
	
	g_return_if_fail(ha);
	g_return_if_fail(ha->channel_buffer);
	
	do {
		bufdata = purple_circular_buffer_get_output(ha->channel_buffer);
		bufsize = purple_circular_buffer_get_max_read(ha->channel_buffer);
		remaining = purple_circular_buffer_get_used(ha->channel_buffer);
		
		len_end = g_strstr_len(bufdata, bufsize, "\n");
		if (len_end == NULL) {
			// Not enough data to read
			purple_debug_info("hangouts", "Couldn't find length of chunk\n");
			return;
		}
		len_len = len_end - bufdata;
		len_str = g_strndup(bufdata, len_len);
		len = (gsize) atoi(len_str);
		g_free(len_str);
		
		// Len was 0 ?  Must have been a bad read :(
		g_return_if_fail(len);
		
		remaining = remaining - len_len - 1;
		
		if (len > remaining) {
			// Not enough data to read
			purple_debug_info("hangouts", "Couldn't read %d bytes when we have %d\n", len, remaining);
			return;
		}
		purple_circular_buffer_mark_read(ha->channel_buffer, len_len + 1);
		
		chunk_pos = chunk = g_new0(gchar, len + 1);
		while (remaining > 0) {
			bufsize = MIN(remaining, purple_circular_buffer_get_max_read(ha->channel_buffer));
			bufdata = purple_circular_buffer_get_output(ha->channel_buffer);
			
			g_memmove(chunk_pos, bufdata, bufsize);
			purple_circular_buffer_mark_read(ha->channel_buffer, bufsize);
			
			remaining -= bufsize;
			chunk_pos += bufsize;
		}
		
		//purple_debug_info("hangouts", "Got chunk %.*s\n", (int) len, chunk);
		
		hangouts_process_data_chunks(ha, chunk, len);
		
		remaining = purple_circular_buffer_get_used(ha->channel_buffer);
		
	} while (remaining);
}

static void
hangouts_set_auth_headers(HangoutsAccount *ha, PurpleHttpRequest *request)
{
	gint64 mstime;
	gchar *mstime_str;
	GTimeVal time;
	PurpleHash *hash;
	gchar sha1[41];
	gchar *sapisid_cookie;
	
	g_get_current_time(&time);
	mstime = (((gint64) time.tv_sec) * 1000) + (time.tv_usec / 1000);
	mstime_str = g_strdup_printf("%" G_GINT64_FORMAT, mstime);
	sapisid_cookie = purple_http_cookie_jar_get(ha->cookie_jar, "SAPISID");
	
	hash = purple_sha1_hash_new();
	purple_hash_append(hash, (guchar *) mstime_str, strlen(mstime_str));
	purple_hash_append(hash, (guchar *) " ", 1);
	purple_hash_append(hash, (guchar *) sapisid_cookie, strlen(sapisid_cookie));
	purple_hash_append(hash, (guchar *) " ", 1);
	purple_hash_append(hash, (guchar *) HANGOUTS_PBLITE_XORIGIN_URL, strlen(HANGOUTS_PBLITE_XORIGIN_URL));
	purple_hash_digest_to_str(hash, sha1, 41);
	purple_hash_destroy(hash);
	
	purple_http_request_header_set_printf(request, "Authorization", "SAPISIDHASH %s_%s", mstime_str, sha1);
	purple_http_request_header_set(request, "X-Origin", HANGOUTS_PBLITE_XORIGIN_URL);
	purple_http_request_header_set(request, "X-Goog-AuthUser", "0");
	
	g_free(sapisid_cookie);
}


static gboolean
hangouts_longpoll_request_content(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, const gchar *buffer, size_t offset, size_t length, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	
	if (purple_http_response_get_error(response) != NULL) {
		return FALSE;
	}
	
	purple_circular_buffer_append(ha->channel_buffer, buffer, length);
	
	hangouts_process_channel_buffer(ha);
	
	return TRUE;
}

static void
hangouts_longpoll_request_closed(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	
	if (!PURPLE_IS_CONNECTION(purple_http_conn_get_purple_connection(http_conn))) {
		return;
	}
	
	purple_circular_buffer_destroy(ha->channel_buffer);
	ha->channel_buffer = purple_circular_buffer_new(0);
	
	if (purple_http_response_get_error(response) != NULL) {
		//TODO error checking
		hangouts_fetch_channel_sid(ha);
	} else {
		hangouts_longpoll_request(ha);
	}
}

void
hangouts_longpoll_request(HangoutsAccount *ha)
{
	PurpleHttpRequest *request;
	GString *url;

	
	url = g_string_new(HANGOUTS_CHANNEL_URL_PREFIX "channel/bind" "?");
	g_string_append(url, "VER=8&");           // channel protocol version
	g_string_append_printf(url, "gsessionid=%s&", purple_url_encode(ha->gsessionid_param));
	g_string_append(url, "RID=rpc&");         // request identifier
	g_string_append(url, "t=1&");             // trial
	g_string_append_printf(url, "SID=%s&", purple_url_encode(ha->sid_param));  // session ID
	g_string_append(url, "CI=0&");            // 0 if streaming/chunked requests should be used
	g_string_append(url, "ctype=hangouts&");  // client type
	g_string_append(url, "TYPE=xmlhttp&");    // type of request
	
	request = purple_http_request_new(NULL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_url(request, url->str);
	purple_http_request_set_timeout(request, -1);  // to infinity and beyond!
	purple_http_request_set_response_writer(request, hangouts_longpoll_request_content, ha);
	purple_http_request_set_keepalive_pool(request, ha->channel_keepalive_pool);
	
	hangouts_set_auth_headers(ha, request);
	
	purple_http_request(ha->pc, request, hangouts_longpoll_request_closed, ha);
	
	g_string_free(url, TRUE);
}



static void
hangouts_send_maps_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	/*111
	 * [
	 * [0,["c","<sid>","",8]],
	 * [1,[{"gsid":"<gsid>"}]]
	 * ]
	 */
	JsonNode *node;
	HangoutsAccount *ha = user_data;
	const gchar *res_raw;
	size_t res_len;
	gchar *gsid;
	gchar *sid;
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, purple_http_response_get_error(response));
		return;
	}

	res_raw = purple_http_response_get_data(response, &res_len);
	res_raw = g_strstr_len(res_raw, res_len, "\n");
	res_raw++;
	node = json_decode(res_raw, -1);
	sid = hangouts_json_path_query_string(node, "$[0][1][1]", NULL);
	gsid = hangouts_json_path_query_string(node, "$[1][1][0].gsid", NULL);

	if (sid != NULL) {
		g_free(ha->sid_param);
		ha->sid_param = sid;
	}
	if (gsid != NULL) {
		g_free(ha->gsessionid_param);
		ha->gsessionid_param = gsid;
	}

	json_node_free (node);
	
	hangouts_longpoll_request(ha);
}

void
hangouts_fetch_channel_sid(HangoutsAccount *ha)
{
	g_free(ha->sid_param);
	g_free(ha->gsessionid_param);
	ha->sid_param = NULL;
	ha->gsessionid_param = NULL;
	
	hangouts_send_maps(ha, NULL, hangouts_send_maps_cb);
}

void
hangouts_send_maps(HangoutsAccount *ha, JsonArray *map_list, PurpleHttpCallback send_maps_callback)
{
	PurpleHttpRequest *request;
	GString *url, *postdata;
	guint map_list_len, i;
	
	url = g_string_new(HANGOUTS_CHANNEL_URL_PREFIX "channel/bind" "?");
	g_string_append(url, "VER=8&");           // channel protocol version
	g_string_append(url, "RID=81188&");       // request identifier
	g_string_append(url, "ctype=hangouts&");  // client type
	if (ha->gsessionid_param)
		g_string_append_printf(url, "gsessionid=%s&", purple_url_encode(ha->gsessionid_param));
	if (ha->sid_param)
		g_string_append_printf(url, "SID=%s&", purple_url_encode(ha->sid_param));  // session ID
	
	request = purple_http_request_new(NULL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_url(request, url->str);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded");
	
	hangouts_set_auth_headers(ha, request);
	
	postdata = g_string_new(NULL);
	if (map_list != NULL) {
		map_list_len = json_array_get_length(map_list);
		g_string_append_printf(postdata, "count=%d&", map_list_len);
		g_string_append(postdata, "ofs=0&");
		for(i = 0; i < map_list_len; i++) {
			JsonObject *obj = json_array_get_object_element(map_list, i);
			GList *members = json_object_get_members(obj);
			
			for (; members != NULL; members = members->next) {
				const gchar *member_name = members->data;
				JsonNode *value = json_object_get_member(obj, member_name);
				
				g_string_append_printf(postdata, "req%u_%s=", i, purple_url_encode(member_name));
				g_string_append_printf(postdata, "%s&", purple_url_encode(json_node_get_string(value)));
			}
		}
	}
	purple_http_request_set_contents(request, postdata->str, postdata->len);

	purple_http_request(ha->pc, request, send_maps_callback, ha);
	
	g_string_free(postdata, TRUE);
	g_string_free(url, TRUE);	
}

void
hangouts_add_channel_services(HangoutsAccount *ha)
{
	JsonArray *map_list = json_array_new();
	JsonObject *obj;
	
	// TODO Work out what this is for
	obj = json_object_new();
	json_object_set_string_member(obj, "p", "{\"3\":{\"1\":{\"1\":\"tango_service\"}}}");
	json_array_add_object_element(map_list, obj);
	
	// This is for the chat messages
	obj = json_object_new();
	json_object_set_string_member(obj, "p", "{\"3\":{\"1\":{\"1\":\"babel\"}}}");
	json_array_add_object_element(map_list, obj);
	
	// TODO Work out what this is for
	obj = json_object_new();
	json_object_set_string_member(obj, "p", "{\"3\":{\"1\":{\"1\":\"hangout_invite\"}}}");
	json_array_add_object_element(map_list, obj);
	
	hangouts_send_maps(ha, map_list, NULL);
	
	json_array_unref(map_list);
}


typedef struct {
	HangoutsAccount *ha;
	HangoutsPbliteResponseFunc callback;
	ProtobufCMessage *response_message;
	gpointer user_data;
} LazyPblistRequestStore;

static void
hangouts_pblite_request_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	LazyPblistRequestStore *request_info = user_data;
	HangoutsAccount *ha = request_info->ha;
	HangoutsPbliteResponseFunc callback = request_info->callback;
	gpointer real_user_data = request_info->user_data;
	ProtobufCMessage *response_message = request_info->response_message;
	ProtobufCMessage *unpacked_message;
	const gchar *raw_response;
	guchar *decoded_response;
	gsize response_len;
	const gchar *content_type;
	
	if (purple_http_response_get_error(response) != NULL) {
		g_free(request_info);
		g_free(response_message);
		return; //TODO should we send NULL to the callee?
	}
	
	if (callback != NULL) {
		raw_response = purple_http_response_get_data(response, NULL);
		
		content_type = purple_http_response_get_header(response, "X-Goog-Safety-Content-Type");
		if (g_strcmp0(content_type, "application/x-protobuf") == 0) {
			decoded_response = purple_base64_decode(raw_response, &response_len);
			unpacked_message = protobuf_c_message_unpack(response_message->descriptor, NULL, response_len, decoded_response);
			
			if (unpacked_message != NULL) {
				callback(ha, unpacked_message, real_user_data);
				protobuf_c_message_free_unpacked(unpacked_message, NULL);
			} else {
				purple_debug_error("hangouts", "Error decoding protobuf!\n");
			}
		} else {
			gchar *tidied_json = hangouts_json_tidy_blank_arrays(raw_response);
			JsonArray *response_array = json_decode_array(tidied_json, -1);
			
			pblite_decode(response_message, response_array, /*Ignore First Item= */TRUE);
			purple_debug_info("hangouts", "A '%s' says '%s'\n", response_message->descriptor->name, json_array_get_string_element(response_array, 0));
			callback(ha, response_message, real_user_data);
			
			json_array_unref(response_array);
			g_free(tidied_json);
		}
	}
	
	g_free(request_info);
	g_free(response_message);
}

void
hangouts_pblite_request(HangoutsAccount *ha, const gchar *endpoint, ProtobufCMessage *request_message, HangoutsPbliteResponseFunc callback, ProtobufCMessage *response_message, gpointer user_data)
{
	PurpleHttpRequest *request;
	gsize request_len;
	gchar *request_data;
	LazyPblistRequestStore *request_info = g_new0(LazyPblistRequestStore, 1);
	
	request_len = protobuf_c_message_get_packed_size(request_message);
	request_data = g_new0(gchar, request_len);
	protobuf_c_message_pack(request_message, (guchar *)request_data);
	
	request = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(request, "https://clients6.google.com/chat/v1/%s?alt=protojson", endpoint);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "Content-Type", "application/x-protobuf");
	purple_http_request_header_set(request, "X-Goog-Encode-Response-If-Executable", "base64");
	purple_http_request_set_contents(request, request_data, request_len);
	
	request_info->ha = ha;
	request_info->callback = callback;
	request_info->response_message = response_message;
	request_info->user_data = user_data;
	
	
	hangouts_set_auth_headers(ha, request);
	
	purple_http_request(ha->pc, request, hangouts_pblite_request_cb, request_info);
	
	g_free(request_data);
}


void
hangouts_pblite_send_chat_message(HangoutsAccount *ha, SendChatMessageRequest *request, HangoutsPbliteChatMessageResponseFunc callback, gpointer user_data)
{
	SendChatMessageResponse *response = g_new0(SendChatMessageResponse, 1);
	
	send_chat_message_response__init(response);
	hangouts_pblite_request(ha, "conversations/sendchatmessage", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}

void
hangouts_pblite_set_typing(HangoutsAccount *ha, SetTypingRequest *request, HangoutsPbliteSetTypingResponseFunc callback, gpointer user_data)
{
	SetTypingResponse *response = g_new0(SetTypingResponse, 1);
	
	set_typing_response__init(response);
	hangouts_pblite_request(ha, "conversations/settyping", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}

void
hangouts_pblite_get_self_info(HangoutsAccount *ha, GetSelfInfoRequest *request, HangoutsPbliteSelfInfoResponseFunc callback, gpointer user_data)
{
	GetSelfInfoResponse *response = g_new0(GetSelfInfoResponse, 1);
	
	get_self_info_response__init(response);
	hangouts_pblite_request(ha, "contacts/getselfinfo", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}

void
hangouts_pblite_get_recent_conversations(HangoutsAccount *ha, SyncRecentConversationsRequest *request, HangoutsPbliteRecentConversationsResponseFunc callback, gpointer user_data)
{
	SyncRecentConversationsResponse *response = g_new0(SyncRecentConversationsResponse, 1);
	
	sync_recent_conversations_response__init(response);
	hangouts_pblite_request(ha, "conversations/syncrecentconversations", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}

void
hangouts_pblite_query_presence(HangoutsAccount *ha, QueryPresenceRequest *request, HangoutsPbliteQueryPresenceResponseFunc callback, gpointer user_data)
{
	QueryPresenceResponse *response = g_new0(QueryPresenceResponse, 1);
	
	query_presence_response__init(response);
	hangouts_pblite_request(ha, "presence/querypresence", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}

void
hangouts_pblite_get_conversation(HangoutsAccount *ha, GetConversationRequest *request, HangoutsPbliteGetConversationResponseFunc callback, gpointer user_data)
{
	GetConversationResponse *response = g_new0(GetConversationResponse, 1);
	
	get_conversation_response__init(response);
	hangouts_pblite_request(ha, "conversations/getconversation", (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);
}