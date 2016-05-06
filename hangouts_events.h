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


#ifndef _HANGOUTS_EVENTS_H_
#define _HANGOUTS_EVENTS_H_

#include "libhangouts.h"
#include "hangouts.pb-c.h"

void hangouts_register_events(gpointer plugin);

void hangouts_received_other_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_event_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_presence_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_typing_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_watermark_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_block_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_view_modification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_delete_notification(PurpleConnection *pc, StateUpdate *state_update);
void hangouts_received_state_update(PurpleConnection *pc, StateUpdate *state_update);


void hangouts_process_presence_result(HangoutsAccount *ha, PresenceResult *presence);
void hangouts_process_conversation_event(HangoutsAccount *ha, Conversation *conversation, Event *event, gint64 current_server_time);

#endif /*_HANGOUTS_EVENTS_H_*/