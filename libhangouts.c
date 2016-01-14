#define PURPLE_PLUGINS

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <protobuf-c.h>

#include "debug.h"
#include "plugin.h"
#include "version.h"
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

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

gchar *json_encode(JsonNode *node, gsize *len);

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
JsonArray *pblite_encode(ProtobufCMessage *message);


#define STRUCT_MEMBER_P(struct_p, struct_offset) \
    ((void *) ((uint8_t *) (struct_p) + (struct_offset)))
#define STRUCT_MEMBER_PTR(member_type, struct_p, struct_offset) \
    ((member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))
#define STRUCT_MEMBER(member_type, struct_p, struct_offset) \
    (*(member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))

gboolean
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
			return TRUE;
			
		case PROTOBUF_C_TYPE_SINT32:
			*(int32_t *) member = json_node_get_int(value);
			return TRUE;
	
		case PROTOBUF_C_TYPE_INT64:
		case PROTOBUF_C_TYPE_UINT64:
		case PROTOBUF_C_TYPE_SFIXED64:
		case PROTOBUF_C_TYPE_FIXED64:
		case PROTOBUF_C_TYPE_DOUBLE:
			*(uint64_t *) member = json_node_get_int(value);
			return TRUE;
		
		case PROTOBUF_C_TYPE_SINT64:
			*(int64_t *) member = json_node_get_int(value);
			return TRUE;
		
		case PROTOBUF_C_TYPE_BOOL:
			*(protobuf_c_boolean *) member = json_node_get_int(value);
			return TRUE;
			
		case PROTOBUF_C_TYPE_STRING: {
			char **pstr = member;

			//TODO - free old data
			// if (maybe_clear && *pstr != NULL) {
				// const char *def = scanned_member->field->default_value;
				// if (*pstr != NULL && *pstr != def)
					// do_free(allocator, *pstr);
			// }
			*pstr = g_strdup(json_node_get_string(value));
			return TRUE;
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
			return TRUE;
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
			
			return pblite_decode(*pmessage, json_node_get_array(value), FALSE);
		}
		
		default: {
			//todo glib fail thing
			return FALSE;
		}
	}
}


JsonNode *
pblite_encode_field(const ProtobufCFieldDescriptor *field, gpointer value)
{
	JsonNode *node = NULL;
	
	switch (field->type) {
		case PROTOBUF_C_TYPE_INT32:
		case PROTOBUF_C_TYPE_UINT32:
		case PROTOBUF_C_TYPE_SFIXED32:
		case PROTOBUF_C_TYPE_FIXED32:
		case PROTOBUF_C_TYPE_FLOAT:
		case PROTOBUF_C_TYPE_ENUM: {
			uint32_t * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
			
		case PROTOBUF_C_TYPE_SINT32: {
			int32_t * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
	
		case PROTOBUF_C_TYPE_INT64:
		case PROTOBUF_C_TYPE_UINT64:
		case PROTOBUF_C_TYPE_SFIXED64:
		case PROTOBUF_C_TYPE_FIXED64:
		case PROTOBUF_C_TYPE_DOUBLE: {
			uint64_t * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
		
		case PROTOBUF_C_TYPE_SINT64: {
			int64_t * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
		
		case PROTOBUF_C_TYPE_BOOL: {
			protobuf_c_boolean * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
			
		case PROTOBUF_C_TYPE_STRING: {
			char **pstr = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_string(node, *pstr);		
			break;
		}
		
		case PROTOBUF_C_TYPE_BYTES: {
			ProtobufCBinaryData *bd = value;
			gchar *b64_data;
			
			b64_data = g_base64_encode(bd->data, bd->len);
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_string(node, b64_data);
			g_free(b64_data);
			break;
		}
		
		case PROTOBUF_C_TYPE_MESSAGE: {
			ProtobufCMessage **pmessage = value;
			
			node = json_node_new(JSON_NODE_ARRAY);
			if (pmessage != NULL) {
				json_node_take_array(node, pblite_encode(*pmessage));
			}
			break;
		}
	}
	
	return node;
}

static gboolean
pblite_decode_element(ProtobufCMessage *message, guint index, JsonNode *value)
{
	const ProtobufCFieldDescriptor *field;
	gboolean success = TRUE;
	
#ifdef DEBUG
	printf("pblite_decode_element field %d ", index);
#endif
	field = protobuf_c_message_descriptor_get_field(message->descriptor, index + 1);
	if (!field) {
#ifdef DEBUG
	printf("skipped\n");
#endif
		return TRUE;
	}
#ifdef DEBUG
	printf("is %s\n", field->name);
#endif
	
	if (JSON_NODE_HOLDS_NULL(value)) {
#ifdef DEBUG
		printf("pblite_decode_element field %d skipped\n", index);
#endif
		if (field->default_value != NULL) {
			*(const void **) STRUCT_MEMBER(void *, message, field->offset) = field->default_value;
		}
		return TRUE;
	}
		
	if (field->label == PROTOBUF_C_LABEL_REPEATED) {
		JsonArray *value_array = json_node_get_array(value);
		guint j;
		size_t siz;
		size_t array_len;
		
		array_len = json_array_get_length(value_array);
#ifdef DEBUG
		printf(" which is an array of length %d\n", array_len);
#endif
		
		//see protobuf-c.c:3068
		siz = sizeof_elt_in_repeated_array(field->type);
		STRUCT_MEMBER(size_t, message, field->quantifier_offset) = array_len;
		STRUCT_MEMBER(void *, message, field->offset) = g_malloc0(siz * array_len);
		
		for (j = 0; j < array_len; j++) {
#ifdef DEBUG
			if (JSON_NODE_HOLDS_NULL(json_array_get_element(value_array, j))) {
				printf("array %d contains null\n", j);
			}
#endif
			success = pblite_decode_field(field, json_array_get_element(value_array, j), STRUCT_MEMBER(void *, message, field->offset) + (siz * j));
			g_return_val_if_fail(success, FALSE);
		}
	} else {
		success = pblite_decode_field(field, value, STRUCT_MEMBER_P(message, field->offset));
		g_return_val_if_fail(success, FALSE);
	}
	
	return TRUE;
}

gboolean
pblite_decode(ProtobufCMessage *message, JsonArray *pblite_array, gboolean ignore_first_item)
{
	const ProtobufCMessageDescriptor *descriptor = message->descriptor;
	guint i, len;
	guint offset = (ignore_first_item ? 1 : 0);
	gboolean last_element_is_object = FALSE;
	
	g_return_val_if_fail(descriptor, FALSE);
	
	len = json_array_get_length(pblite_array);
#ifdef DEBUG
	printf("pblite_decode of %s with length %d\n", descriptor->name, len);
	
	if (JSON_NODE_HOLDS_OBJECT(json_array_get_element(pblite_array, len - 1))) {
		printf("ZOMG the last element is an object\n");
		last_element_is_object = TRUE;
		len = len - 1;
		//exit(-1);
	}
#endif
	
	for (i = offset; i < len; i++) {
		//stuff
		JsonNode *value = json_array_get_element(pblite_array, i);
		gboolean success = pblite_decode_element(message, i - offset, value);
		
		g_return_val_if_fail(success, FALSE);
	}
	
	//Continue on the array with the objects
	if (last_element_is_object) {
		JsonObject *last_object = json_array_get_object_element(pblite_array, len);
		GList *members = json_object_get_members(last_object);
		for (; members != NULL; members = members->next) {
			const gchar *member_name = members->data;
			guint64 member = g_ascii_strtoull(member_name, NULL, 0);
			JsonNode *value = json_object_get_member(last_object, member_name);
			
			gboolean success = pblite_decode_element(message, member - offset, value);
		
			g_return_val_if_fail(success, FALSE);
		}
	}
	
	return TRUE;
}

JsonArray *
pblite_encode(ProtobufCMessage *message)
{
	//Maiku asked for more dinosaurs
	JsonArray *pblite = json_array_new();
	JsonObject *cheats_object = json_object_new();
	const ProtobufCMessageDescriptor *descriptor = message->descriptor;
	guint i;
#ifdef DEBUG
	printf("pblite_encode of %s with length %d\n", descriptor->name, descriptor->n_fields);
#endif
	
	json_array_add_object_element(pblite, cheats_object);
	for (i = 0; i < descriptor->n_fields; i++) {
		const ProtobufCFieldDescriptor *field_descriptor = descriptor->fields + i;
		void *field = STRUCT_MEMBER_P(message, field_descriptor->offset);
		JsonNode *encoded_value = NULL;
		
#ifdef DEBUG
		printf("pblite_encode_element field %d (%d) ", i, field_descriptor->id);
#endif
#ifdef DEBUG
		printf("is %s\n", field_descriptor->name);
#endif
		
		if (field_descriptor->label == PROTOBUF_C_LABEL_REPEATED) {
			guint j;
			size_t siz;
			size_t array_len;
			JsonArray *value_array;
			
			siz = sizeof_elt_in_repeated_array(field_descriptor->type);
			array_len = STRUCT_MEMBER(size_t, message, field_descriptor->quantifier_offset);
			
#ifdef DEBUG
			printf(" which is an array of length %d\n", array_len);
#endif
			
			value_array = json_array_new();
			for (j = 0; j < array_len; j++) {
				field = STRUCT_MEMBER(void *, message, field_descriptor->offset) + (siz * j);
				json_array_add_element(value_array, pblite_encode_field(field_descriptor, field));
			}
			encoded_value = json_node_new(JSON_NODE_ARRAY);
			json_node_take_array(encoded_value, value_array);
		} else {
			if (field_descriptor->label == PROTOBUF_C_LABEL_OPTIONAL) {
				if (field_descriptor->type == PROTOBUF_C_TYPE_MESSAGE || 
				    field_descriptor->type == PROTOBUF_C_TYPE_STRING)
				{
					const void *ptr = *(const void * const *) field;
					if (ptr == NULL || ptr == field_descriptor->default_value)
						encoded_value = json_node_new(JSON_NODE_NULL);
				} else {
					const protobuf_c_boolean *val = field;
					if (!*val)
						encoded_value = json_node_new(JSON_NODE_NULL);
				}
			}
			if (encoded_value == NULL) {
				encoded_value = pblite_encode_field(field_descriptor, field);
			}
		}
		
		//need to insert at point [field_descriptor->id - 1] of the array, somehow
		//json_array_add_element(pblite, encoded_value);
		//... so dont, just cheat!
		if (!JSON_NODE_HOLDS_NULL(encoded_value)) {
			gchar *obj_id = g_strdup_printf("%u", field_descriptor->id - 1);
			json_object_add_member(cheats_object, obj_id, encoded_value);
			g_free(obj_id);
		}
	}
	
	return pblite;
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

gchar *
json_encode(JsonNode *node, gsize *len)
{
	gchar *data;
	JsonGenerator *generator = json_generator_new();
	
	json_generator_set_root(generator, node);
	
	data = json_generator_to_data(generator, len);
	
	g_object_unref(generator);
	
	return data;
}

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
#ifdef DEBUG
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
#endif
		//g_object_ref(root);
	}
	g_object_unref(parser);
	
	return root;
}

static void
hangouts_process_data_chunks(const gchar *data, gsize len)
{
	JsonNode *root;
	JsonArray *chunks;
	guint i, num_chunks;
	
	root = json_decode(data, len);
	if (!JSON_NODE_HOLDS_ARRAY(root)) {
		// That ain't my belly button
		json_node_free(root);
		return;
	}
	chunks = json_node_get_array(root);
	
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
			JsonNode *p_node = json_decode(p, -1);
			JsonObject *wrapper = json_node_get_object(p_node);
			
			if (json_object_has_member(wrapper, "3")) {
				const gchar *new_client_id = json_object_get_string_member(json_object_get_object_member(wrapper, "3"), "2");
				purple_debug_info("hangouts", "Received new client_id: %s\n", new_client_id);
			}
			if (json_object_has_member(wrapper, "2")) {
				const gchar *wrapper22 = json_object_get_string_member(json_object_get_object_member(wrapper, "2"), "2");
				JsonNode *wrapper22_node = json_decode(wrapper22, -1);
				JsonArray *pblite_message;

				if (!JSON_NODE_HOLDS_ARRAY(wrapper22_node)) {
					// That ain't my thumb, neither!
#ifdef DEBUG
					printf("bad wrapper22 %s\n", wrapper22);
#endif
					json_node_free(wrapper22_node);
					json_node_free(p_node);
					continue;
				}
				pblite_message = json_node_get_array(wrapper22_node);
				
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
				
				json_node_free(wrapper22_node);
			}
			
			json_node_free(p_node);
		}
	}
	
	json_node_free(root);
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

static void
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

static void
init_plugin(PurplePlugin *plugin)
{
}
	

PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

#ifdef DEBUG
int main (int argc, const char * argv[])
{
	g_type_init();
	
	(void) glib_protobufc_allocator;
	(void) test_string1;
	gsize len;
	gchar *test_string1_real = (gchar*) g_base64_decode(test_string1, &len);
	
	hangouts_process_data_chunks(test_string2, -1);
	hangouts_process_data_chunks(test_string1_real, len);
	
#ifndef O_BINARY 
#	define O_BINARY 0 
#endif
#ifndef O_RDONLY 
#	define O_RDONLY 0 
#endif
	printf("reading file test-response-1.txt");
	int fd = open("test-response-1.txt", O_RDONLY | O_BINARY);
	hangouts_process_channel(fd);
	close(fd);
	
	return 0;
}
#endif
