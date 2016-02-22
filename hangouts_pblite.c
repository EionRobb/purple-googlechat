#include "hangouts_pblite.h"

#include <stdlib.h>
#include <stdio.h>

static void *glib_protobufc_allocator_alloc(void *allocator_data, size_t size) { return g_try_malloc0(size); };
static void glib_protobufc_allocator_free(void *allocator_data, void *pointer) { g_free(pointer); };

static ProtobufCAllocator glib_protobufc_allocator = {
	.alloc = &glib_protobufc_allocator_alloc,
	.free = &glib_protobufc_allocator_free,
	.allocator_data = NULL,
};

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



#define STRUCT_MEMBER_P(struct_p, struct_offset) \
    ((void *) ((uint8_t *) (struct_p) + (struct_offset)))
#define STRUCT_MEMBER_PTR(member_type, struct_p, struct_offset) \
    ((member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))
#define STRUCT_MEMBER(member_type, struct_p, struct_offset) \
    (*(member_type *) STRUCT_MEMBER_P((struct_p), (struct_offset)))

static gboolean
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
			(void) (glib_protobufc_allocator);
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


static JsonNode *
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
			json_node_set_boolean(node, *member);
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
	field = protobuf_c_message_descriptor_get_field(message->descriptor, index);
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
		
		if (field->label == PROTOBUF_C_LABEL_OPTIONAL && field->quantifier_offset) {
			STRUCT_MEMBER(protobuf_c_boolean, message, field->quantifier_offset) = TRUE;
		}
	}
	
#ifdef DEBUG
	printf("end\n");
#endif
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
#endif
	if (len == 0) {
		return TRUE;
	}
	
	if (JSON_NODE_HOLDS_OBJECT(json_array_get_element(pblite_array, len - 1))) {
#ifdef DEBUG
		printf("ZOMG the last element is an object\n");
#endif
		last_element_is_object = TRUE;
		len = len - 1;
	}
	
	for (i = offset; i < len; i++) {
		//stuff
		JsonNode *value = json_array_get_element(pblite_array, i);
		gboolean success = pblite_decode_element(message, i - offset + 1, value);
		
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
					const protobuf_c_boolean *val = STRUCT_MEMBER_P(message, field_descriptor->quantifier_offset);
					if (!*val)
						encoded_value = json_node_new(JSON_NODE_NULL);
				}
			}
			if (encoded_value == NULL) {
				encoded_value = pblite_encode_field(field_descriptor, field);
			}
		}
		
		if (json_array_get_length(pblite) + 1 == field_descriptor->id) {
			json_array_add_element(pblite, encoded_value);
		} else {
			//need to insert at point [field_descriptor->id - 1] of the array, somehow
			//... so dont, just cheat!
			if (!JSON_NODE_HOLDS_NULL(encoded_value)) {
				gchar *obj_id = g_strdup_printf("%u", field_descriptor->id);
				json_object_set_member(cheats_object, obj_id, encoded_value);
				g_free(obj_id);
			}
		}
	}
	if (json_object_get_size(cheats_object)) {
		json_array_add_object_element(pblite, cheats_object);
	}
	
	return pblite;
}





static JsonObject *pblite_encode_for_json(ProtobufCMessage *message);

static JsonNode *
pblite_encode_field_for_json(const ProtobufCFieldDescriptor *field, gpointer value)
{
	JsonNode *node = NULL;
	
	switch (field->type) {
		case PROTOBUF_C_TYPE_INT32:
		case PROTOBUF_C_TYPE_UINT32:
		case PROTOBUF_C_TYPE_SFIXED32:
		case PROTOBUF_C_TYPE_FIXED32:
		case PROTOBUF_C_TYPE_FLOAT: {
			uint32_t * member = value;
			node = json_node_new(JSON_NODE_VALUE);
			json_node_set_int(node, *member);
			break;
		}
		case PROTOBUF_C_TYPE_ENUM: {
			uint32_t * member = value;
			const ProtobufCEnumDescriptor *enum_descriptor = field->descriptor;
			const ProtobufCEnumValue *enum_value = protobuf_c_enum_descriptor_get_value(enum_descriptor, *member);
			node = json_node_new(JSON_NODE_VALUE);
			if (enum_value == NULL) {
				gchar *unknown_text = g_strdup_printf("UNKNOWN ENUM VALUE %d", *member);
				json_node_set_string(node, unknown_text);
				g_free(unknown_text);
			} else {
				json_node_set_string(node, enum_value->name);
			}
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
			json_node_set_boolean(node, *member);
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
			
			node = json_node_new(JSON_NODE_OBJECT);
			if (pmessage != NULL) {
				json_node_take_object(node, pblite_encode_for_json(*pmessage));
			}
			break;
		}
	}
	
	return node;
}

static JsonObject *
pblite_encode_for_json(ProtobufCMessage *message)
{
	JsonObject *pblite = json_object_new();
	const ProtobufCMessageDescriptor *descriptor = message->descriptor;
	guint i;
	
	for (i = 0; i < descriptor->n_fields; i++) {
		const ProtobufCFieldDescriptor *field_descriptor = descriptor->fields + i;
		void *field = STRUCT_MEMBER_P(message, field_descriptor->offset);
		JsonNode *encoded_value = NULL;
		
		if (field_descriptor->label == PROTOBUF_C_LABEL_REPEATED) {
			guint j;
			size_t siz;
			size_t array_len;
			JsonArray *value_array;
			
			siz = sizeof_elt_in_repeated_array(field_descriptor->type);
			array_len = STRUCT_MEMBER(size_t, message, field_descriptor->quantifier_offset);
			
			value_array = json_array_new();
			for (j = 0; j < array_len; j++) {
				field = STRUCT_MEMBER(void *, message, field_descriptor->offset) + (siz * j);
				json_array_add_element(value_array, pblite_encode_field_for_json(field_descriptor, field));
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
					const protobuf_c_boolean *val = STRUCT_MEMBER_P(message, field_descriptor->quantifier_offset);
					if (!*val)
						encoded_value = json_node_new(JSON_NODE_NULL);
				}
			}
			if (encoded_value == NULL) {
				encoded_value = pblite_encode_field_for_json(field_descriptor, field);
			}
		}
		
		json_object_set_member(pblite, field_descriptor->name, encoded_value);
	}
	
	return pblite;
}


gchar *
pblite_dump_json(ProtobufCMessage *message)
{
	JsonObject *pblite = pblite_encode_for_json(message);
	JsonNode *node = json_node_new(JSON_NODE_OBJECT);
	gchar *json;
	
	json_node_take_object(node, pblite);
	json = json_pretty_encode(node, NULL);
	
	json_node_free(node);
	return json;
}
