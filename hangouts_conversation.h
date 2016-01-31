
#ifndef _HANGOUTS_CONVERSATION_H_
#define _HANGOUTS_CONVERSATION_H_

#include "libhangouts.h"
#include "conversation.h"
#include "connection.h"

GHashTable *hangouts_chat_info_defaults(PurpleConnection *pc, const char *chatname);

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

#endif /*_HANGOUTS_CONVERSATION_H_*/