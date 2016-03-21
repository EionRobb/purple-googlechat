#include "hangouts_events.h"

#include "debug.h"
#include "glibcompat.h"
#include "image.h"
#include "image-store.h"

#include "hangouts_conversation.h"
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
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_watermark_notification), NULL);
	purple_signal_connect(plugin, "hangouts-received-stateupdate", plugin, PURPLE_CALLBACK(hangouts_received_state_update), NULL);
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
hangouts_received_state_update(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	
	if (ha != NULL && state_update->state_update_header != NULL) {
		gint64 current_server_time = state_update->state_update_header->current_server_time;
		
		ha->active_client_state = state_update->state_update_header->active_client_state;
		
		// libpurple can't store a 64bit int on a 32bit machine, so convert to something more usable instead (puke)
		//  also needs to work cross platform, in case the accounts.xml is being shared (double puke)
		purple_account_set_int(ha->account, "last_event_timestamp_high", current_server_time >> 32);
		purple_account_set_int(ha->account, "last_event_timestamp_low", current_server_time & 0xFFFFFFFF);
	}
}

void
hangouts_received_other_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	if (state_update->typing_notification != NULL ||
		state_update->presence_notification != NULL ||
		state_update->event_notification != NULL ||
		state_update->watermark_notification != NULL) {
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
hangouts_received_watermark_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha;
	WatermarkNotification *watermark_notification = state_update->watermark_notification;
	
	if (watermark_notification == NULL) {
		return;
	}
	
	ha = purple_connection_get_protocol_data(pc);
	
	if (FALSE && g_strcmp0(watermark_notification->sender_id->gaia_id, ha->self_gaia_id)) {
		//We marked this message as read ourselves
		PurpleConversation *conv = NULL;
		const gchar *conv_id = watermark_notification->conversation_id->id;
		gint64 *last_read_timestamp_ptr;
		gint64 latest_read_timestamp;
		
		if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
			conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(g_hash_table_lookup(ha->one_to_ones, conv_id), ha->account));
		} else if (g_hash_table_contains(ha->group_chats, conv_id)) {
			conv = PURPLE_CONVERSATION(purple_conversations_find_chat_with_account(conv_id, ha->account));
		} else {
			// Unknown conversation!
			return;
		}
		if (conv == NULL) {
			return;
		}
		
		latest_read_timestamp = watermark_notification->latest_read_timestamp;
		last_read_timestamp_ptr = (gint64 *)purple_conversation_get_data(conv, "last_read_timestamp");
		if (last_read_timestamp_ptr == NULL) {
			last_read_timestamp_ptr = g_new0(gint64, 1);
		}
		if (latest_read_timestamp > *last_read_timestamp_ptr) {
			*last_read_timestamp_ptr = watermark_notification->latest_read_timestamp;
			purple_conversation_set_data(conv, "last_read_timestamp", last_read_timestamp_ptr);
		}
	}
}
	
void
hangouts_process_presence_result(HangoutsAccount *ha, PresenceResult *presence)
{
	const gchar *gaia_id = presence->user_id->gaia_id;
	const gchar *status_id = NULL;
	const gchar *conv_id = g_hash_table_lookup(ha->one_to_ones_rev, gaia_id);
	gboolean reachable = FALSE;
	gboolean available = FALSE;
	gchar *message = NULL;
	PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, gaia_id);
	
	if (buddy != NULL) {
		status_id = purple_status_get_id(purple_presence_get_active_status(purple_buddy_get_presence(buddy)));
	}
	
	if (g_strcmp0(status_id, "mobile") == 0 || (conv_id != NULL && g_hash_table_contains(ha->google_voice_conversations, conv_id))) {
		// SMS contacts normally appear as 'offline'
		status_id = "mobile";
	} else if (presence->presence->has_reachable || presence->presence->has_available) {
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
	} else if (buddy == NULL) {
		return;
	}
	
	if (presence->presence->mood_setting) {
		MoodMessage *mood_message = presence->presence->mood_setting->mood_message;
		MoodContent *mood_content = mood_message ? mood_message->mood_content : NULL;
		size_t n_segments;
		Segment **segments;
		GString *message_str;
		guint i;

		if (mood_content != NULL && mood_content->n_segment) {
			n_segments = mood_content->n_segment;
			segments = mood_content->segment;
			message_str = g_string_new(NULL);
			
			for (i = 0; i < n_segments; i++) {
				Segment *segment = segments[i];
				if (segment->type == SEGMENT_TYPE__SEGMENT_TYPE_TEXT) {
					g_string_append(message_str, segment->text);
					g_string_append_c(message_str, ' ');
				}
			}
			message = g_string_free(message_str, FALSE);
		}
	}
	
	if (message != NULL) {
		purple_protocol_got_user_status(ha->account, gaia_id, status_id, "message", message, NULL);
	} else {
		purple_protocol_got_user_status(ha->account, gaia_id, status_id, NULL);
	}
	
	g_free(message);
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

static void
hangouts_got_http_image_for_conv(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	const gchar *url;
	const gchar *gaia_id;
	const gchar *conv_id;
	PurpleMessageFlags msg_flags;
	time_t message_timestamp;
	PurpleImage *image;
	const gchar *response_data;
	size_t response_size;
	guint image_id;
	gchar *msg;
	
	if (purple_http_response_get_error(response) != NULL) {
		g_dataset_destroy(connection);
		return;
	}
	
	url = g_dataset_get_data(connection, "url");
	gaia_id = g_dataset_get_data(connection, "gaia_id");
	conv_id = g_dataset_get_data(connection, "conv_id");
	msg_flags = GPOINTER_TO_INT(g_dataset_get_data(connection, "msg_flags"));
	message_timestamp = GPOINTER_TO_INT(g_dataset_get_data(connection, "message_timestamp"));
	
	response_data = purple_http_response_get_data(response, &response_size);
	image = purple_image_new_from_data(g_memdup(response_data, response_size), response_size);
	image_id = purple_image_store_add(image);
	msg = g_strdup_printf("<a href='%s'><img id='%d' /></a>", url, image_id);
	msg_flags |= PURPLE_MESSAGE_IMAGES;
		
	if (g_hash_table_contains(ha->group_chats, conv_id)) {
		purple_serv_got_chat_in(ha->pc, g_str_hash(conv_id), gaia_id, msg_flags, msg, message_timestamp);
	} else {
		if (msg_flags & PURPLE_MESSAGE_RECV) {
			purple_serv_got_im(ha->pc, gaia_id, msg, msg_flags, message_timestamp);
		} else {
			gaia_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
			if (gaia_id) {
				PurpleIMConversation *imconv = purple_conversations_find_im_with_account(gaia_id, ha->account);
				PurpleMessage *message = purple_message_new_outgoing(gaia_id, msg, msg_flags);
				if (imconv == NULL) {
					imconv = purple_im_conversation_new(ha->account, gaia_id);
				}
				purple_message_set_time(message, message_timestamp);
				purple_conversation_write_message(PURPLE_CONVERSATION(imconv), message);
				purple_message_destroy(message);
			}
		}
	}
	
	g_free(msg);
	g_dataset_destroy(connection);
}

void
hangouts_received_event_notification(PurpleConnection *pc, StateUpdate *state_update)
{
	HangoutsAccount *ha;
	EventNotification *event_notification = state_update->event_notification;
	Conversation *conversation = state_update->conversation;
	Event *event;
	gint64 current_server_time = state_update->state_update_header->current_server_time;
	
	if (event_notification == NULL) {
		return;
	}
	
	ha = purple_connection_get_protocol_data(pc);
	
	event = event_notification->event;
	if (ha->self_gaia_id == NULL) {
		ha->self_gaia_id = g_strdup(event->self_event_state->user_id->gaia_id);
		purple_connection_set_display_name(pc, ha->self_gaia_id);
	}
	
	hangouts_process_conversation_event(ha, conversation, event, current_server_time);
}

void
hangouts_process_conversation_event(HangoutsAccount *ha, Conversation *conversation, Event *event, gint64 current_server_time)
{
	PurpleConnection *pc = ha->pc;
	const gchar *gaia_id;
	const gchar *conv_id;
	gint64 timestamp;
	const gchar *client_generated_id;
	ChatMessage *chat_message;
	PurpleMessageFlags msg_flags;
	
	if (conversation && (conv_id = conversation->conversation_id->id) &&
			!g_hash_table_contains(ha->one_to_ones, conv_id) && 
			!g_hash_table_contains(ha->group_chats, conv_id)) {
		// New conversation we ain't seen before
		
		hangouts_add_conversation_to_blist(ha, conversation, NULL);
	}
	
	gaia_id = event->sender_id->gaia_id;
	conv_id = event->conversation_id->id;
	timestamp = event->timestamp;
	chat_message = event->chat_message;
	client_generated_id = event->self_event_state->client_generated_id;
	
	if (client_generated_id && g_hash_table_remove(ha->sent_message_ids, client_generated_id)) {
		// This probably came from us
		return;
	}
	
	if (event->membership_change != NULL) {
		//event->event_type == EVENT_TYPE__EVENT_TYPE_REMOVE_USER || EVENT_TYPE__EVENT_TYPE_ADD_USER
		MembershipChange *membership_change = event->membership_change;
		guint i;
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		
		if (chatconv != NULL) {
			for (i = 0; i < membership_change->n_participant_ids; i++) {
				ParticipantId *participant_id = membership_change->participant_ids[i];
				
				if (membership_change->type == MEMBERSHIP_CHANGE_TYPE__MEMBERSHIP_CHANGE_TYPE_LEAVE) {
					purple_chat_conversation_remove_user(chatconv, participant_id->gaia_id, NULL);
					if (g_strcmp0(participant_id->gaia_id, ha->self_gaia_id) == 0) {
						purple_serv_got_chat_left(ha->pc, g_str_hash(conv_id));
						g_hash_table_remove(ha->group_chats, conv_id);
						purple_blist_remove_chat(purple_blist_find_chat(ha->account, conv_id));
					}
				} else {
					PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
					purple_chat_conversation_add_user(chatconv, participant_id->gaia_id, NULL, cbflags, TRUE);
				}
			}
		}
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
				if (formatting->italics) {
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
			
			if (purple_conversation_has_focus(PURPLE_CONVERSATION(chatconv))) {
				hangouts_mark_conversation_seen(PURPLE_CONVERSATION(chatconv), PURPLE_CONVERSATION_UPDATE_UNSEEN);
			}
		} else {
			PurpleIMConversation *imconv = NULL;
			// It's most likely a one-to-one message
			if (msg_flags & PURPLE_MESSAGE_RECV) {
				purple_serv_got_im(pc, gaia_id, msg, msg_flags, message_timestamp);
			} else {
				gaia_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
				if (gaia_id) {
					imconv = purple_conversations_find_im_with_account(gaia_id, ha->account);
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
			
			if (imconv == NULL) {
				imconv = purple_conversations_find_im_with_account(gaia_id, ha->account);
			}
			if (purple_conversation_has_focus(PURPLE_CONVERSATION(imconv))) {
				hangouts_mark_conversation_seen(PURPLE_CONVERSATION(imconv), PURPLE_CONVERSATION_UPDATE_UNSEEN);
			}
		}
		
		g_free(msg);
		purple_xmlnode_free(html);
	
		if (chat_message->message_content->n_attachment) {
			size_t n_attachment = chat_message->message_content->n_attachment;
			Attachment **attachments = chat_message->message_content->attachment;
			guint i;
			
			for (i = 0; i < n_attachment; i++) {
				Attachment *attachment = attachments[i];
				EmbedItem *embed_item = attachment->embed_item;
				if (embed_item->plus_photo) {
					PlusPhoto *plus_photo = embed_item->plus_photo;
					const gchar *image_url = plus_photo->thumbnail->image_url;
					const gchar *url = plus_photo->original_content_url;
					PurpleHttpConnection *connection;
					
					connection = purple_http_get(ha->pc, hangouts_got_http_image_for_conv, ha, image_url);
					purple_http_request_set_max_len(purple_http_conn_get_request(connection), -1);
					g_dataset_set_data_full(connection, "url", g_strdup(url), g_free);
					g_dataset_set_data_full(connection, "gaia_id", g_strdup(gaia_id), g_free);
					g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
					g_dataset_set_data(connection, "msg_flags", GINT_TO_POINTER(msg_flags));
					g_dataset_set_data(connection, "message_timestamp", GINT_TO_POINTER(message_timestamp));
				}
			}
		}
	}
	
	if (timestamp && conv_id) {
		PurpleConversation *conv = NULL;
		if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
			conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(g_hash_table_lookup(ha->one_to_ones, conv_id), ha->account));
		} else if (g_hash_table_contains(ha->group_chats, conv_id)) {
			conv = PURPLE_CONVERSATION(purple_conversations_find_chat_with_account(conv_id, ha->account));
		}
		if (conv != NULL) {
			gint64 *last_event_timestamp_ptr = (gint64 *)purple_conversation_get_data(conv, "last_event_timestamp");
			if (last_event_timestamp_ptr == NULL) {
				last_event_timestamp_ptr = g_new0(gint64, 1);
			}
			if (timestamp > *last_event_timestamp_ptr) {
				*last_event_timestamp_ptr = timestamp;
				purple_conversation_set_data(conv, "last_event_timestamp", last_event_timestamp_ptr);
			}
		}
	}
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
	
	//purple_debug_info("hangouts", "Received new typing event %p\n", typing_notification);
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
		if (chatconv != NULL) {
			PurpleChatUser *cb = purple_chat_conversation_find_user(chatconv, gaia_id);
			PurpleChatUserFlags cbflags;

			if (cb == NULL) {
				// Getting notified about a buddy we dont know about yet
				//TODO add buddy
				return;
			}
			cbflags = purple_chat_user_get_flags(cb);
			
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