
#ifndef _HANGOUTS_CONNECTION_H_
#define _HANGOUTS_CONNECTION_H_

#include <glib.h>

#include "purple-socket.h"
#include "http.h"

#include "libhangouts.h"
#include "hangouts_pblite.h"
#include "hangouts.pb-c.h"

#define HANGOUTS_PBLITE_XORIGIN_URL "https://talkgadget.google.com"
#define HANGOUTS_CHANNEL_URL_PREFIX "https://0.client-channel.google.com/client-channel/"

void hangouts_process_data_chunks(HangoutsAccount *ha, const gchar *data, gsize len);

void hangouts_process_channel(int fd);


void hangouts_longpoll_request(HangoutsAccount *ha);
void hangouts_fetch_channel_sid(HangoutsAccount *ha);
void hangouts_send_maps(HangoutsAccount *ha, JsonArray *map_list, PurpleHttpCallback send_maps_callback);
void hangouts_add_channel_services(HangoutsAccount *ha);

void hangouts_default_response_dump(HangoutsAccount *ha, ProtobufCMessage *response, gpointer user_data);
gboolean hangouts_set_active_client(PurpleConnection *pc);
void hangouts_search_users(PurpleProtocolAction *action);

typedef enum {
	HANGOUTS_CONTENT_TYPE_NONE = 0,
	HANGOUTS_CONTENT_TYPE_JSON,
	HANGOUTS_CONTENT_TYPE_PBLITE,
	HANGOUTS_CONTENT_TYPE_PROTOBUF
} HangoutsContentType;
PurpleHttpConnection *hangouts_client6_request(HangoutsAccount *ha, const gchar *path, HangoutsContentType request_type, const gchar *request_data, gssize request_len, HangoutsContentType response_type, PurpleHttpCallback callback, gpointer user_data);

typedef void(* HangoutsPbliteResponseFunc)(HangoutsAccount *ha, ProtobufCMessage *response, gpointer user_data);
void hangouts_pblite_request(HangoutsAccount *ha, const gchar *endpoint, ProtobufCMessage *request, HangoutsPbliteResponseFunc callback, ProtobufCMessage *response_message, gpointer user_data);


#define HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(name, type, url) \
typedef void(* HangoutsPblite##type##ResponseFunc)(HangoutsAccount *ha, type##Response *response, gpointer user_data);\
static inline void \
hangouts_pblite_##name(HangoutsAccount *ha, type##Request *request, HangoutsPblite##type##ResponseFunc callback, gpointer user_data)\
{\
	type##Response *response = g_new0(type##Response, 1);\
	\
	name##_response__init(response);\
	hangouts_pblite_request(ha, (url), (ProtobufCMessage *)request, (HangoutsPbliteResponseFunc)callback, (ProtobufCMessage *)response, user_data);\
}

HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(send_chat_message, SendChatMessage, "conversations/sendchatmessage");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(set_typing, SetTyping, "conversations/settyping");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(get_self_info, GetSelfInfo, "contacts/getselfinfo");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(sync_recent_conversations, SyncRecentConversations, "conversations/syncrecentconversations");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(sync_all_new_events, SyncAllNewEvents, "conversations/syncallnewevents");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(set_presence, SetPresence, "presence/setpresence");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(query_presence, QueryPresence, "presence/querypresence");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(get_conversation, GetConversation, "conversations/getconversation");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(create_conversation, CreateConversation, "conversations/createconversation");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(delete_conversation, DeleteConversation, "conversations/deleteconversation");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(add_user, AddUser, "conversations/adduser");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(remove_user, RemoveUser, "conversations/removeuser");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(update_watermark, UpdateWatermark, "conversations/updatewatermark");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(set_active_client, SetActiveClient, "clients/setactiveclient");
HANGOUTS_DEFINE_PBLITE_REQUEST_FUNC(get_entity_by_id, GetEntityById, "contacts/getentitybyid");

#endif /*_HANGOUTS_CONNECTION_H_*/