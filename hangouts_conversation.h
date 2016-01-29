
#ifndef _HANGOUTS_CONVERSATION_H_
#define _HANGOUTS_CONVERSATION_H_

#include "libhangouts.h"
#include "conversation.h"
#include "connection.h"

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

#endif /*_HANGOUTS_CONVERSATION_H_*/