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


#ifndef _GOOGLECHAT_CONVERSATION_H_
#define _GOOGLECHAT_CONVERSATION_H_

#include "libgooglechat.h"
#include "conversation.h"
#include "connection.h"

#include "googlechat.pb-c.h"

RequestHeader *googlechat_get_request_header(GoogleChatAccount *ha);
void googlechat_request_header_free(RequestHeader *header);

GList *googlechat_chat_info(PurpleConnection *pc);
GHashTable *googlechat_chat_info_defaults(PurpleConnection *pc, const char *chatname);
void googlechat_join_chat(PurpleConnection *pc, GHashTable *data);
gchar *googlechat_get_chat_name(GHashTable *data);

void googlechat_join_chat_from_url(GoogleChatAccount *ha, const gchar *url);

void googlechat_get_all_events(GoogleChatAccount *ha, guint64 since_timestamp);
void googlechat_add_conversation_to_blist(GoogleChatAccount *ha, Conversation *conversation, GHashTable *unique_user_ids);

void googlechat_get_self_info(GoogleChatAccount *ha);
void googlechat_get_conversation_list(GoogleChatAccount *ha);
void googlechat_get_buddy_list(GoogleChatAccount *ha);

gint googlechat_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
	PurpleMessage *msg
#else
	const gchar *who, const gchar *message, PurpleMessageFlags flags
#endif
);

gint googlechat_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg
#else
const gchar *message, PurpleMessageFlags flags
#endif
);

guint googlechat_send_typing(PurpleConnection *pc, const gchar *name, PurpleIMTypingState state);
guint googlechat_conv_send_typing(PurpleConversation *conv, PurpleIMTypingState state, GoogleChatAccount *ha);


void googlechat_get_users_presence(GoogleChatAccount *ha, GList *user_ids);
void googlechat_get_users_information(GoogleChatAccount *ha, GList *user_ids);
void googlechat_get_info(PurpleConnection *pc, const gchar *who);
gboolean googlechat_poll_buddy_status(gpointer ha_pointer);

void googlechat_chat_leave_by_conv_id(PurpleConnection *pc, const gchar *conv_id, const gchar *who);
void googlechat_chat_leave(PurpleConnection *pc, int id);
void googlechat_chat_kick(PurpleConnection *pc, int id, const char *who);
void googlechat_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who);
void googlechat_create_conversation(GoogleChatAccount *ha, gboolean is_one_to_one, const char *who, const gchar *optional_message);
void googlechat_archive_conversation(GoogleChatAccount *ha, const gchar *conv_id);
void googlechat_rename_conversation(GoogleChatAccount *ha, const gchar *conv_id, const gchar *alias);
void googlechat_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata);

void googlechat_mark_conversation_seen(PurpleConversation *conv, PurpleConversationUpdateType type);

void googlechat_set_status(PurpleAccount *account, PurpleStatus *status);

void googlechat_block_user(PurpleConnection *pc, const char *who);
void googlechat_unblock_user(PurpleConnection *pc, const char *who);

PurpleRoomlist *googlechat_roomlist_get_list(PurpleConnection *pc);

#endif /*_GOOGLECHAT_CONVERSATION_H_*/
