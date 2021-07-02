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

#include "googlechat_events.h"

#include <string.h>
#include <glib.h>

#include "core.h"
#include "debug.h"
#include "glibcompat.h"
#include "image.h"
#include "image-store.h"
#include "mediamanager.h"

#include "googlechat_conversation.h"
#include "googlechat.pb-c.h"

// From googlechat_pblite
gchar *pblite_dump_json(ProtobufCMessage *message);


void googlechat_received_other_notification(PurpleConnection *pc, Event *event);
void googlechat_received_presence_notification(PurpleConnection *pc, Event *event);
void googlechat_received_typing_notification(PurpleConnection *pc, Event *event);
void googlechat_received_message_event(PurpleConnection *pc, Event *event);

//purple_signal_emit(purple_connection_get_protocol(ha->pc), "googlechat-received-event", ha->pc, events_response.event);

void
googlechat_register_events(gpointer plugin)
{
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_typing_notification), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_presence_notification), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_message_event), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_other_notification), NULL);
}

void
googlechat_process_received_event(GoogleChatAccount *ha, Event *event)
{
	// Can't just use this ;(
	//purple_signal_emit(purple_connection_get_protocol(ha->pc), "googlechat-received-event", ha->pc, event);
	
	size_t n_bodies = 0;
	Event__EventBody **bodies = NULL;
	
	//An event can have multiple events within it.  Mangle the structs to split multiples out into singles
	if (event->n_bodies) {
		n_bodies = event->n_bodies;
		bodies = event->bodies;
		
		event->n_bodies = 0;
		event->bodies = NULL;
	}
	
	// Send an initial 'bare' event, if there is one
	if (event->body) {
		purple_signal_emit(purple_connection_get_protocol(ha->pc), "googlechat-received-event", ha->pc, event);
	}
	
	if (n_bodies > 0) {
		Event__EventBody *orig_body = event->body;
		guint i;
		
		// loop through all the sub-bodies and make them a primary
		for (i = 0; i < n_bodies; i++) {
			Event__EventBody *body = bodies[i];
			
			event->body = body;
			
			event->has_type = TRUE;
			event->type = body->event_type;
			
			purple_signal_emit(purple_connection_get_protocol(ha->pc), "googlechat-received-event", ha->pc, event);
		}
		
		// put everything back the way it was to let memory be free'd
		event->body = orig_body;
		event->n_bodies = n_bodies;
		event->bodies = bodies;
	}
}

/*void
googlechat_received_state_update(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	
	if (ha != NULL && state_update->state_update_header != NULL) {
		gint64 current_server_time = state_update->state_update_header->current_server_time;
		
		ha->active_client_state = state_update->state_update_header->active_client_state;
		
		// libpurple can't store a 64bit int on a 32bit machine, so convert to something more usable instead (puke)
		//  also needs to work cross platform, in case the accounts.xml is being shared (double puke)
		purple_account_set_int(ha->account, "last_event_timestamp_high", current_server_time >> 32);
		purple_account_set_int(ha->account, "last_event_timestamp_low", current_server_time & 0xFFFFFFFF);
	}
}*/

/*
static void
googlechat_remove_conversation(GoogleChatAccount *ha, const gchar *conv_id)
{
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		const gchar *buddy_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
		PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, buddy_id);
		
		purple_blist_remove_buddy(buddy);
		g_hash_table_remove(ha->one_to_ones, conv_id);
		g_hash_table_remove(ha->one_to_ones_rev, buddy_id);
		
	} else if (g_hash_table_contains(ha->group_chats, conv_id)) {
		PurpleChat *chat = purple_blist_find_chat(ha->account, conv_id);
		purple_blist_remove_chat(chat);
		
		g_hash_table_remove(ha->group_chats, conv_id);
		
	} else {
		// Unknown conversation!
		return;
	}
}*/


void
googlechat_received_other_notification(PurpleConnection *pc, Event *event)
{
	gchar *json_dump;

	if (event->type == EVENT__EVENT_TYPE__MESSAGE_POSTED ||
		event->type == EVENT__EVENT_TYPE__TYPING_STATE_CHANGED) {
		return;
	}
	
	purple_debug_info("googlechat", "Received new other event %p\n", event);
	json_dump = pblite_dump_json((ProtobufCMessage *)event);
	purple_debug_info("googlechat", "%s\n", json_dump);

	g_free(json_dump);
}

void 
googlechat_process_presence_result(GoogleChatAccount *ha, UserPresence *presence)
{
	
}

void
googlechat_received_presence_notification(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha;
	UserStatusUpdatedEvent *user_status_updated_event;
	
	if (event->type != EVENT__EVENT_TYPE__USER_STATUS_UPDATED_EVENT) {
		return;
	}
	user_status_updated_event = event->body->user_status_updated_event;
	
	ha = purple_connection_get_protocol_data(pc);
	
	const gchar *status_id = NULL;
	gboolean reachable = FALSE;
	gboolean available = FALSE;
	gchar *message = NULL;
	UserStatus *user_status = user_status_updated_event->user_status;
	const gchar *user_id = user_status->user_id->id;
	PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, user_id);
	
	if (buddy != NULL) {
		status_id = purple_status_get_id(purple_presence_get_active_status(purple_buddy_get_presence(buddy)));
	}
	
	if (user_status->dnd_settings && user_status->dnd_settings->has_dnd_state) {
		DndSettings__DndStateState dnd_state = user_status->dnd_settings->dnd_state;
		
		available = TRUE;
		if (dnd_state ==  DND_SETTINGS__DND_STATE__STATE__AVAILABLE) {
			//TODO fetch presence separately from status
			reachable = TRUE;
		}
		
		if (reachable && available) {
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);
		} else if (reachable) {
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AWAY);
		} else if (available) {
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_EXTENDED_AWAY);
		} else if (purple_account_get_bool(ha->account, "treat_invisible_as_offline", FALSE)) {
			status_id = "gone";
		} else {
			// GoogleChat contacts are never really unreachable, just invisible
			status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_INVISIBLE);
		}
	} else if (buddy == NULL) {
		return;
	}
	
	if (user_status != NULL && user_status->custom_status) {
		const gchar *status_text = user_status->custom_status->status_text;
		
		if (status_text && *status_text) {
			message = g_strdup(status_text);
		}
	}
	
	if (message != NULL) {
		purple_protocol_got_user_status(ha->account, user_id, status_id, "message", message, NULL);
	} else {
		purple_protocol_got_user_status(ha->account, user_id, status_id, NULL);
	}
	
	g_free(message);
}

static void
googlechat_got_http_image_for_conv(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
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
	gchar *escaped_image_url;
	
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
	escaped_image_url = g_markup_escape_text(purple_http_request_get_url(purple_http_conn_get_request(connection)), -1);
	msg = g_strdup_printf("<a href='%s'>View full image <img id='%u' src='%s' /></a>", url, image_id, escaped_image_url);
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
			}
		}
	}
	
	g_free(escaped_image_url);
	g_free(msg);
	g_dataset_destroy(connection);
}

void
googlechat_received_message_event(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha;
	MessageEvent *message_event;
	const gchar *conv_id;
	const gchar *sender_id;
	
	if (event->type != EVENT__EVENT_TYPE__MESSAGE_POSTED) {
		return;
	}
	message_event = event->body->message_posted;
	
	ha = purple_connection_get_protocol_data(pc);
	
	Message *message = message_event->message;
	guint i;
	
	if (message->local_id && g_hash_table_remove(ha->sent_message_ids, message->local_id)) {
		// This probably came from us
		return;
	}
	
	//TODO safety checks
	sender_id = message->creator->user_id->id;
	GroupId *group_id = message->id->parent_id->topic_id->group_id;
	gboolean is_dm = !!group_id->dm_id;
	if (is_dm) {
		conv_id = group_id->dm_id->dm_id;
	} else {
		conv_id = group_id->space_id->space_id;
	}
	
	
	time_t message_timestamp = (message->create_time / 1000000) - ha->server_time_offset;
	PurpleMessageFlags msg_flags = (g_strcmp0(sender_id, ha->self_gaia_id) ? PURPLE_MESSAGE_RECV : (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_REMOTE_SEND | PURPLE_MESSAGE_DELAYED));
	if (((message->create_time / 1000000) - time(NULL) - ha->server_time_offset) > 120) {
		msg_flags |= PURPLE_MESSAGE_DELAYED;
	}
	PurpleConversation *pconv = NULL;
	
	//TODO process Annotations to add formatting
	gchar *msg = g_strdup(message->text_body);
	
	if (!is_dm) {
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		if (chatconv == NULL) {
			//TODO /api/get_group
			// chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
			// purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
			// if (conversation) {
				// guint i;
				// for (i = 0; i < conversation->n_current_participant; i++) {
					// PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
					// purple_chat_conversation_add_user(chatconv, conversation->current_participant[i]->gaia_id, NULL, cbflags, FALSE);
				// }
			// }
		}
		pconv = PURPLE_CONVERSATION(chatconv);
		purple_serv_got_chat_in(pc, g_str_hash(conv_id), sender_id, msg_flags, msg, message_timestamp);
		
	} else {
		PurpleIMConversation *imconv = NULL;
		// It's most likely a one-to-one message
		if (msg_flags & PURPLE_MESSAGE_RECV) {
			purple_serv_got_im(pc, sender_id, msg, msg_flags, message_timestamp);
		} else {
			sender_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
			if (sender_id) {
				imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
				PurpleMessage *pmessage = purple_message_new_outgoing(sender_id, msg, msg_flags);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(ha->account, sender_id);
				}
				purple_message_set_time(pmessage, message_timestamp);
				purple_conversation_write_message(PURPLE_CONVERSATION(imconv), pmessage);
			}
		}
		
		if (imconv == NULL) {
			imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
		}
		pconv = PURPLE_CONVERSATION(imconv);
	}
	
	if (purple_conversation_has_focus(pconv)) {
		googlechat_mark_conversation_seen(pconv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
	}
	
	g_free(msg);
	//purple_xmlnode_free(html);
	
	for (i = 0; i < message->n_annotations; i++) {
		Annotation *annotation = message->annotations[i];
		
		if (annotation->drive_metadata) {
			DriveMetadata *drive_metadata = annotation->drive_metadata;
			const gchar *image_url = drive_metadata->thumbnail_url;
			const gchar *url = drive_metadata->url_fragment;
			PurpleHttpConnection *connection;
			
			//TODO
			continue;
			
			if (g_strcmp0(purple_core_get_ui(), "BitlBee") == 0) {
				// Bitlbee doesn't support images, so just plop a url to the image instead
				if (g_hash_table_contains(ha->group_chats, conv_id)) {
					purple_serv_got_chat_in(pc, g_str_hash(conv_id), sender_id, msg_flags, url, message_timestamp);
				} else {
					if (msg_flags & PURPLE_MESSAGE_RECV) {
						purple_serv_got_im(pc, sender_id, url, msg_flags, message_timestamp);
					} else {
						PurpleMessage *img_message = purple_message_new_outgoing(sender_id, url, msg_flags);
						purple_message_set_time(img_message, message_timestamp);
						purple_conversation_write_message(pconv, img_message);
					}
				}
			} else {
				connection = purple_http_get(ha->pc, googlechat_got_http_image_for_conv, ha, image_url);
				purple_http_request_set_max_len(purple_http_conn_get_request(connection), -1);
				g_dataset_set_data_full(connection, "url", g_strdup(url), g_free);
				g_dataset_set_data_full(connection, "sender_id", g_strdup(sender_id), g_free);
				g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
				g_dataset_set_data(connection, "msg_flags", GINT_TO_POINTER(msg_flags));
				g_dataset_set_data(connection, "message_timestamp", GINT_TO_POINTER(message_timestamp));
			}
		}
	}
}

/*
void
googlechat_received_event_notification(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha;
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
	
	googlechat_process_conversation_event(ha, conversation, event, current_server_time);
}

void
googlechat_process_conversation_event(GoogleChatAccount *ha, Conversation *conversation, Event *event, gint64 current_server_time)
{
	PurpleConnection *pc = ha->pc;
	const gchar *gaia_id;
	const gchar *conv_id;
	gint64 timestamp;
	const gchar *client_generated_id;
	ChatMessage *chat_message;
	PurpleMessageFlags msg_flags;
	PurpleConversation *pconv = NULL;
	
	if (conversation && (conv_id = conversation->conversation_id->id) &&
			!g_hash_table_contains(ha->one_to_ones, conv_id) && 
			!g_hash_table_contains(ha->group_chats, conv_id)) {
		// New conversation we ain't seen before
		
		googlechat_add_conversation_to_blist(ha, conversation, NULL);
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
	
	if (conv_id == NULL) {
		// Invalid conversation
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
					//TODO
					//LeaveReason reason = membership_change->leave_reason;
					
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
		
		for (i = 0; i < chat_message->n_annotation; i++) {
			EventAnnotation *annotation = chat_message->annotation[i];
			
			if (annotation->type == GOOGLECHAT_MAGIC_HALF_EIGHT_SLASH_ME_TYPE) {
				//TODO strip name off the front of the first segment
				purple_xmlnode_insert_data(html, "/me ", -1);
				break;
			}
		}

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
					const gchar *href = segment->link_data->link_target;
					purple_xmlnode_set_attrib(node, "href", href);
					
					// Strip out the www.google.com/url?q= bit
					if (purple_account_get_bool(ha->account, "unravel_google_url", FALSE)) {
						PurpleHttpURL *url = purple_http_url_parse(href);
						if (purple_strequal(purple_http_url_get_host(url), "www.google.com")) {
							const gchar *path = purple_http_url_get_path(url);
							//apparently the path includes the query string
							if (g_str_has_prefix(path, "/url?q=")) {
								const gchar *end = strchr(path, '&');
								gsize len = (end ? (gsize) (end - path) : (gsize) strlen(path));
								gchar *new_href = g_strndup(path + 7, len - 7);
								purple_xmlnode_set_attrib(node, "href", purple_url_decode(new_href));
								g_free(new_href);
							}
						}
						purple_http_url_free(url);
					}
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
		msg_flags = (g_strcmp0(gaia_id, ha->self_gaia_id) ? PURPLE_MESSAGE_RECV : (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_REMOTE_SEND | PURPLE_MESSAGE_DELAYED));
		if (((current_server_time - timestamp) / 1000000) > 120) {
			msg_flags |= PURPLE_MESSAGE_DELAYED;
		}
		
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
			pconv = PURPLE_CONVERSATION(chatconv);
			purple_serv_got_chat_in(pc, g_str_hash(conv_id), gaia_id, msg_flags, msg, message_timestamp);
			
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
				}
			}
			
			if (imconv == NULL) {
				imconv = purple_conversations_find_im_with_account(gaia_id, ha->account);
			}
			pconv = PURPLE_CONVERSATION(imconv);
		}
		
		if (purple_conversation_has_focus(pconv)) {
			googlechat_mark_conversation_seen(pconv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
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
					const gchar *url = plus_photo->url;
					PurpleHttpConnection *connection;
					
					// Provide a direct link to the video
					if (plus_photo->media_type == PLUS_PHOTO__MEDIA_TYPE__MEDIA_TYPE_VIDEO && plus_photo->download_url != NULL) {
						url = plus_photo->download_url;
					}
					
					if (g_strcmp0(purple_core_get_ui(), "BitlBee") == 0) {
						// Bitlbee doesn't support images, so just plop a url to the image instead
						if (g_hash_table_contains(ha->group_chats, conv_id)) {
							purple_serv_got_chat_in(pc, g_str_hash(conv_id), gaia_id, msg_flags, url, message_timestamp);
						} else {
							if (msg_flags & PURPLE_MESSAGE_RECV) {
								purple_serv_got_im(pc, gaia_id, url, msg_flags, message_timestamp);
							} else {
								PurpleMessage *img_message = purple_message_new_outgoing(gaia_id, url, msg_flags);
								purple_message_set_time(img_message, message_timestamp);
								purple_conversation_write_message(pconv, img_message);
							}
						}
					} else {
						connection = purple_http_get(ha->pc, googlechat_got_http_image_for_conv, ha, image_url);
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
	}
	
	if (event->hangout_event != NULL) {
		//event->event_type == EVENT_TYPE__EVENT_TYPE_HANGOUT || EVENT_TYPE__EVENT_TYPE_PHONE_CALL
		//Something to do with calling
		const gchar *msg;
		HangoutEvent *hangout_event = event->hangout_event;
		time_t message_timestamp = time(NULL) - ((current_server_time - timestamp) / 1000000);
		
		switch (hangout_event->event_type) {
			case HANGOUT_EVENT_TYPE__HANGOUT_EVENT_TYPE_START:
				msg = _("Call started");
				break;
			case HANGOUT_EVENT_TYPE__HANGOUT_EVENT_TYPE_END:
				msg = _("Call ended");
				break;
			default:
				msg = NULL;
				break;
		}
		if (msg != NULL) {
			if (g_hash_table_contains(ha->group_chats, conv_id)) {
				purple_serv_got_chat_in(ha->pc, g_str_hash(conv_id), gaia_id, PURPLE_MESSAGE_SYSTEM, msg, message_timestamp);
			} else {
				gaia_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
				purple_serv_got_im(ha->pc, gaia_id, msg, PURPLE_MESSAGE_SYSTEM, message_timestamp);
			}
		}
		if (hangout_event->event_type == HANGOUT_EVENT_TYPE__HANGOUT_EVENT_TYPE_START) {
			if (purple_account_get_bool(ha->account, "show-call-links", !purple_media_manager_get())) {
				//No voice/video support, display URL
				gchar *join_message = g_strdup_printf("%s https://plus.google.com/googlechat/_/CONVERSATION/%s", _("To join the call, open "), conv_id);
				if (g_hash_table_contains(ha->group_chats, conv_id)) {
					purple_serv_got_chat_in(ha->pc, g_str_hash(conv_id), gaia_id, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, join_message, message_timestamp);
				} else {
					purple_serv_got_im(ha->pc, gaia_id, join_message, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG, message_timestamp);
				}
				g_free(join_message);
			}
		}
	}
	
	if (conv_id && event->conversation_rename != NULL) {
		ConversationRename *conversation_rename = event->conversation_rename;
		PurpleChat *chat;
		
		if (g_hash_table_contains(ha->group_chats, conv_id)) {
			chat = purple_blist_find_chat(ha->account, conv_id);
			if (chat && !purple_strequal(purple_chat_get_alias(chat), conversation_rename->new_name)) {
				g_dataset_set_data(ha, "ignore_set_alias", "true");
				purple_chat_set_alias(chat, conversation_rename->new_name);
				g_dataset_set_data(ha, "ignore_set_alias", NULL);
			}
		}
	}
	
	if (timestamp && conv_id) {
		if (pconv == NULL) {
			if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
				pconv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(g_hash_table_lookup(ha->one_to_ones, conv_id), ha->account));
			} else if (g_hash_table_contains(ha->group_chats, conv_id)) {
				pconv = PURPLE_CONVERSATION(purple_conversations_find_chat_with_account(conv_id, ha->account));
			}
		}
		if (pconv != NULL) {
			gint64 *last_event_timestamp_ptr = (gint64 *)purple_conversation_get_data(pconv, "last_event_timestamp");
			if (last_event_timestamp_ptr == NULL) {
				last_event_timestamp_ptr = g_new0(gint64, 1);
			}
			if (timestamp > *last_event_timestamp_ptr) {
				*last_event_timestamp_ptr = timestamp;
				purple_conversation_set_data(pconv, "last_event_timestamp", last_event_timestamp_ptr);
			}
		}
	}
}*/

void
googlechat_received_typing_notification(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha;
	TypingStateChangedEvent *typing_notification;
	const gchar *user_id;
	const gchar *conv_id;
	PurpleIMTypingState typing_state;
	
	if (event->type != EVENT__EVENT_TYPE__TYPING_STATE_CHANGED) {
		return;
	}
	typing_notification = event->body->typing_state_changed_event;
	
	ha = purple_connection_get_protocol_data(pc);
	
	user_id = typing_notification->user_id->id;
	if (ha->self_gaia_id && g_strcmp0(user_id, ha->self_gaia_id) == 0)
		return;
	
	if (!typing_notification->context->group_id) {
		//TODO handle topic_id -> group_id conversion
		return;
	}
	
	GroupId *group_id = typing_notification->context->group_id;
	gboolean is_dm = !!group_id->dm_id;
	if (is_dm) {
		conv_id = group_id->dm_id->dm_id;
	} else {
		conv_id = group_id->space_id->space_id;
	}
	
	if (!is_dm) {
		// This is a group conversation
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		if (chatconv != NULL) {
			PurpleChatUser *cb = purple_chat_conversation_find_user(chatconv, user_id);
			PurpleChatUserFlags cbflags;

			if (cb == NULL) {
				// Getting notified about a buddy we dont know about yet
				//TODO add buddy
				return;
			}
			cbflags = purple_chat_user_get_flags(cb);
			
			if (typing_notification->state == TYPING_STATE__TYPING)
				cbflags |= PURPLE_CHAT_USER_TYPING;
			else
				cbflags &= ~PURPLE_CHAT_USER_TYPING;
			
			purple_chat_user_set_flags(cb, cbflags);
		}
		return;
	}
	
	switch(typing_notification->state) {
		case TYPING_STATE__TYPING:
			typing_state = PURPLE_IM_TYPING;
			break;
		
		default:
		case TYPING_STATE__STOPPED:
			typing_state = PURPLE_IM_NOT_TYPING;
			break;
	}
	
	purple_serv_got_typing(pc, user_id, 20, typing_state);
}
