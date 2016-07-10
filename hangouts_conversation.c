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

#include "hangouts_conversation.h"

#include "hangouts.pb-c.h"
#include "hangouts_connection.h"
#include "hangouts_events.h"

#include "debug.h"
#include "status.h"
#include "glibcompat.h"
#include "image-store.h"

// From hangouts_pblite
gchar *pblite_dump_json(ProtobufCMessage *message);

RequestHeader *
hangouts_get_request_header(HangoutsAccount *ha)
{
	RequestHeader *header = g_new0(RequestHeader, 1);
	ClientVersion *version = g_new0(ClientVersion, 1);
	request_header__init(header);
	client_version__init(version);
	
	if (ha->client_id != NULL) {
		ClientIdentifier *client_identifier = g_new0(ClientIdentifier, 1);
		client_identifier__init(client_identifier);
		
		header->client_identifier = client_identifier;
		header->client_identifier->resource = g_strdup(ha->client_id);
	}
	
	version->has_client_id = TRUE;
	version->client_id = CLIENT_ID__CLIENT_ID_ANDROID;
	
	header->client_version = version;
	
	return header;
}

void
hangouts_request_header_free(RequestHeader *header)
{
	if (header->client_identifier) {
		g_free(header->client_identifier->resource);
		g_free(header->client_identifier);
	}
	g_free(header->client_version);
	g_free(header);
}

EventRequestHeader *
hangouts_get_event_request_header(HangoutsAccount *ha, const gchar *conv_id)
{
	EventRequestHeader *header = g_new0(EventRequestHeader, 1);
	event_request_header__init(header);
	
	if (conv_id != NULL) {
		ConversationId *conversation_id = g_new0(ConversationId, 1);
		conversation_id__init(conversation_id);
		
		conversation_id->id = g_strdup(conv_id);
		header->conversation_id = conversation_id;
		
		if (g_hash_table_contains(ha->google_voice_conversations, conv_id)) {
			DeliveryMedium *delivery_medium = g_new0(DeliveryMedium, 1);
			PhoneNumber *self_phone = g_new0(PhoneNumber, 1);
			delivery_medium__init(delivery_medium);
			phone_number__init(self_phone);
			
			delivery_medium->has_medium_type = TRUE;
			delivery_medium->medium_type = DELIVERY_MEDIUM_TYPE__DELIVERY_MEDIUM_GOOGLE_VOICE;
			self_phone->e164 = g_strdup(ha->self_phone);
			delivery_medium->self_phone = self_phone;
			
			header->delivery_medium = delivery_medium;
			header->has_event_type = TRUE;
			header->event_type = EVENT_TYPE__EVENT_TYPE_SMS;
		}
	}
	
	header->has_client_generated_id = TRUE;
	header->client_generated_id = g_random_int();
	
	//todo off the record status
	
	return header;
}

void
hangouts_event_request_header_free(EventRequestHeader *header)
{
	if (header->conversation_id) {
		g_free(header->conversation_id->id);
		g_free(header->conversation_id);
	}
	if (header->delivery_medium) {
		if (header->delivery_medium->self_phone) {
			g_free(header->delivery_medium->self_phone->e164);
			g_free(header->delivery_medium->self_phone);
		}
		g_free(header->delivery_medium);
	}
	
	g_free(header);
}

static void 
hangouts_got_self_info(HangoutsAccount *ha, GetSelfInfoResponse *response, gpointer user_data)
{
	Entity *self_entity = response->self_entity;
	PhoneData *phone_data = response->phone_data;
	guint i;
	const gchar *alias;
	
	g_return_if_fail(self_entity);
	
	g_free(ha->self_gaia_id);
	ha->self_gaia_id = g_strdup(self_entity->id->gaia_id);
	purple_connection_set_display_name(ha->pc, ha->self_gaia_id);
	purple_account_set_string(ha->account, "self_gaia_id", ha->self_gaia_id);
	
	alias = purple_account_get_private_alias(ha->account);
	if (alias == NULL || *alias == '\0') {
		purple_account_set_private_alias(ha->account, self_entity->properties->display_name);
	}
	
	if (phone_data != NULL) {
		for (i = 0; i < phone_data->n_phone; i++) {
			Phone *phone = phone_data->phone[i];
			if (phone->google_voice) {
				g_free(ha->self_phone);
				ha->self_phone = g_strdup(phone->phone_number->e164);
				break;
			}
		}
	}
	
	hangouts_get_buddy_list(ha);
}

void
hangouts_get_self_info(HangoutsAccount *ha)
{
	GetSelfInfoRequest request;
	get_self_info_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	
	hangouts_pblite_get_self_info(ha, &request, hangouts_got_self_info, NULL);
	
	hangouts_request_header_free(request.request_header);
	
	if (ha->last_event_timestamp != 0) {
		hangouts_get_all_events(ha, ha->last_event_timestamp);
	}
}

static void
hangouts_got_users_presence(HangoutsAccount *ha, QueryPresenceResponse *response, gpointer user_data)
{
	guint i;
	
	for (i = 0; i < response->n_presence_result; i++) {
		hangouts_process_presence_result(ha, response->presence_result[i]);
	}
}

void
hangouts_get_users_presence(HangoutsAccount *ha, GList *user_ids)
{
	QueryPresenceRequest request;
	ParticipantId **participant_id;
	guint n_participant_id;
	GList *cur;
	guint i;
	
	query_presence_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	n_participant_id = g_list_length(user_ids);
	participant_id = g_new0(ParticipantId *, n_participant_id);
	
	for (i = 0, cur = user_ids; cur && cur->data && i < n_participant_id; (cur = cur->next), i++) {
		participant_id[i] = g_new0(ParticipantId, 1);
		participant_id__init(participant_id[i]);
		
		participant_id[i]->gaia_id = (gchar *) cur->data;
		participant_id[i]->chat_id = (gchar *) cur->data; //XX do we need this?
	}
	
	request.participant_id = participant_id;
	request.n_participant_id = n_participant_id;
	
	request.n_field_mask = 4;
	request.field_mask = g_new0(FieldMask, request.n_field_mask);
	request.field_mask[0] = FIELD_MASK__FIELD_MASK_AVAILABLE;
	request.field_mask[1] = FIELD_MASK__FIELD_MASK_REACHABLE;
	request.field_mask[2] = FIELD_MASK__FIELD_MASK_MOOD;
	request.field_mask[3] = FIELD_MASK__FIELD_MASK_LAST_SEEN;

	hangouts_pblite_query_presence(ha, &request, hangouts_got_users_presence, NULL);
	
	hangouts_request_header_free(request.request_header);
	for (i = 0; i < n_participant_id; i++) {
		g_free(participant_id[i]);
	}
	g_free(participant_id);
	g_free(request.field_mask);
}

static void hangouts_got_buddy_photo(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data);

static void
hangouts_got_users_information(HangoutsAccount *ha, GetEntityByIdResponse *response, gpointer user_data)
{
	guint i;
	
	for (i = 0; i < response->n_entity_result; i++) {
		Entity *entity = response->entity_result[i]->entity[0];
		const gchar *gaia_id;

		if (entity == NULL) {
			continue;
		}
		gaia_id = entity->id ? entity->id->gaia_id : NULL;
		
		if (gaia_id != NULL && entity->properties) {
			PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, gaia_id);
			
			// Give a best-guess for the buddy's alias
			if (entity->properties->display_name)
				purple_serv_got_alias(ha->pc, gaia_id, entity->properties->display_name);
			else if (entity->properties->canonical_email)
				purple_serv_got_alias(ha->pc, gaia_id, entity->properties->canonical_email);
			else if (entity->entity_type == PARTICIPANT_TYPE__PARTICIPANT_TYPE_OFF_NETWORK_PHONE
			         && entity->properties->n_phone)
				purple_serv_got_alias(ha->pc, gaia_id, entity->properties->phone[0]);
			
			// Set the buddy photo, if it's real
			if (entity->properties->photo_url != NULL && entity->properties->photo_url_status == PHOTO_URL_STATUS__PHOTO_URL_STATUS_USER_PHOTO) {
				gchar *photo = g_strconcat("https:", entity->properties->photo_url, NULL);
				if (g_strcmp0(purple_buddy_icons_get_checksum_for_user(buddy), photo)) {
					PurpleHttpRequest *photo_request = purple_http_request_new(photo);
					
					if (ha->icons_keepalive_pool == NULL) {
						ha->icons_keepalive_pool = purple_http_keepalive_pool_new();
						purple_http_keepalive_pool_set_limit_per_host(ha->icons_keepalive_pool, 4);
					}
					purple_http_request_set_keepalive_pool(photo_request, ha->icons_keepalive_pool);
					
					purple_http_request(ha->pc, photo_request, hangouts_got_buddy_photo, buddy);
					purple_http_request_unref(photo_request);
				}
				g_free(photo);
			}
		}
		
		if (entity->entity_type == PARTICIPANT_TYPE__PARTICIPANT_TYPE_OFF_NETWORK_PHONE) {
			purple_protocol_got_user_status(ha->account, gaia_id, "mobile", NULL);
		}
	}
}

void
hangouts_get_users_information(HangoutsAccount *ha, GList *user_ids)
{
	GetEntityByIdRequest request;
	size_t n_batch_lookup_spec;
	EntityLookupSpec **batch_lookup_spec;
	GList *cur;
	guint i;
	
	get_entity_by_id_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	n_batch_lookup_spec = g_list_length(user_ids);
	batch_lookup_spec = g_new0(EntityLookupSpec *, n_batch_lookup_spec);
	
	for (i = 0, cur = user_ids; cur && cur->data && i < n_batch_lookup_spec; (cur = cur->next), i++) {
		batch_lookup_spec[i] = g_new0(EntityLookupSpec, 1);
		entity_lookup_spec__init(batch_lookup_spec[i]);
		
		batch_lookup_spec[i]->gaia_id = (gchar *) cur->data;
	}
	
	request.batch_lookup_spec = batch_lookup_spec;
	request.n_batch_lookup_spec = n_batch_lookup_spec;
	
	hangouts_pblite_get_entity_by_id(ha, &request, hangouts_got_users_information, NULL);
	
	hangouts_request_header_free(request.request_header);
	for (i = 0; i < n_batch_lookup_spec; i++) {
		g_free(batch_lookup_spec[i]);
	}
	g_free(batch_lookup_spec);
}

static void
hangouts_got_user_info(HangoutsAccount *ha, GetEntityByIdResponse *response, gpointer user_data)
{
	Entity *entity;
	EntityProperties *props;
	PurpleNotifyUserInfo *user_info;
	gchar *who = user_data;
	guint i;
	
	if (response->n_entity_result < 1) {
		g_free(who);
		return;
	}
	
	entity = response->entity_result[0]->entity[0];
	if (entity == NULL || entity->properties == NULL) {
		g_free(who);
		return;
	}
	props = entity->properties;
	
	user_info = purple_notify_user_info_new();
	
	const gchar *type_str;
	switch (entity->entity_type) {
		case PARTICIPANT_TYPE__PARTICIPANT_TYPE_OFF_NETWORK_PHONE: type_str = _("SMS"); break;
		case PARTICIPANT_TYPE__PARTICIPANT_TYPE_GAIA: type_str = _("Hangouts (Gaia)"); break;
		default: type_str = _("Unknown"); break;
	}
	purple_notify_user_info_add_pair_html(user_info, _("Type"), type_str);
	if (props->display_name != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("Display Name"), props->display_name);
	if (props->first_name != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("First Name"), props->first_name);

	if (props->photo_url) {
		gchar *prefix = strncmp(props->photo_url, "//", 2) ? "" : "https:";
		gchar *photo_tag = g_strdup_printf("<a href=\"%s%s\"><img width=\"128\" src=\"%s%s\"/></a>",
		                                   prefix, props->photo_url, prefix, props->photo_url);
		purple_notify_user_info_add_pair_html(user_info, _("Photo"), photo_tag);
		g_free(photo_tag);
	}

	for (i = 0; i < props->n_email; i++) {
		purple_notify_user_info_add_pair_html(user_info, _("Email"), props->email[i]);
	}
	for (i = 0; i < props->n_phone; i++) {
		purple_notify_user_info_add_pair_html(user_info, _("Phone"), props->phone[i]);
	}
	if (props->has_gender) {
		const gchar *gender_str;
		switch (props->gender) {
			case GENDER__GENDER_MALE: gender_str = _("Male"); break;
			case GENDER__GENDER_FEMALE: gender_str = _("Female"); break;
			default: gender_str = _("Unknown"); break;
		}
		purple_notify_user_info_add_pair_html(user_info, _("Gender"), gender_str);
	}
	if (props->canonical_email != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("Canonical Email"), props->canonical_email);
       
	purple_notify_userinfo(ha->pc, who, user_info, NULL, NULL);
       
	g_free(who);
}


void
hangouts_get_info(PurpleConnection *pc, const gchar *who)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	GetEntityByIdRequest request;
	EntityLookupSpec entity_lookup_spec;
	EntityLookupSpec *batch_lookup_spec;
	gchar *who_dup = g_strdup(who);
	
	get_entity_by_id_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	entity_lookup_spec__init(&entity_lookup_spec);
	entity_lookup_spec.gaia_id = who_dup;
	
	batch_lookup_spec = &entity_lookup_spec;
	request.batch_lookup_spec = &batch_lookup_spec;
	request.n_batch_lookup_spec = 1;
	
	hangouts_pblite_get_entity_by_id(ha, &request, hangouts_got_user_info, who_dup);
	
	hangouts_request_header_free(request.request_header);
}

static void
hangouts_got_conversation_events(HangoutsAccount *ha, GetConversationResponse *response, gpointer user_data)
{
	Conversation *conversation;
	const gchar *conv_id;
	PurpleConversation *conv;
	PurpleChatConversation *chatconv;
	guint i;
	PurpleConversationUiOps *convuiops;
	PurpleGroup *temp_group = NULL;
	
	if (response->conversation_state == NULL) {
		if (response->response_header->status == RESPONSE_STATUS__RESPONSE_STATUS_ERROR_INVALID_CONVERSATION) {
			purple_notify_error(ha->pc, _("Invalid conversation"), _("This is not a valid conversation"), _("Please use the Room List to search for a valid conversation"), purple_request_cpar_from_connection(ha->pc));
		} else {
			purple_notify_error(ha->pc, _("Error"), _("An error occurred while fetching the history of the conversation"), NULL, purple_request_cpar_from_connection(ha->pc));
		}
		g_warn_if_reached();
		return;
	}
	
	conversation = response->conversation_state->conversation;
	g_return_if_fail(conversation != NULL);
	conv_id = conversation->conversation_id->id;
	
	//purple_debug_info("hangouts", "got conversation events %s\n", pblite_dump_json((ProtobufCMessage *)response));
	
	if (conversation->type == CONVERSATION_TYPE__CONVERSATION_TYPE_GROUP) {
		chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		if (!chatconv) {
			chatconv = purple_serv_got_joined_chat(ha->pc, g_str_hash(conv_id), conv_id);
			purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
		}
		conv = PURPLE_CONVERSATION(chatconv);
		convuiops = purple_conversation_get_ui_ops(conv);
		
		for (i = 0; i < conversation->n_participant_data; i++) {
			ConversationParticipantData *participant_data = conversation->participant_data[i];
			PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
			const gchar *gaia_id = participant_data->id->gaia_id;
			PurpleChatUser *cb;
			gboolean ui_update_sent = FALSE;
			
			purple_chat_conversation_add_user(chatconv, gaia_id, NULL, cbflags, FALSE);
			cb = purple_chat_conversation_find_user(chatconv, gaia_id);
			purple_chat_user_set_alias(cb, participant_data->fallback_name);
			
			if (convuiops != NULL) {
				// Horrible hack.  Don't try this at home!
				if (convuiops->chat_rename_user != NULL) {
#if PURPLE_VERSION_CHECK(3, 0, 0)
					convuiops->chat_rename_user(chatconv, gaia_id, gaia_id, participant_data->fallback_name);
#else
					convuiops->chat_rename_user(conv, gaia_id, gaia_id, participant_data->fallback_name);
#endif
					ui_update_sent = TRUE;
				} else if (convuiops->chat_update_user != NULL) {
#if PURPLE_VERSION_CHECK(3, 0, 0)
					convuiops->chat_update_user(cb);
#else
					convuiops->chat_update_user(conv, gaia_id);
#endif
					ui_update_sent = TRUE;
				}
			}
			
			if (ui_update_sent == FALSE) {
				// Bitlbee doesn't have the above two functions, lets do an even worse hack
				PurpleBuddy *fakebuddy;
				if (temp_group == NULL) {
					temp_group = purple_blist_find_group("Hangouts Temporary Chat Buddies");
					if (!temp_group)
					{
						temp_group = purple_group_new("Hangouts Temporary Chat Buddies");
						purple_blist_add_group(temp_group, NULL);
					}
				}
				
				fakebuddy = purple_buddy_new(ha->account, gaia_id, participant_data->fallback_name);
				purple_blist_node_set_transient(PURPLE_BLIST_NODE(fakebuddy), TRUE);
				purple_blist_add_buddy(fakebuddy, NULL, temp_group, NULL);
			}
		}
	}

	for (i = 0; i < response->conversation_state->n_event; i++) {
		Event *event = response->conversation_state->event[i];
		//Send event to the hangouts_events.c slaughterhouse
		hangouts_process_conversation_event(ha, conversation, event, response->response_header->current_server_time);
	}
}

void
hangouts_get_conversation_events(HangoutsAccount *ha, const gchar *conv_id, gint64 since_timestamp)
{
	//since_timestamp is in microseconds
	GetConversationRequest request;
	ConversationId conversation_id;
	ConversationSpec conversation_spec;
	EventContinuationToken event_continuation_token;
	
	get_conversation_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	conversation_spec__init(&conversation_spec);
	request.conversation_spec = &conversation_spec;
	
	conversation_id__init(&conversation_id);
	conversation_id.id = (gchar *) conv_id;
	conversation_spec.conversation_id = &conversation_id;
	
	if (since_timestamp > 0) {
		request.has_include_event = TRUE;
		request.include_event = TRUE;
		request.has_max_events_per_conversation = TRUE;
		request.max_events_per_conversation = 50;
		
		event_continuation_token__init(&event_continuation_token);
		event_continuation_token.event_timestamp = since_timestamp;
		request.event_continuation_token = &event_continuation_token;
	}
	
	hangouts_pblite_get_conversation(ha, &request, hangouts_got_conversation_events, NULL);
	
	hangouts_request_header_free(request.request_header);
	
}

static void
hangouts_got_all_events(HangoutsAccount *ha, SyncAllNewEventsResponse *response, gpointer user_data)
{
	guint i, j;
	guint64 sync_timestamp;
	
	//purple_debug_info("hangouts", "%s\n", pblite_dump_json((ProtobufCMessage *)response));
	/*
struct  _SyncAllNewEventsResponse
{
  ProtobufCMessage base;
  ResponseHeader *response_header;
  protobuf_c_boolean has_sync_timestamp;
  uint64_t sync_timestamp;
  size_t n_conversation_state;
  ConversationState **conversation_state;
};

struct  _ConversationState
{
  ProtobufCMessage base;
  ConversationId *conversation_id;
  Conversation *conversation;
  size_t n_event;
  Event **event;
  EventContinuationToken *event_continuation_token;
};

*/
	sync_timestamp = response->sync_timestamp;
	for (i = 0; i < response->n_conversation_state; i++) {
		ConversationState *conversation_state = response->conversation_state[i];
		Conversation *conversation = conversation_state->conversation;
		
		for (j = 0; j < conversation_state->n_event; j++) {
			Event *event = conversation_state->event[j];
			
			hangouts_process_conversation_event(ha, conversation, event, sync_timestamp);
		}
	}
}

void
hangouts_get_all_events(HangoutsAccount *ha, guint64 since_timestamp)
{
	SyncAllNewEventsRequest request;
	
	g_return_if_fail(since_timestamp > 0);
	
	sync_all_new_events_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	request.has_last_sync_timestamp = TRUE;
	request.last_sync_timestamp = since_timestamp;
	
	request.has_max_response_size_bytes = TRUE;
	request.max_response_size_bytes = 1048576; // 1 mibbily bite
	
	hangouts_pblite_sync_all_new_events(ha, &request, hangouts_got_all_events, NULL);
	
	hangouts_request_header_free(request.request_header);
}

GList *
hangouts_chat_info(PurpleConnection *pc)
{
	GList *m = NULL;
	PurpleProtocolChatEntry *pce;

	pce = g_new0(PurpleProtocolChatEntry, 1);
	pce->label = _("Conversation ID");
	pce->identifier = "conv_id";
	pce->required = TRUE;
	m = g_list_append(m, pce);
	
	return m;
}

GHashTable *
hangouts_chat_info_defaults(PurpleConnection *pc, const char *chatname)
{
	GHashTable *defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	
	if (chatname != NULL)
	{
		g_hash_table_insert(defaults, "conv_id", g_strdup(chatname));
	}
	
	return defaults;
}

void
hangouts_join_chat(PurpleConnection *pc, GHashTable *data)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	gchar *conv_id;
	PurpleChatConversation *chatconv;
	
	conv_id = (gchar *)g_hash_table_lookup(data, "conv_id");
	if (conv_id == NULL)
	{
		return;
	}
	
	chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
	if (chatconv != NULL && !purple_chat_conversation_has_left(chatconv)) {
		purple_conversation_present(PURPLE_CONVERSATION(chatconv));
		return;
	}
	
	chatconv = purple_serv_got_joined_chat(pc, g_str_hash(conv_id), conv_id);
	purple_conversation_set_data(PURPLE_CONVERSATION(chatconv), "conv_id", g_strdup(conv_id));
	
	purple_conversation_present(PURPLE_CONVERSATION(chatconv));
	
	//TODO store and use timestamp of last event
	hangouts_get_conversation_events(ha, conv_id, 0);
}


gchar *
hangouts_get_chat_name(GHashTable *data)
{
	gchar *temp;

	if (data == NULL)
		return NULL;
	
	temp = g_hash_table_lookup(data, "conv_id");

	if (temp == NULL)
		return NULL;

	return g_strdup(temp);
}

void
hangouts_add_conversation_to_blist(HangoutsAccount *ha, Conversation *conversation, GHashTable *unique_user_ids)
{
	PurpleGroup *hangouts_group = NULL;
	guint i;
	gchar *conv_id = conversation->conversation_id->id;
	
	if ((conversation->self_conversation_state->delivery_medium_option && conversation->self_conversation_state->delivery_medium_option[0]->delivery_medium->medium_type == DELIVERY_MEDIUM_TYPE__DELIVERY_MEDIUM_GOOGLE_VOICE) 
			|| conversation->network_type[0] == NETWORK_TYPE__NETWORK_TYPE_PHONE) {
		g_hash_table_replace(ha->google_voice_conversations, g_strdup(conv_id), NULL);
		
		if (conversation->self_conversation_state->delivery_medium_option && ha->self_phone == NULL) {
			ha->self_phone = g_strdup(conversation->self_conversation_state->delivery_medium_option[0]->delivery_medium->self_phone->e164);
		}
	}
	
	if (conversation->type == CONVERSATION_TYPE__CONVERSATION_TYPE_ONE_TO_ONE) {
		gchar *other_person = conversation->participant_data[0]->id->gaia_id;
		guint participant_num = 0;
		gchar *other_person_alias;
		
		if (!g_strcmp0(other_person, conversation->self_conversation_state->self_read_state->participant_id->gaia_id)) {
			other_person = conversation->participant_data[1]->id->gaia_id;
			participant_num = 1;
		}
		other_person_alias = conversation->participant_data[participant_num]->fallback_name;
		
		g_hash_table_replace(ha->one_to_ones, g_strdup(conv_id), g_strdup(other_person));
		g_hash_table_replace(ha->one_to_ones_rev, g_strdup(other_person), g_strdup(conv_id));
		
		if (!purple_blist_find_buddy(ha->account, other_person)) {
			if (hangouts_group == NULL) {
				hangouts_group = purple_blist_find_group("Hangouts");
				if (!hangouts_group)
				{
					hangouts_group = purple_group_new("Hangouts");
					purple_blist_add_group(hangouts_group, NULL);
				}
			}
			purple_blist_add_buddy(purple_buddy_new(ha->account, other_person, other_person_alias), NULL, hangouts_group, NULL);
		} else {
			purple_serv_got_alias(ha->pc, other_person, other_person_alias);
		}
		
		if (unique_user_ids == NULL) {
			GList *user_list = g_list_prepend(NULL, other_person);
			hangouts_get_users_presence(ha, user_list);
			g_list_free(user_list);
		}
	} else {
		PurpleChat *chat = purple_blist_find_chat(ha->account, conv_id);
		gchar *name = conversation->name;
		gboolean has_name = name ? TRUE : FALSE;
		
		g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
		
		if (chat == NULL) {
			if (hangouts_group == NULL) {
				hangouts_group = purple_blist_find_group("Hangouts");
				if (!hangouts_group)
				{
					hangouts_group = purple_group_new("Hangouts");
					purple_blist_add_group(hangouts_group, NULL);
				}
			}
			if (!has_name) {
				gchar **name_set = g_new0(gchar *, conversation->n_participant_data + 1);
				for (i = 0; i < conversation->n_participant_data; i++) {
					gchar *p_name = conversation->participant_data[i]->fallback_name;
					if (p_name != NULL) {
						name_set[i] = p_name;
					} else {
						name_set[i] = _("Unknown");
					}
				}
				name = g_strjoinv(", ", name_set);
				g_free(name_set);
			}
			purple_blist_add_chat(purple_chat_new(ha->account, name, hangouts_chat_info_defaults(ha->pc, conv_id)), hangouts_group, NULL);
			if (!has_name)
				g_free(name);
		} else {
			if(has_name && strstr(purple_chat_get_name(chat), _("Unknown")) != NULL) {
				purple_chat_set_alias(chat, name);
			}
		}
	}
	
	
	for (i = 0; i < conversation->n_participant_data; i++) {
		ConversationParticipantData *participant_data = conversation->participant_data[i];
		
		if (participant_data->participant_type != PARTICIPANT_TYPE__PARTICIPANT_TYPE_UNKNOWN) {
			if (participant_data->fallback_name != NULL) {
				purple_serv_got_alias(ha->pc, participant_data->id->gaia_id, participant_data->fallback_name);
			}
			if (unique_user_ids != NULL) {
				g_hash_table_replace(unique_user_ids, participant_data->id->gaia_id, participant_data->id);
			}
		}
	}
}

static void
hangouts_got_conversation_list(HangoutsAccount *ha, SyncRecentConversationsResponse *response, gpointer user_data)
{
	guint i;
	GHashTable *unique_user_ids = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	GList *unique_user_ids_list;
	
	for (i = 0; i < response->n_conversation_state; i++) {
		ConversationState *conversation_state = response->conversation_state[i];
		Conversation *conversation = conversation_state->conversation;
		
		//purple_debug_info("hangouts", "got conversation state %s\n", pblite_dump_json((ProtobufCMessage *)conversation_state));
		hangouts_add_conversation_to_blist(ha, conversation, unique_user_ids);
	}
	//todo mark the account as connected
	
	unique_user_ids_list = g_hash_table_get_keys(unique_user_ids);
	hangouts_get_users_presence(ha, unique_user_ids_list);
	hangouts_get_users_information(ha, unique_user_ids_list);
	g_list_free(unique_user_ids_list);
	g_hash_table_unref(unique_user_ids);
}

void
hangouts_get_conversation_list(HangoutsAccount *ha)
{
	SyncRecentConversationsRequest request;
	SyncFilter sync_filter[1];
	sync_recent_conversations_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	request.has_max_conversations = TRUE;
	request.max_conversations = 100;
	request.has_max_events_per_conversation = TRUE;
	request.max_events_per_conversation = 1;
	
	sync_filter[0] = SYNC_FILTER__SYNC_FILTER_INBOX;
	request.sync_filter = sync_filter;
	request.n_sync_filter = 1;  // Back streets back, alright!
	
	hangouts_pblite_sync_recent_conversations(ha, &request, hangouts_got_conversation_list, NULL);
	
	hangouts_request_header_free(request.request_header);
}


static void
hangouts_got_buddy_photo(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	PurpleBuddy *buddy = user_data;
	PurpleAccount *account = purple_buddy_get_account(buddy);
	const gchar *name = purple_buddy_get_name(buddy);
	PurpleHttpRequest *request = purple_http_conn_get_request(connection);
	const gchar *photo_url = purple_http_request_get_url(request);
	const gchar *response_str;
	gsize response_len;
	gpointer response_dup;
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_debug_error("hangouts", "Failed to get buddy photo for %s from %s\n", name, photo_url);
		return;
	}
	
	response_str = purple_http_response_get_data(response, &response_len);
	response_dup = g_memdup(response_str, response_len);
	purple_buddy_icons_set_for_user(account, name, response_dup, response_len, photo_url);
}

static void
hangouts_got_buddy_list(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	PurpleGroup *hangouts_group = NULL;
	const gchar *response_str;
	gsize response_len;
	JsonObject *obj;
	JsonArray *mergedPerson;
	gsize i, len;
/*
{
	"id": "ListMergedPeople",
	"result": {
		"requestMetadata": {
			"serverTimeMs": "527",
		},
		"selection": {
			"totalCount": "127",
		},
		"mergedPerson": [{
			"personId": "{USER ID}",
			"metadata": {
				"contactGroupId": [
					"family",
					"myContacts",
					"starred"
				 ],
			},
			name": [{
				"displayName": "{USERS NAME}",
			}],
			"photo": [{
				"url": "https://lh5.googleusercontent.com/-iPLHmUq4g_0/AAAAAAAAAAI/AAAAAAAAAAA/j1C9pusixPY/photo.jpg",
				"photoToken": "CAASFTEwOTE4MDY1MTIyOTAyODgxNDcwOBih9d_CAg=="
			}],
			"inAppReachability": [
			 {
			  "metadata": {
			   "container": "PROFILE",
			   "encodedContainerId": "{USER ID}"
			  },
			  "appType": "BABEL",
			  "status": "REACHABLE"
			 }]
		}
*/
	if (purple_http_response_get_error(response) != NULL) {
		purple_debug_error("hangouts", "Failed to download buddy list: %s\n", purple_http_response_get_error(response));
		return;
	}
	
	response_str = purple_http_response_get_data(response, &response_len);
	obj = json_decode_object(response_str, response_len);
	mergedPerson = json_object_get_array_member(json_object_get_object_member(obj, "result"), "mergedPerson");
	len = json_array_get_length(mergedPerson);
	for (i = 0; i < len; i++) {
		JsonNode *node = json_array_get_element(mergedPerson, i);
		JsonObject *person = json_node_get_object(node);
		const gchar *name;
		gchar *alias;
		gchar *photo;
		PurpleBuddy *buddy;
		gchar *reachableAppType = hangouts_json_path_query_string(node, "$.inAppReachability[*].appType", NULL);
		
		if (g_strcmp0(reachableAppType, "BABEL")) {
			//Not a hangouts user
			g_free(reachableAppType);
			continue;
		}
		g_free(reachableAppType);
		
		name = json_object_get_string_member(person, "personId");
		alias = hangouts_json_path_query_string(node, "$.name[*].displayName", NULL);
		photo = hangouts_json_path_query_string(node, "$.photo[*].url", NULL);
		buddy = purple_blist_find_buddy(ha->account, name);
		
		if (buddy == NULL) {
			if (hangouts_group == NULL) {
				hangouts_group = purple_blist_find_group("Hangouts");
				if (!hangouts_group)
				{
					hangouts_group = purple_group_new("Hangouts");
					purple_blist_add_group(hangouts_group, NULL);
				}
			}
			buddy = purple_buddy_new(ha->account, name, alias);
			purple_blist_add_buddy(buddy, NULL, hangouts_group, NULL);
		} else {
			purple_serv_got_alias(ha->pc, name, alias);
		}
		
		if (g_strcmp0(purple_buddy_icons_get_checksum_for_user(buddy), photo)) {
			PurpleHttpRequest *photo_request = purple_http_request_new(photo);
			
			if (ha->icons_keepalive_pool == NULL) {
				ha->icons_keepalive_pool = purple_http_keepalive_pool_new();
				purple_http_keepalive_pool_set_limit_per_host(ha->icons_keepalive_pool, 4);
			}
			purple_http_request_set_keepalive_pool(photo_request, ha->icons_keepalive_pool);
			
			purple_http_request(ha->pc, photo_request, hangouts_got_buddy_photo, buddy);
			purple_http_request_unref(photo_request);
		}
		
		g_free(alias);
		g_free(photo);
	}
}

void
hangouts_get_buddy_list(HangoutsAccount *ha)
{
	gchar *request_data;
	/*
	POST https://clients6.google.com/rpc/plusi?key={KEY}&alt=json 
Authorization: SAPISIDHASH {AUTH HEADER}
Content-Type: application/json
Accept: application/json
Cookie: {COOKIES}
Pragma: no-cache
Cache-Control: no-cache

{"method":"plusi.ozinternal.listmergedpeople","id":"ListMergedPeople","apiVersion":"v2","jsonrpc":"2.0","params":{"pageSelection":{"maxResults":1000},"params":{"personId":"{MY_USER_ID}","collection":6,"hasField":[8,11],"requestMask":{"includeField":[1,2,3,8,9,11,32]},"commonParams":{"includeAffinity":[3]},"extensionSet":{"extensionNames":[4]}}}} 
*/
	request_data = g_strdup_printf("{\"method\":\"plusi.ozinternal.listmergedpeople\",\"id\":\"ListMergedPeople\",\"apiVersion\":\"v2\",\"jsonrpc\":\"2.0\",\"params\":{\"pageSelection\":{\"maxResults\":1000},\"params\":{\"personId\":\"%s\",\"collection\":6,\"hasField\":[8,11],\"requestMask\":{\"includeField\":[1,2,3,8,9,11,32]},\"commonParams\":{\"includeAffinity\":[3]},\"extensionSet\":{\"extensionNames\":[4]}}}}", ha->self_gaia_id);
	
	hangouts_client6_request(ha, "/rpc/plusi?key=" GOOGLE_GPLUS_KEY, HANGOUTS_CONTENT_TYPE_JSON, request_data, -1, HANGOUTS_CONTENT_TYPE_JSON, hangouts_got_buddy_list, ha);
	
	g_free(request_data);

	//TODO blocked users
/* {"method":"plusi.ozinternal.loadblockedpeople","id":"getBlockedUsers","apiVersion":"v2","jsonrpc":"2.0","params":{"includeChatBlocked":true}} */
}

void
hangouts_block_user(PurpleConnection *pc, const char *who)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	gchar *request_data;
	
	request_data = g_strdup_printf("{\"method\":\"plusi.ozinternal.blockuser\",\"id\":\"blockUser\",\"apiVersion\":\"v2\",\"jsonrpc\":\"2.0\",\"params\":{\"membersToBlock\": {\"members\":[{\"memberId\":{\"obfuscatedGaiaId\":\"%s\"}}],\"block\":true}}}", who);
	
	hangouts_client6_request(ha, "/rpc/plusi?key=" GOOGLE_GPLUS_KEY, HANGOUTS_CONTENT_TYPE_JSON, request_data, -1, HANGOUTS_CONTENT_TYPE_JSON, NULL, ha);
	
	g_free(request_data);
}

void
hangouts_unblock_user(PurpleConnection *pc, const char *who)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	gchar *request_data;
	
	request_data = g_strdup_printf("{\"method\":\"plusi.ozinternal.blockuser\",\"id\":\"unblockUser\",\"apiVersion\":\"v2\",\"jsonrpc\":\"2.0\",\"params\":{\"membersToBlock\": {\"members\":[{\"memberId\":{\"obfuscatedGaiaId\":\"%s\"}}],\"block\":false},\"legacyChatUnblock\":true}}", who);
	
	hangouts_client6_request(ha, "/rpc/plusi?key=" GOOGLE_GPLUS_KEY, HANGOUTS_CONTENT_TYPE_JSON, request_data, -1, HANGOUTS_CONTENT_TYPE_JSON, NULL, ha);
	
	g_free(request_data);
}

static Segment **
hangouts_convert_html_to_segments(HangoutsAccount *ha, const gchar *html_message, guint *segments_count)
{
	guint n_segments;
	Segment **segments;
	const gchar *c = html_message;
	GString *text_content;
	Segment *segment;
	guint i;
	GList *segment_list = NULL;
	gboolean is_bold = FALSE, is_italic = FALSE, is_strikethrough = FALSE, is_underline = FALSE;
	gboolean is_link = FALSE;
	gchar *last_link = NULL;
	
	g_return_val_if_fail(c && *c, NULL);
	
	text_content = g_string_new("");
	segment = g_new0(Segment, 1);
	segment__init(segment);
	
	while (c && *c) {
		if(*c == '<') {
			gboolean opening = TRUE;
			GString *tag = g_string_new("");
			c++;
			if(*c == '/') { // closing tag
				opening = FALSE;
				c++;
			}
			while (*c != ' ' && *c != '>') {
				g_string_append_c(tag, *c);
				c++;
			}
			if (text_content->len) {
				segment->text = g_string_free(text_content, FALSE);
				text_content = g_string_new("");
				
				segment->formatting = g_new0(Formatting, 1);
				formatting__init(segment->formatting);
				segment->formatting->has_bold = TRUE;
				segment->formatting->bold = is_bold;
				segment->formatting->has_italics = TRUE;
				segment->formatting->italics = is_italic;
				segment->formatting->has_strikethrough = TRUE;
				segment->formatting->strikethrough = is_strikethrough;
				segment->formatting->has_underline = TRUE;
				segment->formatting->underline = is_underline;
				
				if (is_link) {
					segment->type = SEGMENT_TYPE__SEGMENT_TYPE_LINK;
					if (last_link) {
						segment->link_data = g_new0(LinkData, 1);
						link_data__init(segment->link_data);
						segment->link_data->link_target = g_strdup(last_link);
					}
				}
				
				segment_list = g_list_append(segment_list, segment);
				segment = g_new0(Segment, 1);
				segment__init(segment);
			}
			if (!g_ascii_strcasecmp(tag->str, "BR") ||
					!g_ascii_strcasecmp(tag->str, "BR/")) {
				//Line break, push directly onto the stack
				segment->type = SEGMENT_TYPE__SEGMENT_TYPE_LINE_BREAK;
				segment_list = g_list_append(segment_list, segment);
				segment = g_new0(Segment, 1);
				segment__init(segment);
			} else if (!g_ascii_strcasecmp(tag->str, "B") ||
					!g_ascii_strcasecmp(tag->str, "BOLD") ||
					!g_ascii_strcasecmp(tag->str, "STRONG")) {
				is_bold = opening;
			} else if (!g_ascii_strcasecmp(tag->str, "I") ||
					!g_ascii_strcasecmp(tag->str, "ITALIC") ||
					!g_ascii_strcasecmp(tag->str, "EM")) {
				is_italic = opening;
			} else if (!g_ascii_strcasecmp(tag->str, "S") ||
					!g_ascii_strcasecmp(tag->str, "STRIKE")) {
				is_strikethrough = opening;
			} else if (!g_ascii_strcasecmp(tag->str, "U") ||
					!g_ascii_strcasecmp(tag->str, "UNDERLINE")) {
				is_underline = opening;
			} else if (!g_ascii_strcasecmp(tag->str, "A")) {
				is_link = opening;
				if (opening) {
					while (*c != '>') {
						//Grab HREF for the A
						if (g_ascii_strcasecmp(tag->str, " HREF=")) {
							gchar *href_end;
							c += 6;
							if (*c == '"' || *c == '\'') {
								href_end = strchr(c + 1, *c);
								c++;
							} else {
								//Wow this should not happen, but what the hell
								href_end = MIN(strchr(c, ' '), strchr(c, '>'));
								if (!href_end)
									href_end = strchr(c, '>');
							}
							g_free(last_link);
							
							if (href_end > c) {
								gchar *attrib = g_strndup(c, href_end - c);
								last_link = purple_unescape_text(attrib);
								g_free(attrib);
								
								// Strip out the www.google.com/url?q= bit
								if (purple_account_get_bool(ha->account, "unravel_google_url", FALSE)) {
									PurpleHttpURL *url = purple_http_url_parse(last_link);
									if (purple_strequal(purple_http_url_get_host(url), "www.google.com")) {
										const gchar *path = purple_http_url_get_path(url);
										//apparently the path includes the query string
										if (g_str_has_prefix(path, "/url?q=")) {
											attrib = g_strndup(path + 7, path - strchr(path, '&') - 7);
											g_free(last_link);
											last_link = g_strdup(purple_url_decode(attrib));
											g_free(attrib);
										}
									}
									purple_http_url_free(url);
								}
								
								c = href_end;
								break;
							}
							
							// Shouldn't be here :s
							g_warn_if_reached();
							last_link = NULL;
						}
						c++;
					}
				} else {
					g_free(last_link);
					last_link = NULL;
				}
			}
			while (*c != '>') {
				c++;
			}
			
			c++;
			g_string_free(tag, TRUE);
		} else if(*c == '&') {
			const gchar *plain;
			gint len;
			
			if ((plain = purple_markup_unescape_entity(c, &len)) == NULL) {
				g_string_append_c(text_content, *c);
				len = 1;
			} else {
				g_string_append(text_content, plain);
			}
			c += len;
		} else {
			g_string_append_c(text_content, *c);
			c++;
		}
	}
	
	if (text_content->len) {
		segment->text = g_string_free(text_content, FALSE);
		
		segment->formatting = g_new0(Formatting, 1);
		formatting__init(segment->formatting);
		segment->formatting->has_bold = TRUE;
		segment->formatting->bold = is_bold;
		segment->formatting->has_italics = TRUE;
		segment->formatting->italics = is_italic;
		segment->formatting->has_strikethrough = TRUE;
		segment->formatting->strikethrough = is_strikethrough;
		segment->formatting->has_underline = TRUE;
		segment->formatting->underline = is_underline;
				
		if (is_link) {
			segment->type = SEGMENT_TYPE__SEGMENT_TYPE_LINK;
			if (last_link) {
				segment->link_data = g_new0(LinkData, 1);
				link_data__init(segment->link_data);
				segment->link_data->link_target = g_strdup(last_link);
			}
		}
		
		segment_list = g_list_append(segment_list, segment);
	}
	
	n_segments = g_list_length(segment_list);
	segments = g_new0(Segment *, n_segments + 1);
	
	for (i = 0; segment_list && segment_list->data; i++) {
		segments[i] = segment_list->data;
		
		segment_list = g_list_delete_link(segment_list, segment_list);
	}
	
	if (segments_count != NULL) {
		*segments_count = n_segments;
	}
	
	g_free(last_link);
	return segments;
}

static void
hangouts_free_segments(Segment **segments)
{
	guint i;
	for (i = 0; segments[i]; i++) {
		g_free(segments[i]->text);
		g_free(segments[i]->formatting);
		if (segments[i]->link_data != NULL) {
			g_free(segments[i]->link_data->link_target);
		}
		g_free(segments[i]->link_data);
		g_free(segments[i]);
	}
	
	g_free(segments);
}

//Received the photoid of the sent image to be able to attach to an outgoing message
static void
hangouts_conversation_send_image_part2_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha;
	gchar *conv_id;
	gchar *photoid;
	const gchar *response_raw;
	size_t response_len;
	JsonNode *node;
	PurpleConnection *pc = purple_http_conn_get_purple_connection(connection);
	SendChatMessageRequest request;
	ExistingMedia existing_media;
	Photo photo;
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_notify_error(pc, _("Image Send Error"), _("There was an error sending the image"), purple_http_response_get_error(response), purple_request_cpar_from_connection(pc));
		g_dataset_destroy(connection);
		return;
	}
	
	ha = user_data;
	response_raw = purple_http_response_get_data(response, &response_len);
	purple_debug_info("hangouts", "image_part2_cb %s\n", response_raw);
	node = json_decode(response_raw, response_len);
	
	photoid = hangouts_json_path_query_string(node, "$..photoid", NULL);
	conv_id = g_dataset_get_data(connection, "conv_id");
	
	send_chat_message_request__init(&request);
	existing_media__init(&existing_media);
	photo__init(&photo);
	
	request.request_header = hangouts_get_request_header(ha);
	request.event_request_header = hangouts_get_event_request_header(ha, conv_id);
	
	photo.photo_id = photoid;
	existing_media.photo = &photo;
	request.existing_media = &existing_media;
	
	hangouts_pblite_send_chat_message(ha, &request, NULL, NULL);
	
	g_hash_table_insert(ha->sent_message_ids, g_strdup_printf("%" G_GUINT64_FORMAT, request.event_request_header->client_generated_id), NULL);
	
	g_free(photoid);
	g_dataset_destroy(connection);
	hangouts_request_header_free(request.request_header);
	hangouts_event_request_header_free(request.event_request_header);
	json_node_free(node);
}

//Received the url to upload the image data to
static void
hangouts_conversation_send_image_part1_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	HangoutsAccount *ha;
	gchar *conv_id;
	PurpleImage *image;
	gchar *upload_url;
	JsonNode *node;
	PurpleHttpRequest *request;
	PurpleHttpConnection *new_connection;
	PurpleConnection *pc = purple_http_conn_get_purple_connection(connection);
	const gchar *response_raw;
	size_t response_len;
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_notify_error(pc, _("Image Send Error"), _("There was an error sending the image"), purple_http_response_get_error(response), purple_request_cpar_from_connection(pc));
		g_dataset_destroy(connection);
		return;
	}
	
	ha = user_data;
	conv_id = g_dataset_get_data(connection, "conv_id");
	image = g_dataset_get_data(connection, "image");
	
	response_raw = purple_http_response_get_data(response, &response_len);
	purple_debug_info("hangouts", "image_part1_cb %s\n", response_raw);
	node = json_decode(response_raw, response_len);
	
	upload_url = hangouts_json_path_query_string(node, "$..putInfo.url", NULL);
	
	request = purple_http_request_new(upload_url);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/octet-stream");
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_contents(request, purple_image_get_data(image), purple_image_get_size(image));
	
	new_connection = purple_http_request(ha->pc, request, hangouts_conversation_send_image_part2_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(new_connection, "conv_id", g_strdup(conv_id), g_free);
	
	g_free(upload_url);
	g_dataset_destroy(connection);
	json_node_free(node);
}

static void
hangouts_conversation_send_image(HangoutsAccount *ha, const gchar *conv_id, PurpleImage *image)
{
	PurpleHttpRequest *request;
	PurpleHttpConnection *connection;
	gchar *postdata;
	gchar *filename;
	
	filename = (gchar *)purple_image_get_path(image);
	if (filename != NULL) {
		filename = g_path_get_basename(filename);
	} else {
		filename = g_strdup_printf("purple%d.%s", g_random_int(), purple_image_get_extension(image));
	}
	
	postdata = g_strdup_printf("{\"protocolVersion\":\"0.8\",\"createSessionRequest\":{\"fields\":[{\"external\":{\"name\":\"file\",\"filename\":\"%s\",\"put\":{},\"size\":%" G_GSIZE_FORMAT "}},{\"inlined\":{\"name\":\"client\",\"content\":\"hangouts\",\"contentType\":\"text/plain\"}}]}}", filename, (gsize) purple_image_get_size(image));
	
	request = purple_http_request_new(HANGOUTS_IMAGE_UPLOAD_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded;charset=UTF-8");
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_contents(request, postdata, -1);
	purple_http_request_set_max_redirects(request, 0);
	
	connection = purple_http_request(ha->pc, request, hangouts_conversation_send_image_part1_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
	g_dataset_set_data_full(connection, "image", image, NULL);
	
	g_free(filename);
	g_free(postdata);
}

static void
hangouts_conversation_check_message_for_images(HangoutsAccount *ha, const gchar *conv_id, const gchar *message)
{
	const gchar *img;
	
	if ((img = strstr(message, "<img ")) || (img = strstr(message, "<IMG "))) {
		const gchar *id;
		const gchar *close = strchr(img, '>');
		
		if (((id = strstr(img, "ID=\"")) || (id = strstr(img, "id=\""))) &&
				id < close) {
			int imgid = atoi(id + 4);
			PurpleImage *image = purple_image_store_get(imgid);
			
			if (image != NULL) {
				hangouts_conversation_send_image(ha, conv_id, image);
			}
		}
	}
}

static gint
hangouts_conversation_send_message(HangoutsAccount *ha, const gchar *conv_id, const gchar *message)
{
	SendChatMessageRequest request;
	MessageContent message_content;
	Segment **segments;
	guint n_segments;
	
	//Check for any images to send first
	hangouts_conversation_check_message_for_images(ha, conv_id, message);
	
	send_chat_message_request__init(&request);
	message_content__init(&message_content);
	
	segments = hangouts_convert_html_to_segments(ha, message, &n_segments);
	message_content.segment = segments;
	message_content.n_segment = n_segments;
	
	request.request_header = hangouts_get_request_header(ha);
	request.event_request_header = hangouts_get_event_request_header(ha, conv_id);
	request.message_content = &message_content;
	
	//purple_debug_info("hangouts", "%s\n", pblite_dump_json((ProtobufCMessage *)&request)); //leaky
	
	//TODO listen to response
	hangouts_pblite_send_chat_message(ha, &request, NULL, NULL);
	
	g_hash_table_insert(ha->sent_message_ids, g_strdup_printf("%" G_GUINT64_FORMAT, request.event_request_header->client_generated_id), NULL);
	
	hangouts_free_segments(segments);
	hangouts_request_header_free(request.request_header);
	hangouts_event_request_header_free(request.event_request_header);
	
	return 1;
}


gint
hangouts_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *who = purple_message_get_recipient(msg);
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
#endif
	
	HangoutsAccount *ha;
	const gchar *conv_id;
	
	ha = purple_connection_get_protocol_data(pc);
	conv_id = g_hash_table_lookup(ha->one_to_ones_rev, who);
	if (conv_id == NULL) {
		if (G_UNLIKELY(!hangouts_is_valid_id(who))) {
			hangouts_search_users_text(ha, who);
			return -1;
		}
		
		//We don't have any known conversations for this person
		hangouts_create_conversation(ha, TRUE, who, message);
	}
	
	return hangouts_conversation_send_message(ha, conv_id, message);
}

gint
hangouts_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *message, PurpleMessageFlags flags)
{
#endif
	
	HangoutsAccount *ha;
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	gint ret;
	
	ha = purple_connection_get_protocol_data(pc);
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (!conv_id) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
		g_return_val_if_fail(conv_id, -1);
	}
	g_return_val_if_fail(g_hash_table_contains(ha->group_chats, conv_id), -1);
	
	ret = hangouts_conversation_send_message(ha, conv_id, message);
	if (ret > 0) {
		purple_serv_got_chat_in(pc, g_str_hash(conv_id), ha->self_gaia_id, PURPLE_MESSAGE_SEND, message, time(NULL));
	}
	return ret;
}

guint
hangouts_send_typing(PurpleConnection *pc, const gchar *who, PurpleIMTypingState state)
{
	HangoutsAccount *ha;
	const gchar *conv_id;
	SetTypingRequest request;
	ConversationId conversation_id;
	
	ha = purple_connection_get_protocol_data(pc);
	conv_id = g_hash_table_lookup(ha->one_to_ones_rev, who);
	g_return_val_if_fail(conv_id, -1); //TODO create new conversation for this new person
	
	set_typing_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	conversation_id__init(&conversation_id);
	conversation_id.id = (gchar *) conv_id;
	request.conversation_id = &conversation_id;
	
	request.has_type = TRUE;
	switch(state) {
		case PURPLE_IM_TYPING:
			request.type = TYPING_TYPE__TYPING_TYPE_STARTED;
			break;
		
		case PURPLE_IM_TYPED:
			request.type = TYPING_TYPE__TYPING_TYPE_PAUSED;
			break;
		
		case PURPLE_IM_NOT_TYPING:
		default:
			request.type = TYPING_TYPE__TYPING_TYPE_STOPPED;
			break;
	}
	
	//TODO listen to response
	//TODO dont send STOPPED if we just sent a message
	hangouts_pblite_set_typing(ha, &request, NULL, NULL);
	
	hangouts_request_header_free(request.request_header);
	
	return 20;
}

void
hangouts_chat_leave_by_conv_id(PurpleConnection *pc, const gchar *conv_id)
{
	HangoutsAccount *ha;
	RemoveUserRequest request;
	
	g_return_if_fail(conv_id);
	ha = purple_connection_get_protocol_data(pc);
	g_return_if_fail(g_hash_table_contains(ha->group_chats, conv_id));
	
	remove_user_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	request.event_request_header = hangouts_get_event_request_header(ha, conv_id);
	
	//XX do we need to see if this was successful, or does it just come through as a new event?
	hangouts_pblite_remove_user(ha, &request, NULL, NULL);
	
	hangouts_request_header_free(request.request_header);
	hangouts_event_request_header_free(request.event_request_header);
	
	g_hash_table_remove(ha->group_chats, conv_id);
}

void 
hangouts_chat_leave(PurpleConnection *pc, int id)
{
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	return hangouts_chat_leave_by_conv_id(pc, conv_id);
}


static void 
hangouts_created_conversation(HangoutsAccount *ha, CreateConversationResponse *response, gpointer user_data)
{
	Conversation *conversation = response->conversation;
	gchar *message = user_data;
	const gchar *conv_id;
	
	gchar *dump = pblite_dump_json((ProtobufCMessage *) response);
	purple_debug_info("hangouts", "%s\n", dump);
	g_free(dump);
	
	if (conversation == NULL) {
		purple_debug_error("hangouts", "Could not create conversation\n");
		g_free(message);
		return;
	}
	
	hangouts_add_conversation_to_blist(ha, conversation, NULL);
	conv_id = conversation->conversation_id->id;
	hangouts_get_conversation_events(ha, conv_id, 0);
	
	if (message != NULL) {
		hangouts_conversation_send_message(ha, conv_id, message);
		g_free(message);
	}
}

void
hangouts_create_conversation(HangoutsAccount *ha, gboolean is_one_to_one, const char *who, const gchar *optional_message)
{
	CreateConversationRequest request;
	gchar *message_dup = NULL;
	
	create_conversation_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	request.has_type = TRUE;
	if (is_one_to_one) {
		request.type = CONVERSATION_TYPE__CONVERSATION_TYPE_ONE_TO_ONE;
	} else {
		request.type = CONVERSATION_TYPE__CONVERSATION_TYPE_GROUP;
	}
	
	request.n_invitee_id = 1;
	request.invitee_id = g_new0(InviteeID *, 1);
	request.invitee_id[0] = g_new0(InviteeID, 1);
	invitee_id__init(request.invitee_id[0]);
	request.invitee_id[0]->gaia_id = g_strdup(who);
	
	request.has_client_generated_id = TRUE;
	request.client_generated_id = g_random_int();
	
	if (optional_message != NULL) {
		message_dup = g_strdup(optional_message);
	}
	
	hangouts_pblite_create_conversation(ha, &request, hangouts_created_conversation, message_dup);
	
	g_free(request.invitee_id[0]->gaia_id);
	g_free(request.invitee_id[0]);
	g_free(request.invitee_id);
	hangouts_request_header_free(request.request_header);
}

void
hangouts_archive_conversation(HangoutsAccount *ha, const gchar *conv_id)
{
	ModifyConversationViewRequest request;
	ConversationId conversation_id;
	
	if (conv_id == NULL) {
		return;
	}
	
	modify_conversation_view_request__init(&request);
	conversation_id__init(&conversation_id);
	
	conversation_id.id = (gchar *)conv_id;
	
	request.request_header = hangouts_get_request_header(ha);
	request.conversation_id = &conversation_id;
	request.has_new_view = TRUE;
	request.new_view = CONVERSATION_VIEW__CONVERSATION_VIEW_ARCHIVED;
	request.has_last_event_timestamp = TRUE;
	request.last_event_timestamp = ha->last_event_timestamp;
	
	hangouts_pblite_modify_conversation_view(ha, &request, NULL, NULL);
	
	hangouts_request_header_free(request.request_header);
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		gchar *buddy_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
		
		g_hash_table_remove(ha->one_to_ones_rev, buddy_id);
		g_hash_table_remove(ha->one_to_ones, conv_id);
	} else {
		g_hash_table_remove(ha->group_chats, conv_id);
	}
}

void
hangouts_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata)
{
	if(PURPLE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *) node;
		HangoutsAccount *ha;
		
		if (userdata) {
			ha = userdata;
		} else {
			PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
			ha = purple_connection_get_protocol_data(pc);
		}
		
		hangouts_create_conversation(ha, FALSE, purple_buddy_get_name(buddy), NULL);
	}
}

void
hangouts_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who)
{
	HangoutsAccount *ha;
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	AddUserRequest request;
	
	ha = purple_connection_get_protocol_data(pc);
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	add_user_request__init(&request);
	
	request.request_header = hangouts_get_request_header(ha);
	request.event_request_header = hangouts_get_event_request_header(ha, conv_id);
	
	request.n_invitee_id = 1;
	request.invitee_id = g_new0(InviteeID *, 1);
	request.invitee_id[0] = g_new0(InviteeID, 1);
	invitee_id__init(request.invitee_id[0]);
	request.invitee_id[0]->gaia_id = g_strdup(who);
	
	hangouts_pblite_add_user(ha, &request, NULL, NULL);
	
	g_free(request.invitee_id[0]->gaia_id);
	g_free(request.invitee_id[0]);
	g_free(request.invitee_id);
	hangouts_request_header_free(request.request_header);
	hangouts_event_request_header_free(request.event_request_header);
}

#define PURPLE_CONVERSATION_IS_VALID(conv) (g_list_find(purple_conversations_get_all(), conv) != NULL)

gboolean
hangouts_mark_conversation_seen_timeout(gpointer convpointer)
{
	PurpleConversation *conv = convpointer;
	PurpleAccount *account;
	PurpleConnection *pc;
	HangoutsAccount *ha;
	UpdateWatermarkRequest request;
	ConversationId conversation_id;
	const gchar *conv_id = NULL;
	gint64 *last_read_timestamp_ptr, last_read_timestamp = 0;
	gint64 *last_event_timestamp_ptr, last_event_timestamp = 0;
	
	if (!PURPLE_CONVERSATION_IS_VALID(conv))
		return FALSE;
	account = purple_conversation_get_account(conv);
	if (account == NULL || !purple_account_is_connected(account))
		return FALSE;
	pc = purple_account_get_connection(account);
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return FALSE;
	
	purple_conversation_set_data(conv, "mark_seen_timeout", NULL);
	
	ha = purple_connection_get_protocol_data(pc);
	
	if (!purple_presence_is_status_primitive_active(purple_account_get_presence(ha->account), PURPLE_STATUS_AVAILABLE)) {
		// We're not here
		return FALSE;
	}
	
	last_read_timestamp_ptr = (gint64 *)purple_conversation_get_data(conv, "last_read_timestamp");
	if (last_read_timestamp_ptr != NULL) {
		last_read_timestamp = *last_read_timestamp_ptr;
	}
	last_event_timestamp_ptr = (gint64 *)purple_conversation_get_data(conv, "last_event_timestamp");
	if (last_event_timestamp_ptr != NULL) {
		last_event_timestamp = *last_event_timestamp_ptr;
	}
	
	if (last_event_timestamp <= last_read_timestamp) {
		return FALSE;
	}
	
	update_watermark_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	conv_id = purple_conversation_get_data(conv, "conv_id");
	if (conv_id == NULL) {
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			conv_id = g_hash_table_lookup(ha->one_to_ones_rev, purple_conversation_get_name(conv));
		} else {
			conv_id = purple_conversation_get_name(conv);
		}
	}
	conversation_id__init(&conversation_id);
	conversation_id.id = (gchar *) conv_id;
	request.conversation_id = &conversation_id;
	
	request.has_last_read_timestamp = TRUE;
	request.last_read_timestamp = last_event_timestamp;
	
	hangouts_pblite_update_watermark(ha, &request, (HangoutsPbliteUpdateWatermarkResponseFunc)hangouts_default_response_dump, NULL);
	
	hangouts_request_header_free(request.request_header);
	
	if (last_read_timestamp_ptr == NULL) {
		last_read_timestamp_ptr = g_new0(gint64, 1);
	}
	*last_read_timestamp_ptr = last_event_timestamp;
	purple_conversation_set_data(conv, "last_read_timestamp", last_read_timestamp_ptr);
	
	return FALSE;
}

void
hangouts_mark_conversation_seen(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	gint mark_seen_timeout;
	PurpleConnection *pc;
	
	if (type != PURPLE_CONVERSATION_UPDATE_UNSEEN)
		return;
	
	if (!purple_conversation_has_focus(conv))
		return;
	
	pc = purple_conversation_get_connection(conv);
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return;
	
	if (g_strcmp0(purple_protocol_get_id(purple_connection_get_protocol(pc)), HANGOUTS_PLUGIN_ID))
		return;
	
	mark_seen_timeout = GPOINTER_TO_INT(purple_conversation_get_data(conv, "mark_seen_timeout"));
	
	if (mark_seen_timeout) {
		purple_timeout_remove(mark_seen_timeout);
	}
	
	mark_seen_timeout = purple_timeout_add_seconds(1, hangouts_mark_conversation_seen_timeout, conv);
	
	purple_conversation_set_data(conv, "mark_seen_timeout", GINT_TO_POINTER(mark_seen_timeout));
	
	hangouts_set_active_client(pc);
}

void
hangouts_set_status(PurpleAccount *account, PurpleStatus *status)
{
	SetPresenceRequest request;
	PurpleConnection *pc = purple_account_get_connection(account);
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	Segment **segments = NULL;
	const gchar *message;
	DndSetting dnd_setting;
	PresenceStateSetting presence_state_setting;
	MoodSetting mood_setting;
	MoodMessage mood_message;
	MoodContent mood_content;
	guint n_segments;

	set_presence_request__init(&request);
	request.request_header = hangouts_get_request_header(ha);
	
	//available:
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_AVAILABLE) {
		presence_state_setting__init(&presence_state_setting);
		presence_state_setting.has_timeout_secs = TRUE;
		presence_state_setting.timeout_secs = 720;
		presence_state_setting.has_type = TRUE;
		presence_state_setting.type = CLIENT_PRESENCE_STATE_TYPE__CLIENT_PRESENCE_STATE_DESKTOP_ACTIVE;
		request.presence_state_setting = &presence_state_setting;
	}
	
	//away
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_AWAY) {
		presence_state_setting__init(&presence_state_setting);
		presence_state_setting.has_timeout_secs = TRUE;
		presence_state_setting.timeout_secs = 720;
		presence_state_setting.has_type = TRUE;
		presence_state_setting.type = CLIENT_PRESENCE_STATE_TYPE__CLIENT_PRESENCE_STATE_DESKTOP_IDLE;
		request.presence_state_setting = &presence_state_setting;
	}
	
	//do-not-disturb
	dnd_setting__init(&dnd_setting);
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_UNAVAILABLE) {
		dnd_setting.has_do_not_disturb = TRUE;
		dnd_setting.do_not_disturb = TRUE;
		dnd_setting.has_timeout_secs = TRUE;
		dnd_setting.timeout_secs = 172800;
	} else {
		dnd_setting.has_do_not_disturb = TRUE;
		dnd_setting.do_not_disturb = FALSE;
	}
	request.dnd_setting = &dnd_setting;
	
	//has message?
	message = purple_status_get_attr_string(status, "message");
	if (message != NULL) {
		mood_setting__init(&mood_setting);
		mood_message__init(&mood_message);
		mood_content__init(&mood_content);
		
		if (*message) {
			segments = hangouts_convert_html_to_segments(ha, message, &n_segments);
			mood_content.segment = segments;
			mood_content.n_segment = n_segments;
		}
		
		mood_message.mood_content = &mood_content;
		mood_setting.mood_message = &mood_message;
		request.mood_setting = &mood_setting;
	}

	hangouts_pblite_set_presence(ha, &request, (HangoutsPbliteSetPresenceResponseFunc)hangouts_default_response_dump, NULL);
	
	hangouts_request_header_free(request.request_header);
	
	if (segments != NULL) {
		hangouts_free_segments(segments);
	}
}


static void
hangouts_roomlist_got_list(HangoutsAccount *ha, SyncRecentConversationsResponse *response, gpointer user_data)
{
	PurpleRoomlist *roomlist = user_data;
	guint i, j;
	
	for (i = 0; i < response->n_conversation_state; i++) {
		ConversationState *conversation_state = response->conversation_state[i];
		Conversation *conversation = conversation_state->conversation;
		
		if (conversation->type == CONVERSATION_TYPE__CONVERSATION_TYPE_GROUP) {
			gchar *users = NULL;
			gchar **users_set = g_new0(gchar *, conversation->n_participant_data + 1);
			gchar *name = conversation->name;
			PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, conversation->conversation_id->id, NULL);
			
			purple_roomlist_room_add_field(roomlist, room, conversation->conversation_id->id);
			
			for (j = 0; j < conversation->n_participant_data; j++) {
				gchar *p_name = conversation->participant_data[j]->fallback_name;
				if (p_name != NULL) {
					users_set[j] = p_name;
				} else {
					users_set[j] = _("Unknown");
				}
			}
			users = g_strjoinv(", ", users_set);
			g_free(users_set);
			purple_roomlist_room_add_field(roomlist, room, users);
			g_free(users);
			
			purple_roomlist_room_add_field(roomlist, room, name);
			
			purple_roomlist_room_add(roomlist, room);
		}
	}
	
	purple_roomlist_set_in_progress(roomlist, FALSE);
}

PurpleRoomlist *
hangouts_roomlist_get_list(PurpleConnection *pc)
{
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	PurpleRoomlist *roomlist;
	GList *fields = NULL;
	PurpleRoomlistField *f;
	
	roomlist = purple_roomlist_new(ha->account);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("ID"), "chatname", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Name"), "name", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(roomlist, fields);
	purple_roomlist_set_in_progress(roomlist, TRUE);
	
	{
		//Stolen from hangouts_get_conversation_list()
		SyncRecentConversationsRequest request;
		SyncFilter sync_filter[1];
		sync_recent_conversations_request__init(&request);
		
		request.request_header = hangouts_get_request_header(ha);
		request.has_max_conversations = TRUE;
		request.max_conversations = 100;
		request.has_max_events_per_conversation = TRUE;
		request.max_events_per_conversation = 1;
		
		sync_filter[0] = SYNC_FILTER__SYNC_FILTER_INBOX;
		request.sync_filter = sync_filter;
		request.n_sync_filter = 1;  // Back streets back, alright!
		
		hangouts_pblite_sync_recent_conversations(ha, &request, hangouts_roomlist_got_list, roomlist);
		
		hangouts_request_header_free(request.request_header);
	}
	
	
	return roomlist;
}


