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


#ifndef _GOOGLECHAT_EVENTS_H_
#define _GOOGLECHAT_EVENTS_H_

#include "libgooglechat.h"
#include "googlechat.pb-c.h"
#include "gmail.pb-c.h"

void googlechat_register_events(gpointer plugin);

void googlechat_received_other_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_event_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_presence_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_typing_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_watermark_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_block_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_view_modification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_delete_notification(PurpleConnection *pc, StateUpdate *state_update);
void googlechat_received_state_update(PurpleConnection *pc, StateUpdate *state_update);

void googlechat_received_gmail_notification(PurpleConnection *pc, const gchar *username, GmailNotification *msg);

void googlechat_process_presence_result(GoogleChatAccount *ha, PresenceResult *presence);
void googlechat_process_conversation_event(GoogleChatAccount *ha, Conversation *conversation, Event *event, gint64 current_server_time);

#endif /*_GOOGLECHAT_EVENTS_H_*/