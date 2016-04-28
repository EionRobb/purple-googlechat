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


gboolean hangouts_initiate_media(PurpleAccount *account, const gchar *who, PurpleMediaSessionType type);


#define HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(name, type, url) \
typedef void(* HangoutsPblite##type##ResponseFunc)(HangoutsAccount *ha, type##Response *response, gpointer user_data);\
static inline void \
hangouts_pblite_media_##name(HangoutsAccount *ha, type##Request *request, HangoutsPblite##type##ResponseFunc callback, gpointer user_data)\
{\
	type##Response *response = g_new0(type##Response, 1);\
	\
	name##_response__init(response);\
	hangouts_pblite_request(ha, "/hangouts/v1/" url, (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);\
}

HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(hangout_resolve, HangoutResolve, "hangouts/resolve");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(hangout_query, HangoutQuery, "hangouts/query");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_session_add, MediaSessionAdd, "media_sessions/add");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_session_modify, MediaSessionModify, "media_sessions/modify");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_session_query, MediaSessionQuery, "media_sessions/query");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(hangout_participant_add, HangoutParticipantAdd, "hangout_participants/add");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(hangout_invitation_add, HangoutInvitationAdd, "hangout_invitations/add");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(hangout_bulk, HangoutBulk, "hangouts/bulk");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_source_add, MediaSourceAdd, "media_sources/add");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_stream_add, MediaStreamAdd, "media_streams/add");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_stream_search, MediaStreamSearch, "media_streams/search");
HANGOUTS_DEFINE_PBLITE_MEDIA_REQUEST_FUNC(media_stream_modify, MediaStreamModify, "media_streams/modify");

#endif /*_HANGOUTS_MEDIA_H_*/