#include "hangouts_events.h"

#include "debug.h"
#include "glibcompat.h"

#include "hangouts.pb-c.h"

// From hangouts_pblite
gchar *pblite_dump_json(ProtobufCMessage *message);

//purple_signal_emit(purple_connection_get_protocol(ha->pc), "hangouts-received-stateupdate", ha->pc, batch_update.state_update[j]);


void
hangouts_register_events(gpointer plugin)
{
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_typing_notification), NULL);
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_event_notification), NULL);
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_presence_notification), NULL);
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_other_notification), NULL);
}

/*
struct  _StateUpdate
{
  ProtobufCMessage base;
  StateUpdateHeader *state_update_header;
  Conversation *conversation;
  EventNotification *event_notification;
  SetFocusNotification *focus_notification;
  SetTypingNotification *typing_notification;
  SetConversationNotificationLevelNotification *notification_level_notification;
  ReplyToInviteNotification *reply_to_invite_notification;
  WatermarkNotification *watermark_notification;
  ConversationViewModification *view_modification;
  EasterEggNotification *easter_egg_notification;
  SelfPresenceNotification *self_presence_notification;
  DeleteActionNotification *delete_notification;
  PresenceNotification *presence_notification;
  BlockNotification *block_notification;
  SetNotificationSettingNotification *notification_setting_notification;
  RichPresenceEnabledStateNotification *rich_presence_enabled_state_notification;
};*/

void
hangouts_received_other_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	if (state_update->typing_notification != NULL) {
		return;
	}
	
	purple_debug_info("hangouts", "Received new other event %p\n", state_update);
	purple_debug_info("hangouts", "%s\n", pblite_dump_json((ProtobufCMessage *)state_update));
}

/*        "conversation" : {
                "conversation_id" : {
                        "id" : "UgxGdpCK_mSrhBX8hrx4AaABAQ"
                },
                "type" : "CONVERSATION_TYPE_ONE_TO_ONE",
                "name" : null,
                "self_conversation_state" : {
                        "client_generated_id" : null,
                        "self_read_state" : {
                                "participant_id" : {
                                        "gaia_id" : "110174066375061118727",
                                        "chat_id" : "110174066375061118727"
                                },
                                "latest_read_timestamp" : null
                        },
                        "status" : "CONVERSATION_STATUS_ACTIVE",
                        "notification_level" : "NOTIFICATION_LEVEL_RING",
                        "view" : [
                                "CONVERSATION_VIEW_INBOX"
                        ],
                        "inviter_id" : {
                                "gaia_id" : "111523150620250165866",
                                "chat_id" : "111523150620250165866"
                        },
                        "invite_timestamp" : 1367645831562000,
                        "sort_timestamp" : 1453809415517871,
                        "active_timestamp" : 1367645831562000,
                        "delivery_medium_option" : [
                                {
                                        "delivery_medium" : {
                                                "medium_type" : "DELIVERY_MEDIUM_BABEL",
                                                "phone" : null
                                        },
                                        "current_default" : 1
                                }
                        ]
                },
                "read_state" : [
                        {
                                "participant_id" : {
                                        "gaia_id" : "111523150620250165866",
                                        "chat_id" : "111523150620250165866"
                                },
                                "latest_read_timestamp" : null
                        },
                        {
                                "participant_id" : {
                                        "gaia_id" : "110174066375061118727",
                                        "chat_id" : "110174066375061118727"
                                },
                                "latest_read_timestamp" : null
                        }
                ],
                "has_active_hangout" : null,
                "otr_status" : "OFF_THE_RECORD_STATUS_ON_THE_RECORD",
                "otr_toggle" : "OFF_THE_RECORD_TOGGLE_ENABLED",
                "conversation_history_supported" : null,
                "current_participant" : [
                        {
                                "gaia_id" : "110174066375061118727",
                                "chat_id" : "110174066375061118727"
                        },
                        {
                                "gaia_id" : "111523150620250165866",
                                "chat_id" : "111523150620250165866"
                        }
                ],
                "participant_data" : [
                        {
                                "id" : {
                                        "gaia_id" : "111523150620250165866",
                                        "chat_id" : "111523150620250165866"
                                },
                                "fallback_name" : "Mike Ruprecht",
                                "invitation_status" : "INVITATION_STATUS_ACCEPTED",
                                "participant_type" : "PARTICIPANT_TYPE_GAIA",
                                "new_invitation_status" : "INVITATION_STATUS_ACCEPTED"
                        },
                        {
                                "id" : {
                                        "gaia_id" : "110174066375061118727",
                                        "chat_id" : "110174066375061118727"
                                },
                                "fallback_name" : "Eion Robb",
                                "invitation_status" : "INVITATION_STATUS_ACCEPTED",
                                "participant_type" : "PARTICIPANT_TYPE_GAIA",
                                "new_invitation_status" : "INVITATION_STATUS_ACCEPTED"
                        }
                ],
                "network_type" : [
                        "NETWORK_TYPE_BABEL"
                ],
                "force_history_state" : null
        },*/

void
hangouts_process_presence_result(HangoutsAccount *ha, PresenceResult *presence)
{
	const gchar *gaia_id = presence->user_id->gaia_id;
	const gchar *status_id;
	gboolean reachable = FALSE;
	gboolean available = FALSE;
	
	if (presence->presence->reachable) {
		reachable = TRUE;
	}
	if (presence->presence->available) {
		available = TRUE;
	}
	
	if (reachable && available) {
		status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
	} else if (reachable) {
		status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AWAY);
	} else if (available) {
		status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_EXTENDED_AWAY);
	} else {
		status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE);
	}
	
	purple_protocol_got_user_status(ha->account, gaia_id, status_id, NULL);
}

void
hangouts_received_presence_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha;
	PresenceNotification *presence_notification = state_update->presence_notification;
	guint i;
	
	if (presence_notification == NULL) {
		return;
	}
	
	ha = purple_connection_get_protocol_data(pc);
	
	for (i = 0; i < presence_notification->n_presence; i++) {
		hangouts_process_presence_result(ha, presence_notification->presence[i]);
	}
}

void
hangouts_received_event_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha;
	EventNotification *event_notification = state_update->event_notification;
	Conversation *conversation = state_update->conversation;
	Event *event;
	const gchar *gaia_id;
	const gchar *conv_id;
	gint64 current_server_time = state_update->state_update_header->current_server_time;
	gint64 timestamp;
	ChatMessage *chat_message;
	PurpleMessageFlags msg_flags;
	
	if (event_notification == NULL) {
		return;
	}
	
	ha = purple_connection_get_protocol_data(pc);
	
	if (conversation && (conv_id = conversation->conversation_id->id) &&
			!g_hash_table_contains(ha->one_to_ones, conv_id) && 
			!g_hash_table_contains(ha->group_chats, conv_id)) {
		// New conversation we ain't seen before
		// (see also hangouts_got_conversation_list in hangouts_conversation.c)
		if (conversation->type == CONVERSATION_TYPE__CONVERSATION_TYPE_ONE_TO_ONE) {
			const gchar *other_person = conversation->current_participant[0]->gaia_id;
			if (g_strcmp0(other_person, conversation->self_conversation_state->self_read_state->participant_id->gaia_id) == 0) {
				other_person = conversation->current_participant[1]->gaia_id;
			}
			
			g_hash_table_replace(ha->one_to_ones, g_strdup(conv_id), g_strdup(other_person));
			g_hash_table_replace(ha->one_to_ones_rev, g_strdup(other_person), g_strdup(conv_id));
		} else {
			PurpleChatConversation *chatconv;
			g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
			
			chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
			if (!chatconv) {
				guint i;
				
				chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
				purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));

				for (i = 0; i < conversation->n_current_participant; i++) {
					PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
					purple_chat_conversation_add_user(chatconv, conversation->current_participant[i]->gaia_id, NULL, cbflags, FALSE);
				}
			}
		}
	}
	
	event = event_notification->event;
	gaia_id = event->sender_id->gaia_id;
	conv_id = event->conversation_id->id;
	timestamp = event->timestamp;
	chat_message = event->chat_message;
	
	if (ha->self_gaia_id == NULL) {
		ha->self_gaia_id = g_strdup(event->self_event_state->user_id->gaia_id);
	}
	
	if (g_hash_table_remove(ha->sent_message_ids, event->self_event_state->client_generated_id)) {
		// This probably came from us
		return;
	}
	
	if (chat_message != NULL) {
		size_t n_segments = chat_message->message_content->n_segment;
		Segment **segments = chat_message->message_content->segment;
		guint i;
		PurpleXmlNode *html = purple_xmlnode_new("html");
		gchar *msg;
		time_t message_timestamp;

		for (i = 0; i < n_segments; i++) {
			Segment *segment = segments[i];
			Formatting *formatting = segment->formatting;
			PurpleXmlNode *node;
			
			if (segment->type == SEGMENT_TYPE__SEGMENT_TYPE_TEXT) {
				node = purple_xmlnode_new_child(html, "span");
			} else if (segment->type == SEGMENT_TYPE__SEGMENT_TYPE_LINE_BREAK) {
				purple_xmlnode_new_child(html, "br");
				continue;
			} else if (segment->type == SEGMENT_TYPE__SEGMENT_TYPE_LINK) {
				node = purple_xmlnode_new_child(html, "a");
				if (segment->link_data) {
					purple_xmlnode_set_attrib(node, "href", segment->link_data->link_target);
				}
			} else {
				continue;
			}
			
			if (formatting) {
				if (formatting->bold) {
					node = purple_xmlnode_new_child(node, "b");
				}
				if (formatting->italic) {
					node = purple_xmlnode_new_child(node, "i");
				}
				if (formatting->strikethrough) {
					node = purple_xmlnode_new_child(node, "s");
				}
				if (formatting->underline) {
					node = purple_xmlnode_new_child(node, "u");
				}
			}
			
			purple_xmlnode_insert_data(node, segment->text, -1);
		}
		
		msg = purple_xmlnode_to_str(html, NULL);
		message_timestamp = time(NULL) - ((current_server_time - timestamp) / 1000000);
		msg_flags = (g_strcmp0(gaia_id, ha->self_gaia_id) ? PURPLE_MESSAGE_RECV : PURPLE_MESSAGE_SEND);
		
		if (g_hash_table_contains(ha->group_chats, conv_id)) {
			PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
			if (chatconv == NULL) {
				chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
				purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
				if (conversation) {
					guint i;
					for (i = 0; i < conversation->n_current_participant; i++) {
						PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
						purple_chat_conversation_add_user(chatconv, conversation->current_participant[i]->gaia_id, NULL, cbflags, FALSE);
					}
				}
			}
			purple_serv_got_chat_in(pc, g_str_hash(conv_id), gaia_id, msg_flags, msg, message_timestamp);
			
		} else {
			// It's most likely a one-to-one message
			if (msg_flags & PURPLE_MESSAGE_RECV) {
				purple_serv_got_im(pc, gaia_id, msg, msg_flags, message_timestamp);
			} else {
				gaia_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
				if (gaia_id) {
					PurpleIMConversation *imconv = purple_conversations_find_im_with_account(gaia_id, ha->account);
					PurpleMessage *message = purple_message_new_outgoing(gaia_id, msg, msg_flags);
					if (imconv == NULL)
					{
						imconv = purple_im_conversation_new(ha->account, gaia_id);
					}
					purple_message_set_time(message, message_timestamp);
					purple_conversation_write_message(PURPLE_CONVERSATION(imconv), message);
					purple_message_destroy(message);
				}
			}
			
		}
		
		g_free(msg);
		purple_xmlnode_free(html);
	}
	
		/*
        "event_notification" : {
                "event" : {
                        "conversation_id" : {
                                "id" : "UgxGdpCK_mSrhBX8hrx4AaABAQ"
                        },
                        "sender_id" : {
                                "gaia_id" : "111523150620250165866",
                                "chat_id" : "111523150620250165866"
                        },
                        "timestamp" : 1453887705472924,
                        "self_event_state" : {
                                "user_id" : {
                                        "gaia_id" : "110174066375061118727",
                                        "chat_id" : "110174066375061118727"
                                },
                                "client_generated_id" : null,
                                "notification_level" : "NOTIFICATION_LEVEL_RING"
                        },
                        "source_type" : null,
                        "chat_message" : {
                                "annotation" : [
                                ],
                                "message_content" : {
                                        "segment" : [
                                                {
                                                        "type" : "SEGMENT_TYPE_TEXT",
                                                        "text" : "test",
                                                        "formatting" : null,
                                                        "link_data" : null
                                                }
                                        ],
                                        "attachment" : [
                                        ]
                                }
                        },
                        "membership_change" : null,
                        "conversation_rename" : null,
                        "hangout_event" : null,
                        "event_id" : "7-H0Z7-R95X89Vvpn-PwqO",
                        "expiration_timestamp" : null,
                        "otr_modification" : null,
                        "advances_sort_timestamp" : 1,
                        "otr_status" : "OFF_THE_RECORD_STATUS_ON_THE_RECORD",
                        "persisted" : 1,
                        "medium_type" : {
                                "medium_type" : "DELIVERY_MEDIUM_BABEL",
                                "phone" : null
                        },
                        "event_type" : "EVENT_TYPE_REGULAR_CHAT_MESSAGE",
                        "event_version" : 1453887705472924,
                        "hash_modifier" : {
                                "update_id" : "7-H0Z7-R95X89Vvpn-PwqO",
                                "hash_diff" : 24,
                                "version" : 1453887705472924
                        }
                }
        },*/
		
		/*  "message_content" : {
                                        "segment" : [
                                                {
                                                        "type" : "SEGMENT_TYPE_TEXT",
                                                        "text" : "? <",
                                                        "formatting" : null,
                                                        "link_data" : null
                                                },
                                                {
                                                        "type" : "SEGMENT_TYPE_LINK",
                                                        "text" : "http://apps.timwhitlock.info/emoji/tables/unicode#emoji-modal",
                                                        "formatting" : null,
                                                        "link_data" : {
                                                                "link_target" : "http://www.google.com/url?q=http%3A%2F%2Fapps.timwhitlock.info%2Femoji%2Ftables%2Funicode%23emoji-modal&sa=D&sntz=1&usg=AFQjCNEhwmMkBC9SCsgPdvB9UyKprYBGEQ"
                                                        }
                                                },
                                                {
                                                        "type" : "SEGMENT_TYPE_TEXT",
                                                        "text" : ">",
                                                        "formatting" : null,
                                                        "link_data" : null
                                                }
                                        ],
                                        "attachment" : [
                                        ]
                                }*/
}

void
hangouts_received_typing_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha;
	SetTypingNotification *typing_notification = state_update->typing_notification;
	const gchar *gaia_id;
	const gchar *conv_id;
	PurpleIMTypingState typing_state;
	
	if (typing_notification == NULL) {
		return;
	}
	
	ha = purple_connection_get_protocol_data(pc);
	
	purple_debug_info("hangouts", "Received new typing event %p\n", typing_notification);
	//purple_debug_info("hangouts", "%s\n", pblite_dump_json((ProtobufCMessage *)typing_notification)); //leaky
	/* {
        "state_update_header" : {
                "active_client_state" : ACTIVE_CLIENT_STATE_OTHER_ACTIVE,
                "request_trace_id" : "-316846338299410553",
                "notification_settings" : null,
                "current_server_time" : 1453716154770000
        },
        "event_notification" : null,
        "focus_notification" : null,
        "typing_notification" : {
                "conversation_id" : {
                        "id" : "UgxGdpCK_mSrhBX8hrx4AaABAQ"
                },
                "sender_id" : {
                        "gaia_id" : "110174066375061118727",
                        "chat_id" : "110174066375061118727"
                },
                "timestamp" : 1453716154770000,
                "type" : TYPING_TYPE_STARTED
        },
        "notification_level_notification" : null,
        "reply_to_invite_notification" : null,
        "watermark_notification" : null,
        "view_modification" : null,
        "easter_egg_notification" : null,
        "conversation" : null,
        "self_presence_notification" : null,
        "delete_notification" : null,
        "presence_notification" : null,
        "block_notification" : null,
        "notification_setting_notification" : null,
        "rich_presence_enabled_state_notification" : null
}*/
	
	gaia_id = typing_notification->sender_id->gaia_id;
	if (ha->self_gaia_id && g_strcmp0(gaia_id, ha->self_gaia_id) == 0)
		return;
	
	conv_id = typing_notification->conversation_id->id;
	
	if (g_hash_table_contains(ha->group_chats, conv_id)) {
		// This is a group conversation
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		if (FALSE && chatconv != NULL) {
			//TODO make this next line not crash!
			PurpleChatUser *cb = purple_chat_conversation_find_user(chatconv, gaia_id);
			PurpleChatUserFlags cbflags = purple_chat_user_get_flags(cb);
			
			if (typing_notification->type == TYPING_TYPE__TYPING_TYPE_STARTED)
				cbflags |= PURPLE_CHAT_USER_TYPING;
			else
				cbflags &= ~PURPLE_CHAT_USER_TYPING;
			
			purple_chat_user_set_flags(cb, cbflags);
		}
		return;
	}
	
	switch(typing_notification->type) {
		case TYPING_TYPE__TYPING_TYPE_STARTED:
			typing_state = PURPLE_IM_TYPING;
			break;
		
		case TYPING_TYPE__TYPING_TYPE_PAUSED:
			typing_state = PURPLE_IM_TYPED;
			break;
		
		default:
		case TYPING_TYPE__TYPING_TYPE_STOPPED:
			typing_state = PURPLE_IM_NOT_TYPING;
			break;
	}
	
	purple_serv_got_typing(pc, gaia_id, 20, typing_state);
}