/*
 * GoogleChat Plugin for libpurple/Pidgin
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


#ifndef _GOOGLECHAT_CONNECTION_H_
#define _GOOGLECHAT_CONNECTION_H_

#include <glib.h>

#include "http.h"

#include "libgooglechat.h"
#include "googlechat_pblite.h"
#include "googlechat.pb-c.h"

#define GOOGLECHAT_PBLITE_XORIGIN_URL "https://googlechat.google.com"
//#define GOOGLECHAT_PBLITE_XORIGIN_URL "https://talkgadget.google.com"
#define GOOGLECHAT_PBLITE_API_URL "https://clients6.google.com"
//#define GOOGLECHAT_PBLITE_API_URL "https://www.googleapis.com"
#define GOOGLECHAT_CHANNEL_URL_PREFIX "https://0.client-channel.google.com/client-channel/"

void googlechat_process_data_chunks(GoogleChatAccount *ha, const gchar *data, gsize len);

void googlechat_process_channel(int fd);


void googlechat_longpoll_request(GoogleChatAccount *ha);
void googlechat_fetch_channel_sid(GoogleChatAccount *ha);
void googlechat_send_maps(GoogleChatAccount *ha, JsonArray *map_list, PurpleHttpCallback send_maps_callback);
void googlechat_add_channel_services(GoogleChatAccount *ha);

void googlechat_default_response_dump(GoogleChatAccount *ha, ProtobufCMessage *response, gpointer user_data);
gboolean googlechat_set_active_client(PurpleConnection *pc);
void googlechat_search_users(PurpleProtocolAction *action);
void googlechat_search_users_text(GoogleChatAccount *ha, const gchar *text);

typedef enum {
	GOOGLECHAT_CONTENT_TYPE_NONE = 0,
	GOOGLECHAT_CONTENT_TYPE_JSON,
	GOOGLECHAT_CONTENT_TYPE_PBLITE,
	GOOGLECHAT_CONTENT_TYPE_PROTOBUF
} GoogleChatContentType;
PurpleHttpConnection *googlechat_client6_request(GoogleChatAccount *ha, const gchar *path, GoogleChatContentType request_type, const gchar *request_data, gssize request_len, GoogleChatContentType response_type, PurpleHttpCallback callback, gpointer user_data);

typedef void(* GoogleChatPbliteResponseFunc)(GoogleChatAccount *ha, ProtobufCMessage *response, gpointer user_data);
void googlechat_pblite_request(GoogleChatAccount *ha, const gchar *endpoint, ProtobufCMessage *request, GoogleChatPbliteResponseFunc callback, ProtobufCMessage *response_message, gpointer user_data);


#define GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(name, type, url) \
typedef void(* GoogleChatPblite##type##ResponseFunc)(GoogleChatAccount *ha, type##Response *response, gpointer user_data);\
static inline void \
googlechat_pblite_##name(GoogleChatAccount *ha, type##Request *request, GoogleChatPblite##type##ResponseFunc callback, gpointer user_data)\
{\
	type##Response *response = g_new0(type##Response, 1);\
	\
	name##_response__init(response);\
	googlechat_pblite_request(ha, "/chat/v1/" url, (ProtobufCMessage *)request, (GoogleChatPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);\
}

GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(send_chat_message, SendChatMessage, "conversations/sendchatmessage");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(set_typing, SetTyping, "conversations/settyping");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(get_self_info, GetSelfInfo, "contacts/getselfinfo");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(sync_recent_conversations, SyncRecentConversations, "conversations/syncrecentconversations");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(sync_all_new_events, SyncAllNewEvents, "conversations/syncallnewevents");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(set_presence, SetPresence, "presence/setpresence");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(query_presence, QueryPresence, "presence/querypresence");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(get_conversation, GetConversation, "conversations/getconversation");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(create_conversation, CreateConversation, "conversations/createconversation");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(delete_conversation, DeleteConversation, "conversations/deleteconversation");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(rename_conversation, RenameConversation, "conversations/renameconversation");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(modify_conversation_view, ModifyConversationView, "conversations/modifyconversationview");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(add_user, AddUser, "conversations/adduser");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(remove_user, RemoveUser, "conversations/removeuser");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(update_watermark, UpdateWatermark, "conversations/updatewatermark");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(set_focus, SetFocus, "conversations/setfocus");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(set_active_client, SetActiveClient, "clients/setactiveclient");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(get_entity_by_id, GetEntityById, "contacts/getentitybyid");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(get_group_conversation_url, GetGroupConversationUrl, "conversations/getgroupconversationurl");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(set_group_link_sharing_enabled, SetGroupLinkSharingEnabled, "conversations/setgrouplinksharingenabled");
GOOGLECHAT_DEFINE_PBLITE_REQUEST_FUNC(open_group_conversation_from_url, OpenGroupConversationFromUrl, "conversations/opengroupconversationfromurl");

#endif /*_GOOGLECHAT_CONNECTION_H_*/