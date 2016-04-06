/*
 * Hangouts Plugin for libpurple/Pidgin
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


#ifndef _HANGOUTS_PBLITE_H_
#define _HANGOUTS_PBLITE_H_

#include <protobuf-c/protobuf-c.h>


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
