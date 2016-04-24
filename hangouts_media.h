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


#ifndef _HANGOUTS_MEDIA_H_
#define _HANGOUTS_MEDIA_H_

#include "hangouts_connection.h"
#include "hangout_media.pb-c.h"

typedef void(* HangoutsPbliteResponseFunc)(HangoutsAccount *ha, ProtobufCMessage *response, gpointer user_data);
void hangouts_pblite_media_request(HangoutsAccount *ha, const gchar *endpoint, ProtobufCMessage *request, HangoutsPbliteResponseFunc callback, ProtobufCMessage *response_message, gpointer user_data);


#define HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(name, type, url) \
typedef void(* HangoutsPblite##type##ResponseFunc)(HangoutsAccount *ha, type##Response *response, gpointer user_data);\
static inline void \
hangouts_pblite_media_##name(HangoutsAccount *ha, type##Request *request, HangoutsPblite##type##ResponseFunc callback, gpointer user_data)\
{\
	type##Response *response = g_new0(type##Response, 1);\
	\
	name##_response__init(response);\
	hangouts_pblite_request(ha, "/hangouts/v1/" #url, (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);\
}

HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(hangout_resolve, HangoutResolve, "hangouts/resolve");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(hangout_query, HangoutQuery, "hangouts/query");

#endif /*_HANGOUTS_CONNECTION_H_*/