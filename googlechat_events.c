/*
 * GoogleChat Plugin for libpurple/Pidgin
 * Copyright (c) 2015-2025 Eion Robb, Mike Ruprecht
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
void googlechat_received_read_receipt(PurpleConnection *pc, Event *event);
void googlechat_received_group_viewed(PurpleConnection *pc, Event *event);

//purple_signal_emit(purple_connection_get_protocol(ha->pc), "googlechat-received-event", ha->pc, events_response.event);

void
googlechat_register_events(gpointer plugin)
{
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_typing_notification), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_presence_notification), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_message_event), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_other_notification), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_read_receipt), NULL);
	purple_signal_connect(plugin, "googlechat-received-event", plugin, PURPLE_CALLBACK(googlechat_received_group_viewed), NULL);
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
	
	gint64 event_time = 0;
	if (event->user_revision) {
		event_time = event->user_revision->timestamp;
	}
	if (event->group_revision) {
		event_time = event->group_revision->timestamp;
	}
	
	if (event_time && event_time > ha->last_event_timestamp) {
		// libpurple can't store a 64bit int on a 32bit machine, so convert to something more usable instead (puke)
		//  also needs to work cross platform, in case the accounts.xml is being shared (double puke)
		purple_account_set_int(ha->account, "last_event_timestamp_high", event_time >> 32);
		purple_account_set_int(ha->account, "last_event_timestamp_low", event_time & 0xFFFFFFFF);
	}
}


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
		event->type == EVENT__EVENT_TYPE__TYPING_STATE_CHANGED ||
		event->type == EVENT__EVENT_TYPE__GROUP_VIEWED ||
		event->type == EVENT__EVENT_TYPE__USER_STATUS_UPDATED_EVENT ||
		event->type == EVENT__EVENT_TYPE__READ_RECEIPT_CHANGED
	) {
		return;
	}
	
	purple_debug_info("googlechat", "Received new other event %p\n", event);
	json_dump = pblite_dump_json((ProtobufCMessage *)event);
	purple_debug_info("googlechat", "%s\n", json_dump);

	g_free(json_dump);
}

void 
googlechat_process_presence_result(GoogleChatAccount *ha, DYNProtoUserPresence *presence)
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
	const gchar *drive_url;
	const gchar *sender_id;
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
	drive_url = g_dataset_get_data(connection, "drive_url");
	sender_id = g_dataset_get_data(connection, "sender_id");
	conv_id = g_dataset_get_data(connection, "conv_id");
	msg_flags = GPOINTER_TO_INT(g_dataset_get_data(connection, "msg_flags"));
	message_timestamp = GPOINTER_TO_INT(g_dataset_get_data(connection, "message_timestamp"));
	
	response_data = purple_http_response_get_data(response, &response_size);
	image = purple_image_new_from_data(g_memdup2(response_data, response_size), response_size);
	image_id = purple_image_store_add(image);
	escaped_image_url = g_markup_escape_text(purple_http_request_get_url(purple_http_conn_get_request(connection)), -1);
	if (drive_url) {
		msg = g_strdup_printf("<a href='%s'>View in Drive <img id='%u' src='%s' /></a>", drive_url, image_id, escaped_image_url);
	} else {
		msg = g_strdup_printf("<a href='%s'>View full image <img id='%u' src='%s' /></a>", url, image_id, escaped_image_url);
	}
	msg_flags |= PURPLE_MESSAGE_IMAGES;
		
	if (g_hash_table_contains(ha->group_chats, conv_id)) {
		purple_serv_got_chat_in(ha->pc, g_str_hash(conv_id), sender_id, msg_flags, msg, message_timestamp);
	} else {
		if (msg_flags & PURPLE_MESSAGE_RECV) {
			purple_serv_got_im(ha->pc, sender_id, msg, msg_flags, message_timestamp);
		} else {
			sender_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
			if (sender_id) {
				PurpleIMConversation *imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
				PurpleMessage *message = purple_message_new_outgoing(sender_id, msg, msg_flags);
				if (imconv == NULL) {
					imconv = purple_im_conversation_new(ha->account, sender_id);
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

static const gchar *
googlechat_format_type_to_string(FormatMetadata__FormatType format_type, gboolean close)
{
	static gchar output[16];
	int p = 1;
	
	output[0] = '<';
	if (close) {
		output[1] = '/';
		p = 2;
	}
	
	if (format_type == FORMAT_METADATA__FORMAT_TYPE__BOLD) {
		output[p] = 'B';  p++;
	} else if (format_type == FORMAT_METADATA__FORMAT_TYPE__ITALIC) {
		output[p] = 'I';  p++;
	} else if (format_type == FORMAT_METADATA__FORMAT_TYPE__UNDERLINE) {
		output[p] = 'U';  p++;
	} else if (format_type == FORMAT_METADATA__FORMAT_TYPE__MONOSPACE ||
				format_type == FORMAT_METADATA__FORMAT_TYPE__SOURCE_CODE) {
		output[p] = 'P';
		output[p+1] = 'R';
		output[p+2] = 'E';  p += 3;
	} // else //TODO
	
	output[p] = '>';
	output[p + 1] = '\0';
	
	return output;
}

void
googlechat_received_message_event(PurpleConnection *pc, Event *event)
{
	GoogleChatAccount *ha;
	MessageEvent *message_event;
	const gchar *conv_id;
	const gchar *sender_id;
	
	if (event->type != EVENT__EVENT_TYPE__MESSAGE_POSTED && 
		event->type != EVENT__EVENT_TYPE__MESSAGE_UPDATED) {
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
	// TODO should this be message->last_update_time?
	PurpleMessageFlags msg_flags = (g_strcmp0(sender_id, ha->self_gaia_id) ? PURPLE_MESSAGE_RECV : (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_REMOTE_SEND | PURPLE_MESSAGE_DELAYED));
	if (((message->create_time / 1000000) - time(NULL) - ha->server_time_offset) > 120) {
		msg_flags |= PURPLE_MESSAGE_DELAYED;
	}
	PurpleConversation *pconv = NULL;
	
	//TODO process Annotations to add formatting
	gchar *msg = NULL;
	GList *format_annotations = NULL;
	for (i = 0; i < message->n_annotations; i++) {
		Annotation *annotation = message->annotations[i];
		if (annotation->type == ANNOTATION_TYPE__FORMAT_DATA || annotation->type == ANNOTATION_TYPE__URL) {
			format_annotations = g_list_prepend(format_annotations, annotation);
		}
	}
	
	if (format_annotations == NULL) {
		//shortcut
		msg = purple_markup_escape_text(message->text_body, -1);
		
	} else {
		GString *msg_out = g_string_new(event->type == EVENT__EVENT_TYPE__MESSAGE_UPDATED ? _("Edit: ") : NULL);
		const gchar *current_char = message->text_body;
		gunichar c;
		gint32 pos = 0;
		GList *format;
		gint hidden_output = 0;
		
		do {
			if (format_annotations != NULL) {
				format = format_annotations;
				while(format != NULL) {
					GList *next_format = format->next;
					Annotation *annotation = format->data;
					FormatMetadata__FormatType format_type = annotation->format_metadata ? annotation->format_metadata->format_type : 0;
					
					// Opening
					if (annotation->start_index == pos) {
						if (format_type == FORMAT_METADATA__FORMAT_TYPE__HIDDEN) {
							hidden_output++;
						
						} else if (format_type == FORMAT_METADATA__FORMAT_TYPE__FONT_COLOR) {
							g_string_append_printf(msg_out, "<span style='color: #%02x%02x%02x'>", 
								annotation->format_metadata->font_color >> 16,
								annotation->format_metadata->font_color >> 8 & 0xFF,
								annotation->format_metadata->font_color & 0xFF);
						
						} else if (annotation->type == ANNOTATION_TYPE__URL) {
							UrlMetadata *url_metadata = annotation->url_metadata;
							if (url_metadata && url_metadata->url && url_metadata->url->url) {
								gchar *escaped = g_markup_escape_text(url_metadata->url->url, -1);
								g_string_append_printf(msg_out, "<A HREF=\"%s\">", escaped);
								// TODO this is likely a tenor gif, so we should download it
								if (annotation->length == 0 && url_metadata->has_should_not_render && url_metadata->should_not_render == FALSE) {
									g_string_append(msg_out, escaped);
								}
								g_free(escaped);
							}
						} else {
							g_string_append(msg_out, googlechat_format_type_to_string(format_type, FALSE));
						}
					
					// Closing
					} else if (annotation->length + annotation->start_index == pos) {
						if (format_type == FORMAT_METADATA__FORMAT_TYPE__HIDDEN) {
							hidden_output--;
						} else if (annotation->type == ANNOTATION_TYPE__URL) {
							g_string_append(msg_out, "</A>");
						} else if (format_type == FORMAT_METADATA__FORMAT_TYPE__FONT_COLOR) {
							g_string_append(msg_out, "</span>");
						} else {
							g_string_append(msg_out, googlechat_format_type_to_string(format_type, TRUE));
						}
						
						format_annotations = g_list_delete_link(format_annotations, format);
					}
					
					format = next_format;
				}
			}
			
			if (hidden_output == 0) {
				// from libpurple/util.c
				switch (*current_char)
				{
					case '&':
						g_string_append(msg_out, "&amp;");
						break;

					case '<':
						g_string_append(msg_out, "&lt;");
						break;

					case '>':
						g_string_append(msg_out, "&gt;");
						break;

					case '"':
						g_string_append(msg_out, "&quot;");
						break;

					default:
						c = g_utf8_get_char(current_char);
						if ((0x1 <= c && c <= 0x8) ||
								(0xb <= c && c <= 0xc) ||
								(0xe <= c && c <= 0x1f) ||
								(0x7f <= c && c <= 0x84) ||
								(0x86 <= c && c <= 0x9f))
							g_string_append_printf(msg_out, "&#x%x;", c);
						else
							g_string_append_unichar(msg_out, c);
						break;
				}
			}
			pos++;
			
		} while ((current_char = g_utf8_next_char(current_char)) && *current_char);
		
		msg = g_string_free(msg_out, FALSE);
	}
	
	if (!is_dm) {
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		if (chatconv == NULL) {
			//TODO /api/get_group
			chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
			purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
			g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
			
			googlechat_lookup_group_info(ha, conv_id);
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

	// Process join/part's
	for (i = 0; i < message->n_annotations; i++) {
		Annotation *annotation = message->annotations[i];
		
		if (annotation->membership_changed) { //if (annotation->type == ANNOTATION_TYPE__MEMBERSHIP_CHANGED) {
			PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
			MembershipChangedMetadata *membership_change = annotation->membership_changed;
			guint j;
			
			if (membership_change->type == MEMBERSHIP_CHANGED_METADATA__TYPE__LEFT ||
				membership_change->type == MEMBERSHIP_CHANGED_METADATA__TYPE__REMOVED ||
				membership_change->type == MEMBERSHIP_CHANGED_METADATA__TYPE__BOT_REMOVED) {
				//TODO
				const gchar *reason = NULL;
				
				for (j = 0; j < membership_change->n_affected_members; j++) {
					MemberId *member_id = membership_change->affected_members[j];
					
					purple_chat_conversation_remove_user(chatconv, member_id->user_id->id, reason);
					
					if (g_strcmp0(member_id->user_id->id, ha->self_gaia_id) == 0) {
						purple_serv_got_chat_left(ha->pc, g_str_hash(conv_id));
						g_hash_table_remove(ha->group_chats, conv_id);
						purple_blist_remove_chat(purple_blist_find_chat(ha->account, conv_id));
					}
				}
			} else {
				PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE; //TODO
				
				for (j = 0; j < membership_change->n_affected_members; j++) {
					MemberId *member_id = membership_change->affected_members[j];
					
					purple_chat_conversation_add_user(chatconv, member_id->user_id->id, NULL, cbflags, TRUE);
				}
			}
		}
	}
	
	// Add images
	for (i = 0; i < message->n_annotations; i++) {
		Annotation *annotation = message->annotations[i];
		gchar *image_url = NULL; // Direct image URL
		const gchar *url = NULL; // Display URL
		const gchar *drive_url = NULL; // Google Drive URL
		
		if (annotation->upload_metadata) {
			UploadMetadata *upload_metadata = annotation->upload_metadata;
			
			//const gchar *content_type = upload_metadata->content_type;
			const gchar *attachment_token = upload_metadata->attachment_token;
			
			GString *image_url_str = g_string_new(NULL);
			
			g_string_append(image_url_str, "https://chat.google.com/api/get_attachment_url" "?");
			//could also be THUMBNAIL_URL
			// or DOWNLOAD_URL for a file
			g_string_append(image_url_str, "url_type=FIFE_URL&");
			//g_string_append_printf(image_url_str, "sz=w%d-h%d&", 0, 0); //TODO
			//g_string_append_printf(image_url_str, "content_type=%s&", purple_url_encode(content_type));
			g_string_append_printf(image_url_str, "attachment_token=%s&", purple_url_encode(attachment_token));
			
			// this url redirects to the actual url
			url = image_url = g_string_free(image_url_str, FALSE);
		}
		
		if (annotation->drive_metadata) {
			DriveMetadata *drive_metadata = annotation->drive_metadata;
			
			if (drive_metadata->thumbnail_url) {
				image_url = g_strdup(drive_metadata->thumbnail_url);
				url = drive_metadata->url_fragment;
				
			} else {
				const gchar *drive_id = drive_metadata->id;
				
				GString *image_url_str = g_string_new(NULL);
				
				g_string_append(image_url_str, "https://lh3.googleusercontent.com/d/");
				g_string_append(image_url_str, purple_url_encode(drive_id));
				
				// the preview
				url = image_url = g_string_free(image_url_str, FALSE);

				GString *drive_url_str = g_string_new(NULL);
				g_string_append(drive_url_str, "https://drive.google.com/open?id=");
				g_string_append(drive_url_str, purple_url_encode(drive_id));

				// the link
				drive_url = g_string_free(drive_url_str, FALSE);
			}
		}
		
		if (annotation->url_metadata) {
			UrlMetadata *url_metadata = annotation->url_metadata;
			
			if (url_metadata->image_url) {
				image_url = g_strdup(url_metadata->image_url);
				url = url_metadata->url ? url_metadata->url->url : url_metadata->image_url;
				
			}
		}
		
		if (image_url != NULL) {
			PurpleHttpConnection *connection;
			
			if (!ha->access_token || g_strcmp0(purple_core_get_ui(), "BitlBee") == 0) {
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
				PurpleHttpRequest *request = purple_http_request_new(image_url);
				
				purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->access_token);
				purple_http_request_set_max_len(request, -1);
				
				connection = purple_http_request(ha->pc, request, googlechat_got_http_image_for_conv, ha);
				
				g_dataset_set_data_full(connection, "url", g_strdup(url), g_free);
				g_dataset_set_data_full(connection, "drive_url", g_strdup(drive_url), g_free);
				g_dataset_set_data_full(connection, "sender_id", g_strdup(sender_id), g_free);
				g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
				g_dataset_set_data(connection, "msg_flags", GINT_TO_POINTER(msg_flags));
				g_dataset_set_data(connection, "message_timestamp", GINT_TO_POINTER(message_timestamp));
				
				purple_http_request_unref(request);
			}
		}
		
		g_free(image_url);
	}
	
	// Attachments, eg Cards
	for (i = 0; i < message->n_attachments; i++) {
		Attachment *attachment = message->attachments[i];
		JAddOnsCardItem *card = attachment->card_add_on_data;
		if (card == NULL) {
			continue;
		}

		if (card->n_sections > 0) {
			GString *msg_out = g_string_new(event->type == EVENT__EVENT_TYPE__MESSAGE_UPDATED ? _("Edit: ") : NULL);
			guint j;
			
			for (j = 0; j < card->n_sections; j++) {
				JAddOnsCardItem__CardItemSection *section = card->sections[j];
				if (section->n_widgets > 0) {
					guint k;
					for (k = 0; k < section->n_widgets; k++) {
						JAddOnsWidget *widget = section->widgets[k];
						g_string_append(msg_out, "<hr>");
						if (widget->text_paragraph) {
							g_string_append(msg_out, "<p>");
							JAddOnsFormattedText *text_paragraph = widget->text_paragraph->text;
							if (text_paragraph->n_formatted_text_elements > 0) {
								guint l;
								for (l = 0; l < text_paragraph->n_formatted_text_elements; l++) {
									JAddOnsFormattedText__FormattedTextElement *formatted_text_element = text_paragraph->formatted_text_elements[l];
									if (formatted_text_element->styled_text) {
										if (formatted_text_element->styled_text->n_styles > 0) {
											guint m;
											for (m = 0; m < formatted_text_element->styled_text->n_styles; m++) {
												JAddOnsFormattedText__FormattedTextElement__StyledText__Style style = formatted_text_element->styled_text->styles[m];
												if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__BOLD_DEPRECATED) {
													g_string_append(msg_out, "<b>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__ITALIC) {
													g_string_append(msg_out, "<i>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__UNDERLINE) {
													g_string_append(msg_out, "<u>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__STRIKETHROUGH) {
													g_string_append(msg_out, "<s>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__BR) {
													g_string_append(msg_out, "<br>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__UPPERCASE) {
													//TODO
												}
											}
										}
										if (formatted_text_element->styled_text->font_weight == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__FONT_WEIGHT__BOLD) {
											g_string_append(msg_out, "<b>");
										}
										if (formatted_text_element->styled_text->has_color) {
											gint color = formatted_text_element->styled_text->color;
											g_string_append_printf(msg_out, "<span style='color: #%02x%02x%02x'>", 
												color >> 16,
												color >> 8 & 0xFF,
												color & 0xFF);
										}

										gchar *escaped_text = g_markup_escape_text(formatted_text_element->styled_text->text, -1);
										g_string_append(msg_out, escaped_text);
										g_free(escaped_text);
										
										if (formatted_text_element->styled_text->has_color) {
											g_string_append(msg_out, "</span>");
										}
										if (formatted_text_element->styled_text->font_weight == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__FONT_WEIGHT__BOLD) {
											g_string_append(msg_out, "</b>");
										}
										if (formatted_text_element->styled_text->n_styles > 0) {
											gint m;
											for (m = formatted_text_element->styled_text->n_styles - 1; m >= 0 ; m--) {
												JAddOnsFormattedText__FormattedTextElement__StyledText__Style style = formatted_text_element->styled_text->styles[m];
												if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__BOLD_DEPRECATED) {
													g_string_append(msg_out, "</b>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__ITALIC) {
													g_string_append(msg_out, "</i>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__UNDERLINE) {
													g_string_append(msg_out, "</u>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__STRIKETHROUGH) {
													g_string_append(msg_out, "</s>");
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__BR) {
													//skip
												} else if (style == JADD_ONS_FORMATTED_TEXT__FORMATTED_TEXT_ELEMENT__STYLED_TEXT__STYLE__UPPERCASE) {
													//TODO
												}
											}
										}
									}
									//TODO handle formatted_text_element->hyperlink
								}
							}
							g_string_append(msg_out, "</p>\n");
						}
					}
				}
			}
			
			if (msg_out->len > 0) {
				if (g_hash_table_contains(ha->group_chats, conv_id)) {
					purple_serv_got_chat_in(pc, g_str_hash(conv_id), sender_id, msg_flags, msg_out->str, message_timestamp);
				} else {
					if (msg_flags & PURPLE_MESSAGE_RECV) {
						purple_serv_got_im(pc, sender_id, msg_out->str, msg_flags, message_timestamp);
					} else {
						PurpleMessage *pmessage = purple_message_new_outgoing(sender_id, msg_out->str, msg_flags);
						purple_message_set_time(pmessage, message_timestamp);
						purple_conversation_write_message(pconv, pmessage);
					}
				}
			}
			g_string_free(msg_out, TRUE);
		}
	}
	
	if (pconv != NULL) {
		gint64 *last_event_timestamp_ptr = (gint64 *)purple_conversation_get_data(pconv, "last_event_timestamp");
		if (last_event_timestamp_ptr == NULL) {
			last_event_timestamp_ptr = g_new0(gint64, 1);
		}
		if (message->create_time > *last_event_timestamp_ptr) {
			*last_event_timestamp_ptr = message->create_time;
			purple_conversation_set_data(pconv, "last_event_timestamp", last_event_timestamp_ptr);
		}
	}
}

void
googlechat_received_read_receipt(PurpleConnection *pc, Event *event)
{
	const gchar *user_id;
	GroupId *group_id;
	const gchar *conv_id;
	GoogleChatAccount *ha;
	ReadReceiptSet *receipt_set;
	
	if (event->type != EVENT__EVENT_TYPE__READ_RECEIPT_CHANGED ||
		!event->body->read_receipt_changed ||
		!event->body->read_receipt_changed->read_receipt_set ||
		!event->body->read_receipt_changed->read_receipt_set->enabled ||
		!event->body->read_receipt_changed->group_id
	) {
		return;
	}
	
	receipt_set = event->body->read_receipt_changed->read_receipt_set;
	guint i;
	for (i = 0; i < receipt_set->n_read_receipts; i++) {
		if (receipt_set->read_receipts[i]->user &&
			receipt_set->read_receipts[i]->user->user_id &&
			receipt_set->read_receipts[i]->user->user_id->id
		) {
			user_id = receipt_set->read_receipts[i]->user->user_id->id;
			ha = purple_connection_get_protocol_data(pc);
			
			// don't emit our own receipts
			if (ha->self_gaia_id && g_strcmp0(user_id, ha->self_gaia_id) != 0) {
				group_id = event->body->read_receipt_changed->group_id;
				gboolean is_dm = !!group_id->dm_id;
				if (is_dm) {
					conv_id = group_id->dm_id->dm_id;
				} else {
					conv_id = group_id->space_id->space_id;
				}
				if (conv_id) {
					if (is_dm) {
						// PurpleChat *chat = purple_blist_find_chat(ha->account, conv_id);
						PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, user_id);
						if (buddy) {
							purple_debug_warning("googlechat", "TODO: username %s read DM\n", purple_buddy_get_alias(buddy)); //purple_chat_get_name(chat));
						} else {
							purple_debug_warning("googlechat", "TODO: userid %s read DM\n", user_id);
						}
					} else {
						PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);

						if (chatconv) {
							PurpleChatUser *cb = purple_chat_conversation_find_user(chatconv, user_id);
							if (cb) {
								purple_debug_warning("googlechat", "TODO: username %s read chat\n", cb->name);
							} else {
								purple_debug_warning("googlechat", "TODO: userid %s read chat\n", user_id);
							}
						}
					}
				}
			}
		}
	}
}


void
googlechat_received_group_viewed(PurpleConnection *pc, Event *event)
{
	const gchar *user_id;
	GroupId *group_id;
	const gchar *conv_id;
	const gchar *sender_id;
	GoogleChatAccount *ha;
	PurpleConversation *pconv = NULL;
	
	if (event->type != EVENT__EVENT_TYPE__GROUP_VIEWED ||
		!event->user_id || 
		!event->user_id->id ||
		!event->body->group_viewed->group_id
	) {
		return;
	}
	
	user_id = event->user_id->id;
	ha = purple_connection_get_protocol_data(pc);
	
	purple_debug_warning("googlechat", "Received groupview %p from userid %s\n", event, user_id);
	
	// we only expect to receive GROUP_VIEWED messages for our own user
	if (ha->self_gaia_id && g_strcmp0(user_id, ha->self_gaia_id) == 0) {
		purple_debug_info("googlechat", "...it's us %s\n", user_id);
		group_id = event->body->group_viewed->group_id;
		gboolean is_dm = !!group_id->dm_id;
		
		if (is_dm) {
			conv_id = group_id->dm_id->dm_id;
		} else {
			conv_id = group_id->space_id->space_id;
		}
		
		if (!is_dm) {
			purple_debug_info("googlechat", "...it's not a DM\n");
			PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
			if (chatconv == NULL) {
				//TODO /api/get_group
				chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
				purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
				g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
				
				googlechat_lookup_group_info(ha, conv_id);
			}
			if (chatconv) {
				pconv = PURPLE_CONVERSATION(chatconv);
			} else {
				purple_debug_info("googlechat", "...couldn't find chatconv\n");
			}
		} else {
			purple_debug_info("googlechat", "...it's a DM\n");
			PurpleIMConversation *imconv = NULL;
			// It's most likely a one-to-one message
			sender_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
			if (sender_id) {
				imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
				if (imconv == NULL)
				{
					imconv = purple_im_conversation_new(ha->account, sender_id);
				}
			}
			if (imconv == NULL) {
				imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
			}
			if (imconv) {
				pconv = PURPLE_CONVERSATION(imconv);
			} else {
				purple_debug_info("googlechat", "...couldn't find imconv\n");
			}
		}
		
		// the whole point of GROUP_VIEWED seems to be to sync our own seen status
		if (pconv) {
			purple_debug_warning("googlechat", "TODO: mark conversation '%s' as seen \n", pconv->title);
			// googlechat_mark_conversation_seen(pconv, PURPLE_CONVERSATION_UPDATE_UNSEEN);
			// warning: this makes infinite loops
		} else {
			purple_debug_info("googlechat", "...pconv was null\n");
		}
	} else {
		purple_debug_info("googlechat", "...it's not us (%s)\n", user_id);
	}
}

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
	
	purple_serv_got_typing(pc, user_id, 7, typing_state);
}
