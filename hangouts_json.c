#include "hangouts_json.h"

#include <debug.h>

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

gchar *
json_encode_array(JsonArray *array, gsize *len)
{
	JsonNode *node = json_node_new(JSON_NODE_ARRAY);
	gchar *json;
	
	json_node_set_array(node, array);
	json = json_encode(node, len);
	
	json_node_free(node);
	return json;
}

gchar *
json_encode_object(JsonObject *object, gsize *len)
{
	JsonNode *node = json_node_new(JSON_NODE_OBJECT);
	gchar *json;
	
	json_node_set_object(node, object);
	json = json_encode(node, len);
	
	json_node_free(node);
	return json;
}

JsonNode *
json_decode(const gchar *data, gssize len)
{
	JsonParser *parser = json_parser_new();
	JsonNode *root = NULL;
	
	if (!json_parser_load_from_data(parser, data, len, NULL))
	{
		purple_debug_error("hangouts", "Error parsing JSON: %s\n", data);
	} else {
		root = json_node_copy(json_parser_get_root(parser));
	}
	g_object_unref(parser);
	
	return root;
}

JsonArray *
json_decode_array(const gchar *data, gssize len)
{
	JsonNode *root = json_decode(data, len);
	
	g_return_val_if_fail(root, NULL);
	
	if (!JSON_NODE_HOLDS_ARRAY(root)) {
		// That ain't my belly button!
		json_node_free(root);
		return NULL;
	}
	
	return json_node_get_array(root);
}

JsonObject *
json_decode_object(const gchar *data, gssize len)
{
	JsonNode *root = json_decode(data, len);
	
	g_return_val_if_fail(root, NULL);
	
	if (!JSON_NODE_HOLDS_OBJECT(root)) {
		// That ain't my thumb, neither!
		json_node_free(root);
		return NULL;
	}
	
	return json_node_get_object(root);
}
