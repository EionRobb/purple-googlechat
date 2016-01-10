#define PURPLE_PLUGINS

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <protobuf-c.h>

#include "debug.h"
#include "plugin.h"
#include "version.h"

#include "hangouts.pb-c.h"

#define HANGOUTS_PLUGIN_ID "prpl-hangouts"

#ifndef N_
#define N_(a) (a)
#endif

#ifndef JSON_NODE_HOLDS
#define JSON_NODE_HOLDS(node,t)      (json_node_get_node_type ((node)) == (t))
#endif
#ifndef JSON_NODE_HOLDS_VALUE
#define JSON_NODE_HOLDS_VALUE(node)  (JSON_NODE_HOLDS ((node), JSON_NODE_VALUE))
#endif
#ifndef JSON_NODE_HOLDS_OBJECT
#define JSON_NODE_HOLDS_OBJECT(node) (JSON_NODE_HOLDS ((node), JSON_NODE_OBJECT))
#endif
#ifndef JSON_NODE_HOLDS_ARRAY
#define JSON_NODE_HOLDS_ARRAY(node)  (JSON_NODE_HOLDS ((node), JSON_NODE_ARRAY))
#endif
#ifndef JSON_NODE_HOLDS_NULL
#define JSON_NODE_HOLDS_NULL(node)   (JSON_NODE_HOLDS ((node), JSON_NODE_NULL))
#endif


/**
 * Given a field type, return the in-memory size.
 *
 * \todo Implement as a table lookup.
 *
 * \param type
 *      Field type.
 * \return
 *      Size of the field.
 */
static inline size_t
sizeof_elt_in_repeated_array(ProtobufCType type)
{
	switch (type) {
		case PROTOBUF_C_TYPE_SINT32:
		case PROTOBUF_C_TYPE_INT32:
		case PROTOBUF_C_TYPE_UINT32:
		case PROTOBUF_C_TYPE_SFIXED32:
		case PROTOBUF_C_TYPE_FIXED32:
		case PROTOBUF_C_TYPE_FLOAT:
		case PROTOBUF_C_TYPE_ENUM:
			return 4;
		case PROTOBUF_C_TYPE_SINT64:
		case PROTOBUF_C_TYPE_INT64:
		case PROTOBUF_C_TYPE_UINT64:
		case PROTOBUF_C_TYPE_SFIXED64:
		case PROTOBUF_C_TYPE_FIXED64:
		case PROTOBUF_C_TYPE_DOUBLE:
			return 8;
		case PROTOBUF_C_TYPE_BOOL:
			return sizeof(protobuf_c_boolean);
		case PROTOBUF_C_TYPE_STRING:
		case PROTOBUF_C_TYPE_MESSAGE:
			return sizeof(void *);
		case PROTOBUF_C_TYPE_BYTES:
			return sizeof(ProtobufCBinaryData);
	}
	g_return_val_if_reached(0);
	return 0;
}

static void *glib_protobufc_allocator_alloc(void *allocator_data, size_t size) { return g_try_malloc0(size); };
static void glib_protobufc_allocator_free(void *allocator_data, void *pointer) { g_free(pointer); };

static ProtobufCAllocator glib_protobufc_allocator = {
	.alloc = &glib_protobufc_allocator_alloc,
	.free = &glib_protobufc_allocator_free,
	.allocator_data = NULL,
};

gboolean pblite_decode(ProtobufCMessage *message, JsonArray *pblite_array, gboolean ignore_first_item);


#define STRUCT_MEMBER_P(struct_p, struct_offset) \
    ((void *) ((uint8_t *) (struct_p) + (struct_offset)))
#define STRUCT_MEMBER_PTR(member_type, struct_p, struct_offset) \
    ((member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))
#define STRUCT_MEMBER(member_type, struct_p, struct_offset) \
    (*(member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))

void
pblite_decode_field(const ProtobufCFieldDescriptor *field, JsonNode *value, gpointer member)
{
	
	switch (field->type) {
		case PROTOBUF_C_TYPE_INT32:
		case PROTOBUF_C_TYPE_UINT32:
		case PROTOBUF_C_TYPE_SFIXED32:
		case PROTOBUF_C_TYPE_FIXED32:
		case PROTOBUF_C_TYPE_FLOAT:
		case PROTOBUF_C_TYPE_ENUM:
			*(uint32_t *) member = json_node_get_int(value);
			break;
			
		case PROTOBUF_C_TYPE_SINT32:
			*(int32_t *) member = json_node_get_int(value);
			break;
	
		case PROTOBUF_C_TYPE_INT64:
		case PROTOBUF_C_TYPE_UINT64:
		case PROTOBUF_C_TYPE_SFIXED64:
		case PROTOBUF_C_TYPE_FIXED64:
		case PROTOBUF_C_TYPE_DOUBLE:
			*(uint64_t *) member = json_node_get_int(value);
			break;
		
		case PROTOBUF_C_TYPE_SINT64:
			*(int64_t *) member = json_node_get_int(value);
			break;
		
		case PROTOBUF_C_TYPE_BOOL:
			*(protobuf_c_boolean *) member = json_node_get_int(value);
			break;
			
		case PROTOBUF_C_TYPE_STRING: {
			char **pstr = member;

			//TODO - free old data
			// if (maybe_clear && *pstr != NULL) {
				// const char *def = scanned_member->field->default_value;
				// if (*pstr != NULL && *pstr != def)
					// do_free(allocator, *pstr);
			// }
			*pstr = g_strdup(json_node_get_string(value));			
			break;
		}
		case PROTOBUF_C_TYPE_BYTES: {
			ProtobufCBinaryData *bd = member;
			
			//TODO - free old data
			// def_bd = scanned_member->field->default_value;
			// if (maybe_clear &&
				// bd->data != NULL &&
				// (def_bd == NULL || bd->data != def_bd->data))
			// {
				// do_free(allocator, bd->data);
			// }
			bd->data = g_base64_decode(json_node_get_string(value), &bd->len);
		
			break;
		}
		
		case PROTOBUF_C_TYPE_MESSAGE: {
			ProtobufCMessage **pmessage = member;
			const ProtobufCMessageDescriptor *desc = field->descriptor;
			
			*pmessage = g_malloc0(desc->sizeof_message);
			protobuf_c_message_init(desc, *pmessage);

#ifdef DEBUG
			switch(json_node_get_node_type(value)) {
				case JSON_NODE_OBJECT:
					printf("object\n");
					break;
				case JSON_NODE_ARRAY:
					printf("array\n");
					break;
				case JSON_NODE_NULL:
					printf("null\n");
					break;
				case JSON_NODE_VALUE:
					printf("value %s %" G_GINT64_FORMAT "\n", json_node_get_string(value), json_node_get_int(value));
					break;
			}
#endif
			
			pblite_decode(*pmessage, json_node_get_array(value), FALSE);
			break;
		}
	}
}

gboolean
pblite_decode(ProtobufCMessage *message, JsonArray *pblite_array, gboolean ignore_first_item)
{
	const ProtobufCMessageDescriptor *descriptor = message->descriptor;
	guint i, len;
	guint offset = (ignore_first_item ? 1 : 0);
	
	g_return_val_if_fail(descriptor, FALSE);
	
	len = json_array_get_length(pblite_array);
#ifdef DEBUG
	printf("pblite_decode of %s with length %d\n", descriptor->name, len);
#endif
	
	for (i = offset; i < len; i ++) {
		//stuff
		JsonNode *value = json_array_get_element(pblite_array, i);
		const ProtobufCFieldDescriptor *field;
		
		if (JSON_NODE_HOLDS_NULL(value)) {
#ifdef DEBUG
			printf("pblite_decode field %d skipped\n", i - offset);
#endif
			continue;
		}
		
		//g_return_val_if_fail(i - offset < descriptor->n_fields, FALSE);
		//field = descriptor->fields[i - offset];
		field = protobuf_c_message_descriptor_get_field(descriptor, i - offset + 1);
		g_return_val_if_fail(field, FALSE);
#ifdef DEBUG
		printf("pblite_decode field %d is %s\n", i - offset, field->name);
#endif
		
		if (field->label == PROTOBUF_C_LABEL_REPEATED) {
			JsonArray *value_array = json_node_get_array(value);
			guint j, array_len;
			size_t siz;
			//size_t *n_ptr;
			
			array_len = json_array_get_length(value_array);
#ifdef DEBUG
			printf(" which is an array of length %d\n", array_len);
#endif
			
			//see protobuf-c.c:3068
			siz = sizeof_elt_in_repeated_array(field->type);
			//n_ptr = STRUCT_MEMBER_PTR(size_t, message, field.quantifier_offset);
			STRUCT_MEMBER(void *, message, field->offset) = g_malloc0(siz * array_len);
			for (j = 0; j < array_len; j++) {
				pblite_decode_field(field, json_array_get_element(value_array, j), STRUCT_MEMBER_P(message, field->offset + siz * j));
			}
		} else {
			pblite_decode_field(field, value, STRUCT_MEMBER_P(message, field->offset));
		}
	}
	
	//TODO repeate the array loop with an object loop, if the last element of the array is an object
	
	return TRUE;
}

static const gchar *test_string1 = "W1s4ODMsW3sicCI6IntcIjFcIjp7XCIxXCI6e1wiMVwiOntcIjFcIjoxLFwiMlwiOjF9fSxcIjRcIjpcIjE0NTIzMjgxNzMxNDZcIixcIjVcIjpcIlM4NDdcIn0sXCIyXCI6e1wiMVwiOntcIjFcIjpcImJhYmVsXCIsXCIyXCI6XCJjb25zZXJ2ZXIuZ29vZ2xlLmNvbVwifSxcIjJcIjpcIltcXFwiY2J1XFxcIixbW1syLG51bGwsXFxcIjIyOTY4MjM1MDAwMDI1OTEzNDBcXFwiLFtbMF1cXG5dXFxuLDE0NTIzMjgxNzI3ODcwMDAsWzFdXFxuXVxcbixudWxsLFtbW1xcXCJVZ3hHZHBDS19tU3JoQlg4aHJ4NEFhQUJBUVxcXCJdXFxuLFtcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMTQ1MjMyODE3Mjg5NjcyMyxbW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbixcXFwiMTA1MDQ3MTY0XFxcIiwzMF1cXG4sbnVsbCxudWxsLFtudWxsLG51bGwsW1tbMCxcXFwiYnV0IHRvIGxvYWQgZnJvbSBQRU0sIHlvdSBnbyB2aWEgREVSXFxcIl1cXG5dXFxuXVxcbl1cXG4sbnVsbCxudWxsLG51bGwsbnVsbCxcXFwiNy1IMFo3LVI5NVg4OG1TRzlrLUNkQVxcXCIsbnVsbCxudWxsLDEsMiwxLG51bGwsbnVsbCxbMV1cXG4sbnVsbCxudWxsLDEsMTQ1MjMyODE3Mjg5NjcyMyxudWxsLFtcXFwiNy1IMFo3LVI5NVg4OG1TRzlrLUNkQVxcXCIsMjMsbnVsbCwxNDUyMzI4MTcyODk2NzIzXVxcbixudWxsLDJdXFxuXVxcbixudWxsLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxbW1xcXCJVZ3hHZHBDS19tU3JoQlg4aHJ4NEFhQUJBUVxcXCJdXFxuLDEsbnVsbCxbbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLG51bGwsW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMF1cXG4sMiwzMCxbMV1cXG4sW1xcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiLFxcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiXVxcbiwxMzY3NjQ1ODMxNTYyMDAwLDE0NTIzMjgxNzI4OTY3MjMsMTM2NzY0NTgzMTU2MjAwMCxudWxsLG51bGwsW1tbMV1cXG4sMV1cXG5dXFxuXVxcbixudWxsLG51bGwsbnVsbCxbW1tcXFwiMTExNTIzMTUwNjIwMjUwMTY1ODY2XFxcIixcXFwiMTExNTIzMTUwNjIwMjUwMTY1ODY2XFxcIl1cXG4sMF1cXG4sW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sMF1cXG5dXFxuLDAsMiwxLG51bGwsW1tcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIixcXFwiMTEwMTc0MDY2Mzc1MDYxMTE4NzI3XFxcIl1cXG4sW1xcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiLFxcXCIxMTE1MjMxNTA2MjAyNTAxNjU4NjZcXFwiXVxcbl1cXG4sW1tbXFxcIjExMTUyMzE1MDYyMDI1MDE2NTg2NlxcXCIsXFxcIjExMTUyMzE1MDYyMDI1MDE2NTg2NlxcXCJdXFxuLFxcXCJNaWtlIFJ1cHJlY2h0XFxcIiwyLG51bGwsMiwyXVxcbixbW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbixcXFwiRWlvbiBSb2JiXFxcIiwyLG51bGwsMiwyXVxcbl1cXG4sbnVsbCxudWxsLG51bGwsWzFdXFxuLDBdXFxuXVxcbixbWzIsbnVsbCxcXFwiMjI5NjgyMzUwMDAwMjU5MTM0MFxcXCIsW1swXVxcbl1cXG4sMTQ1MjMyODE3Mjc4NzAwMCxbMV1cXG5dXFxuLG51bGwsbnVsbCxudWxsLFtbXFxcIlVneEdkcENLX21TcmhCWDhocng0QWFBQkFRXFxcIl1cXG4sW1xcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiLFxcXCIxMTAxNzQwNjYzNzUwNjExMTg3MjdcXFwiXVxcbiwxNDUyMzI4MTcyNzg3MDAwLDNdXFxuXVxcbixbWzIsbnVsbCxcXFwiMjI5NjgyMzUwMDAwMjU5MTM0MFxcXCIsW1swXVxcbl1cXG4sMTQ1MjMyODE3Mjc4NzAwMCxbMV1cXG5dXFxuLG51bGwsbnVsbCxudWxsLG51bGwsbnVsbCxudWxsLFtbXFxcIjExMDE3NDA2NjM3NTA2MTExODcyN1xcXCIsXFxcIjExMDE3NDA2NjM3NTA2MTExODcyN1xcXCJdXFxuLFtcXFwiVWd4R2RwQ0tfbVNyaEJYOGhyeDRBYUFCQVFcXFwiXVxcbiwxNDUyMzI4MTcyODk2NzIzXVxcbl1cXG5dXFxuXVxcblwifX0ifV1dDQpdDQo=";

static const gchar *test_string2 = "[[924,[\"noop\"]\
]\
]";

static gboolean
plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	HANGOUTS_PLUGIN_ID,                               /**< id             */
	N_("Hangouts"),                           /**< name           */
	"0.1",                                  /**< version        */
	                                                  /**  summary        */
	N_("Hangouts Protocol Plugins."),
	                                                  /**  description    */
	N_("Adds Hangouts protocol support to libpurple."),
	"Eion Robb <eionrobb+hangouts@gmail.com>",             /**< author         */
	"TODO",                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                      /**< prefs_info     */
	NULL,                                             /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

typedef struct {
	long long a;
} Test;

static JsonNode *
json_decode(const gchar *data, gssize len)
{
	JsonParser *parser = json_parser_new();
	JsonNode *root = NULL;
	
	if (!json_parser_load_from_data(parser, data, len, NULL))
	{
		purple_debug_error("hangouts", "Error parsing JSON: %s\n", data);
	} else {
		root = json_node_copy(json_parser_get_root(parser));
		switch(json_node_get_node_type(root)) {
			case JSON_NODE_OBJECT:
				printf("object\n");
				break;
			case JSON_NODE_ARRAY:
				printf("array\n");
				break;
			case JSON_NODE_NULL:
				printf("null\n");
				break;
			case JSON_NODE_VALUE:
				printf("value\n");
				break;
		}
		//printf("hangouts - %d\n", json_node_get_value_type(root));
		//g_object_ref(root);
	}
	g_object_unref(parser);
	
	return root;
}


static void
init_plugin(PurplePlugin *plugin)
{
	(void) glib_protobufc_allocator;
	(void) test_string1;
	gsize len;
	gchar *test_string1_real = (gchar*) g_base64_decode(test_string1, &len);
	
	JsonArray *chunks;
	JsonArray *chunk;
	JsonArray *array;
	JsonNode *array0;
	
	chunks = json_node_get_array(json_decode(test_string2, -1));
	chunks = json_node_get_array(json_decode(test_string1_real, len));
	
	
	//TODO not be lazy
	chunk = json_array_get_array_element(chunks, 0);
	
	array = json_array_get_array_element(chunk, 1);
	array0 = json_array_get_element(array, 0);
	if (JSON_NODE_HOLDS_VALUE(array0)) {
		//probably a nooooop
		if (g_strcmp0(json_node_get_string(array0), "noop") == 0) {
			//A nope ninja delivers a wicked dragon kick
		}
	} else {
		//TODO not be lazy
		const gchar *p = json_object_get_string_member(json_node_get_object(array0), "p");
		JsonObject *wrapper = json_node_get_object(json_decode(p, -1));
		
		if (json_object_has_member(wrapper, "3")) {
			const gchar *new_client_id = json_object_get_string_member(json_object_get_object_member(wrapper, "3"), "2");
			purple_debug_info("hangouts", "Received new client_id: %s\n", new_client_id);
		}
		if (json_object_has_member(wrapper, "2")) {
			const gchar *wrapper22 = json_object_get_string_member(json_object_get_object_member(wrapper, "2"), "2");
			JsonArray *pblite_message = json_node_get_array(json_decode(wrapper22, -1));
			
			if (g_strcmp0(json_array_get_string_element(pblite_message, 0), "cbu") == 0) {
				BatchUpdate batch_update = BATCH_UPDATE__INIT;
				
				pblite_decode((ProtobufCMessage *) &batch_update, pblite_message, TRUE);
			}
			
			json_array_unref(pblite_message);
		}
		
		json_object_unref(wrapper);
	}
	
	json_array_unref(chunks);
	
	exit(0);
}

PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

int main (int argc, const char * argv[])
{
	g_type_init();
	
	init_plugin(NULL);
	return 0;
}
