
#ifndef _HANGOUTS_CONVERSATION_H_
#define _HANGOUTS_CONVERSATION_H_

#include "libhangouts.h"
#include "conversation.h"
#include "connection.h"

#include "hangouts.pb-c.h"

RequestHeader *hangouts_get_request_header(HangoutsAccount *ha);
void hangouts_request_header_free(RequestHeader *header);

GList *hangouts_chat_info(PurpleConnection *pc);
GHashTable *hangouts_chat_info_defaults(PurpleConnection *pc, const char *chatname);
void hangouts_join_chat(PurpleConnection *pc, GHashTable *data);
gchar *hangouts_get_chat_name(GHashTable *data);

void hangouts_get_all_events(HangoutsAccount *ha, guint64 since_timestamp);
void hangouts_add_conversation_to_blist(HangoutsAccount *ha, Conversation *conversation, GHashTable *unique_user_ids);

void hangouts_get_self_info(HangoutsAccount *ha);
void hangouts_get_conversation_list(HangoutsAccount *ha);
void hangouts_get_buddy_list(HangoutsAccount *ha);

gint hangouts_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
	PurpleMessage *msg
#else
	const gchar *who, const gchar *message, PurpleMessageFlags flags
#endif
);

gint hangouts_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg
#else
const gchar *message, PurpleMessageFlags flags
#endif
);

guint hangouts_send_typing(PurpleConnection *pc, const gchar *name, PurpleIMTypingState state);


void hangouts_get_users_presence(HangoutsAccount *ha, GList *user_ids);
void hangouts_get_users_information(HangoutsAccount *ha, GList *user_ids);

void hangouts_chat_leave_by_conv_id(PurpleConnection *pc, const gchar *conv_id);
void hangouts_chat_leave(PurpleConnection *pc, int id);
void hangouts_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who);
void hangouts_create_conversation(HangoutsAccount *ha, gboolean is_one_to_one, const char *who, const gchar *optional_message);
void hangouts_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata);

void hangouts_mark_conversation_seen(PurpleConversation *conv, PurpleConversationUpdateType type);

void hangouts_set_status(PurpleAccount *account, PurpleStatus *status);

#endif /*_HANGOUTS_CONVERSATION_H_*/