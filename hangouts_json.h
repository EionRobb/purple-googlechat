
#ifndef _HANGOUTS_JSON_H_
#define _HANGOUTS_JSON_H_

#include <glib.h>
#include <json-glib/json-glib.h>


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
 * Returns a string of the encoded JSON object.
 *
 * \param node
 *      The node of data to encode
 * \param len (optional, out)
 *      Returns the length of the encoded string
 * \return
 *      The JSON string or NULL if there was an error.  You are required to g_free() this when you are done.
 */
gchar *json_encode(JsonNode *node, gsize *len);
gchar *json_pretty_encode(JsonNode *node, gsize *len);

gchar *json_encode_object(JsonObject *object, gsize *len);
gchar *json_encode_array(JsonArray *array, gsize *len);

/**
 * Decode a JSON string into a JsonNode object
 *
 * \param data
 *      The JSON-encoded string to decode.
 * \param len
 *      The length of the string, or -1 if the string is NUL-terminated.
 * \return
 *      The JsonNode object or NULL if there was an error.
 */
JsonNode *json_decode(const gchar *data, gssize len);

JsonArray *json_decode_array(const gchar *data, gssize len);
JsonObject *json_decode_object(const gchar *data, gssize len);


JsonNode *hangouts_json_path_query(JsonNode *root, const gchar *expr, GError **error);

gchar *hangouts_json_path_query_string(JsonNode *root, const gchar *expr, GError **error);

gint64 hangouts_json_path_query_int(JsonNode *root, const gchar *expr, GError **error);


/**
 * Tidies invalid JSON from Google into slightly-more-valid JSON, 
 * so that it can be parsed by json-glib
 * e.g. turns [,,,,,] into [null,null,null,null,null]
 *
 * \param json
 *      The JSON-encoded string to clean up.
 * \return
 *      The JSON-encoded string that has been tidied or NULL if there was an error.  You are required to g_free() this when you are done.
 */
gchar *hangouts_json_tidy_blank_arrays(const gchar *json);

#endif /* _HANGOUTS_JSON_H_ */
