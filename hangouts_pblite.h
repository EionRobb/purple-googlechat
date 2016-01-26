
#ifndef _HANGOUTS_PBLITE_H_
#define _HANGOUTS_PBLITE_H_

#include <protobuf-c.h>


#include "hangouts_json.h"
#include "hangouts.pb-c.h"

/**
 * Decode a JsonArray into a ProtobufCMessage.
 *
 * \param message
 *      The message type to store as.  Is also used as the output.
 * \param pblite_array
 *      The JSON array of data to parse.
 * \param ignore_first_item
 *      Whether to skip the first item in the array (sometimes it's garbage data to identify type).
 * \return
 *      TRUE if successful, FALSE if there was an error.
 */
gboolean pblite_decode(ProtobufCMessage *message, JsonArray *pblite_array, gboolean ignore_first_item);

/**
 * Encodes a ProtobufCMessage into a JsonArray.
 *
 * \param message
 *      The message to parse into the equivalent pblite array.
 * \return
 *      The JSON array of data, ready to be sent.
 */
JsonArray *pblite_encode(ProtobufCMessage *message);


gchar *pblite_dump_json(ProtobufCMessage *message);

#endif /* _HANGOUTS_PBLITE_H_ */
