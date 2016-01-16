#define PURPLE_PLUGINS


#include "hangouts_connection.h"


#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"


void
hangouts_process_data_chunks(const gchar *data, gsize len)
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
				//TODO set the client_id on the connection
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
				hangouts_process_data_chunks(chunk, len);
			}
			
			g_free(chunk);
			
			lenpos = 0;
		} else {
			lenpos = lenpos + 1;
		}
	}
}

