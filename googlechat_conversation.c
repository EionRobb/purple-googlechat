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

#include "googlechat_conversation.h"

#include "googlechat.pb-c.h"
#include "googlechat_connection.h"
#include "googlechat_events.h"

#include <string.h>
#include <glib.h>

#include <purple.h>
#include "glibcompat.h"
#include "image-store.h"

// From googlechat_pblite
gchar *pblite_dump_json(ProtobufCMessage *message);

RequestHeader *
googlechat_get_request_header(GoogleChatAccount *ha)
{
	RequestHeader *header = g_new0(RequestHeader, 1);
	request_header__init(header);
	
	header->has_client_type = TRUE;
	header->client_type = REQUEST_HEADER__CLIENT_TYPE__IOS;
	
	header->has_client_version = TRUE;
	header->client_version = 2440378181258;
	
	return header;
}

void
googlechat_request_header_free(RequestHeader *header)
{
	g_free(header);
}

static void 
googlechat_got_self_user_status(GoogleChatAccount *ha, GetSelfUserStatusResponse *response, gpointer user_data)
{
	UserStatus *self_status = response->user_status;
	
	g_return_if_fail(self_status);
	
	g_free(ha->self_gaia_id);
	ha->self_gaia_id = g_strdup(self_status->user_id->id);
	purple_connection_set_display_name(ha->pc, ha->self_gaia_id);
	purple_account_set_string(ha->account, "self_gaia_id", ha->self_gaia_id);
	
	// TODO find self display name
	// const gchar *alias = purple_account_get_private_alias(ha->account);
	// if (alias == NULL || *alias == '\0') {
		// purple_account_set_private_alias(ha->account, self_status->properties->display_name);
	// }
	
	googlechat_get_buddy_list(ha);
}

void
googlechat_get_self_user_status(GoogleChatAccount *ha)
{
	GetSelfUserStatusRequest request;
	get_self_user_status_request__init(&request);
	
	request.request_header = googlechat_get_request_header(ha);
	
	googlechat_api_get_self_user_status(ha, &request, googlechat_got_self_user_status, NULL);
	
	googlechat_request_header_free(request.request_header);
	
	if (ha->last_event_timestamp != 0) {
		googlechat_get_all_events(ha, ha->last_event_timestamp);
	}
}

static void
googlechat_got_users_presence(GoogleChatAccount *ha, GetUserPresenceResponse *response, gpointer user_data)
{
	guint i;
	
	for (i = 0; i < response->n_user_presences; i++) {
		UserPresence *user_presence = response->user_presences[i];
		UserStatus *user_status = user_presence->user_status;
		
		const gchar *user_id = user_presence->user_id->id;
		const gchar *status_id = NULL;
		gchar *message = NULL;
		
		gboolean available = FALSE;
		gboolean reachable = FALSE;
		if (user_presence->dnd_state == DND_STATE__STATE__AVAILABLE) {
			reachable = TRUE;
		}
		if (user_presence->presence == PRESENCE__ACTIVE) {
			available = TRUE;
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
		
		if (user_status != NULL && user_status->custom_status) {
			const gchar *status_text = user_status->custom_status->status_text;
			
			if (status_text && *status_text) {
				message = g_strdup(status_text);
			}
		}
		
		if (message != NULL) {
			purple_protocol_got_user_status(ha->account, user_id, status_id, "message", message, NULL);
			g_free(message);
		} else {
			purple_protocol_got_user_status(ha->account, user_id, status_id, NULL);
		}
	}
}

void
googlechat_get_users_presence(GoogleChatAccount *ha, GList *user_ids)
{
	GetUserPresenceRequest request;
	UserId **user_id;
	guint n_user_id;
	GList *cur;
	guint i;
	
	get_user_presence_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	n_user_id = g_list_length(user_ids);
	user_id = g_new0(UserId *, n_user_id);
	
	for (i = 0, cur = user_ids; cur && cur->data && i < n_user_id; (cur = cur->next), i++) {
		gchar *who = (gchar *) cur->data;
		
		if (G_UNLIKELY(!googlechat_is_valid_id(who))) {
			i--;
			n_user_id--;
		} else {
			user_id[i] = g_new0(UserId, 1);
			user_id__init(user_id[i]);
			user_id[i]->id = who;
		}
	}
	
	request.user_ids = user_id;
	request.n_user_ids = n_user_id;
	
	request.include_user_status = TRUE;
	request.has_include_user_status = TRUE;
	request.include_active_until = TRUE;
	request.has_include_active_until = TRUE;

	googlechat_api_get_user_presence(ha, &request, googlechat_got_users_presence, NULL);
	
	googlechat_request_header_free(request.request_header);
	for (i = 0; i < n_user_id; i++) {
		g_free(user_id[i]);
	}
	g_free(user_id);
}

gboolean
googlechat_poll_buddy_status(gpointer userdata)
{
	GoogleChatAccount *ha = userdata;
	GSList *buddies, *i;
	GList *user_list = NULL;
	
	if (!PURPLE_CONNECTION_IS_CONNECTED(ha->pc)) {
		return FALSE;
	}
	
	buddies = purple_blist_find_buddies(ha->account, NULL);
	for(i = buddies; i; i = i->next) {
		PurpleBuddy *buddy = i->data;
		user_list = g_list_prepend(user_list, (gpointer) purple_buddy_get_name(buddy));
	}
	
	googlechat_get_users_presence(ha, user_list);
	
	g_slist_free(buddies);
	g_list_free(user_list);
	
	return TRUE;
}

static void googlechat_got_buddy_photo(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data);

static void
googlechat_got_users_information_member(GoogleChatAccount *ha, Member *member)
{
	if (member == NULL || member->user == NULL) {
		return;
	}
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	if (member->profile_case != MEMBER__PROFILE_USER) {
		return;
	}
#endif
	User *user = member->user;
	const gchar *gaia_id = user->user_id ? user->user_id->id : NULL;
	
	if (gaia_id != NULL) {
		PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, gaia_id);
		
		// Give a best-guess for the buddy's alias
		if (user->name)
			purple_serv_got_alias(ha->pc, gaia_id, user->name);
		else if (user->email)
			purple_serv_got_alias(ha->pc, gaia_id, user->email);
		//TODO first+last name
		
		// Set the buddy photo, if it's real
		if (user->avatar_url != NULL) {
			const gchar *photo = user->avatar_url;
			if (!purple_strequal(purple_buddy_icons_get_checksum_for_user(buddy), photo)) {
				PurpleHttpRequest *photo_request = purple_http_request_new(photo);
				
				if (ha->icons_keepalive_pool == NULL) {
					ha->icons_keepalive_pool = purple_http_keepalive_pool_new();
					purple_http_keepalive_pool_set_limit_per_host(ha->icons_keepalive_pool, 4);
				}
				purple_http_request_set_keepalive_pool(photo_request, ha->icons_keepalive_pool);
				
				purple_http_request(ha->pc, photo_request, googlechat_got_buddy_photo, buddy);
				purple_http_request_unref(photo_request);
			}
		}
	}
	
	//TODO - process user->deleted == TRUE;
}

static void
googlechat_got_users_information(GoogleChatAccount *ha, GetMembersResponse *response, gpointer user_data)
{
	guint i;
	
	for (i = 0; i < response->n_member_profiles; i++) {
		Member *member = response->member_profiles[i]->member;
		googlechat_got_users_information_member(ha, member);
	}
	
	for (i = 0; i < response->n_members; i++) {
		Member *member = response->members[i];
		googlechat_got_users_information_member(ha, member);
	}
}

void
googlechat_get_users_information(GoogleChatAccount *ha, GList *user_ids)
{
	GetMembersRequest request;
	size_t n_member_ids;
	MemberId **member_ids;
	GList *cur;
	guint i;
	
	get_members_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	n_member_ids = g_list_length(user_ids);
	member_ids = g_new0(MemberId *, n_member_ids);
	
	for (i = 0, cur = user_ids; cur && cur->data && i < n_member_ids; (cur = cur->next), i++) {
		gchar *who = (gchar *) cur->data;
		
		if (G_UNLIKELY(!googlechat_is_valid_id(who))) {
			i--;
			n_member_ids--;
		}
		
		member_ids[i] = g_new0(MemberId, 1);
		member_id__init(member_ids[i]);
		
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		member_ids[i]->id_case = MEMBER_ID__ID_USER_ID;
#endif
		member_ids[i]->user_id = g_new0(UserId, 1);
		user_id__init(member_ids[i]->user_id);
		member_ids[i]->user_id->id = (gchar *) cur->data;
		
	}
	
	request.member_ids = member_ids;
	request.n_member_ids = n_member_ids;
	
	googlechat_api_get_members(ha, &request, googlechat_got_users_information, NULL);
	
	googlechat_request_header_free(request.request_header);
	for (i = 0; i < n_member_ids; i++) {
		g_free(member_ids[i]->user_id);
		g_free(member_ids[i]);
	}
	g_free(member_ids);
}

static void
googlechat_got_user_info(GoogleChatAccount *ha, GetMembersResponse *response, gpointer user_data)
{
	Member *member = NULL;
	PurpleNotifyUserInfo *user_info;
	gchar *who = user_data;
	
	if (response->n_members > 0) {
		member = response->members[0];
	} else if (response->n_member_profiles > 0) {
		member = response->member_profiles[0]->member;
	}
	if (member == NULL || member->user == NULL) {
		g_free(who);
		return;
	}
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	if (member->profile_case != MEMBER__PROFILE_USER) {
		g_free(who);
		return;
	}
#endif
	User *user = member->user;
	//who = user->user_id ? user->user_id->id : NULL;
	
	user_info = purple_notify_user_info_new();

	if (user->name != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("Display Name"), user->name);
	if (user->first_name != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("First Name"), user->first_name);
	if (user->last_name != NULL)
		purple_notify_user_info_add_pair_html(user_info, _("Last Name"), user->last_name);

	if (user->avatar_url) {
		gchar *prefix = strncmp(user->avatar_url, "//", 2) ? "" : "https:";
		gchar *photo_tag = g_strdup_printf("<a href=\"%s%s\"><img width=\"128\" src=\"%s%s\"/></a>",
		                                   prefix, user->avatar_url, prefix, user->avatar_url);
		purple_notify_user_info_add_pair_html(user_info, _("Photo"), photo_tag);
		g_free(photo_tag);
	}

	if (user->email) {
		purple_notify_user_info_add_pair_html(user_info, _("Email"), user->email);
	}
	if (user->gender) {
		purple_notify_user_info_add_pair_html(user_info, _("Gender"), user->gender);
	}
	
	purple_notify_userinfo(ha->pc, who, user_info, NULL, NULL);
       
	g_free(who);
}


void
googlechat_get_info(PurpleConnection *pc, const gchar *who)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	GetMembersRequest request;
	MemberId member_id;
	MemberId *member_ids;
	UserId user_id;
	gchar *who_dup = g_strdup(who);
	
	get_members_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = who_dup;
	
	member_id__init(&member_id);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	member_id.id_case = MEMBER_ID__ID_USER_ID;
#endif
	member_id.user_id = &user_id;
	
	member_ids = &member_id;
	request.member_ids = &member_ids;
	request.n_member_ids = 1;
	
	googlechat_api_get_members(ha, &request, googlechat_got_user_info, who_dup);
	
	googlechat_request_header_free(request.request_header);
}

static void
googlechat_got_events(GoogleChatAccount *ha, CatchUpResponse *response, gpointer user_data)
{
	guint i;
	for (i = 0; i < response->n_events; i++) {
		Event *event = response->events[i];
		
		// TODO Ignore join/parts when loading history
		//Send event to the googlechat_events.c slaughterhouse
		googlechat_process_received_event(ha, event);
	}
}

void
googlechat_get_conversation_events(GoogleChatAccount *ha, const gchar *conv_id, gint64 since_timestamp)
{
	//since_timestamp is in microseconds
	CatchUpGroupRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	CatchUpRange range;
	
	g_return_if_fail(conv_id);
	
	catch_up_group_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	request.page_size = 500;
	request.cutoff_size = 500;
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	catch_up_range__init(&range);
	
	if (since_timestamp > 0) {
		range.has_from_revision_timestamp = TRUE;
		range.from_revision_timestamp = since_timestamp;
	}
	
	googlechat_api_catch_up_group(ha, &request, googlechat_got_events, NULL);
	
	googlechat_request_header_free(request.request_header);
	
}

void
googlechat_get_all_events(GoogleChatAccount *ha, guint64 since_timestamp)
{
	//since_timestamp is in microseconds
	CatchUpUserRequest request;
	CatchUpRange range;
	
	g_return_if_fail(since_timestamp > 0);
	
	catch_up_user_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	request.page_size = 500;
	request.cutoff_size = 500;
	
	catch_up_range__init(&range);
	range.has_from_revision_timestamp = TRUE;
	range.from_revision_timestamp = since_timestamp;
	
	googlechat_api_catch_up_user(ha, &request, googlechat_got_events, NULL);
	
	googlechat_request_header_free(request.request_header);
}

GList *
googlechat_chat_info(PurpleConnection *pc)
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
googlechat_chat_info_defaults(PurpleConnection *pc, const char *chatname)
{
	GHashTable *defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	
	if (chatname != NULL)
	{
		g_hash_table_insert(defaults, "conv_id", g_strdup(chatname));
	}
	
	return defaults;
}

void
googlechat_join_chat(PurpleConnection *pc, GHashTable *data)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
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
	googlechat_get_conversation_events(ha, conv_id, 0);
}

/*
static void
googlechat_got_join_chat_from_url(GoogleChatAccount *ha, OpenGroupConversationFromUrlResponse *response, gpointer user_data)
{
	if (!response || !response->conversation_id || !response->conversation_id->id) {
		purple_notify_error(ha->pc, _("Join from URL Error"), _("Could not join group from URL"), response && response->response_header ? response->response_header->error_description : _("Unknown Error"), purple_request_cpar_from_connection(ha->pc));
		return;
	}
	
	googlechat_get_conversation_events(ha, response->conversation_id->id, 0);
}

void
googlechat_join_chat_from_url(GoogleChatAccount *ha, const gchar *url)
{
	OpenGroupConversationFromUrlRequest request;
	
	g_return_if_fail(url != NULL);
	
	open_group_conversation_from_url_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	request.url = (gchar *) url;
	
	googlechat_pblite_open_group_conversation_from_url(ha, &request, googlechat_got_join_chat_from_url, NULL);
	
	googlechat_request_header_free(request.request_header);
}
*/


gchar *
googlechat_get_chat_name(GHashTable *data)
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
googlechat_add_person_to_blist(GoogleChatAccount *ha, gchar *gaia_id, gchar *alias)
{
	PurpleGroup *googlechat_group = purple_blist_find_group("Google Chat");
	
	if (purple_account_get_bool(ha->account, "hide_self", FALSE) && purple_strequal(gaia_id, ha->self_gaia_id)) {
		return;
	}
	
	if (!googlechat_group)
	{
		googlechat_group = purple_group_new("Google Chat");
		purple_blist_add_group(googlechat_group, NULL);
	}
	purple_blist_add_buddy(purple_buddy_new(ha->account, gaia_id, alias), NULL, googlechat_group, NULL);
}

void
googlechat_add_conversation_to_blist(GoogleChatAccount *ha, Group *group, GHashTable *unique_user_ids)
{
	PurpleGroup *googlechat_group = NULL;
	// guint i;
	gboolean is_dm = !!group->group_id->dm_id;
	gchar *conv_id = is_dm ? group->group_id->dm_id->dm_id : group->group_id->space_id->space_id;
	
	
	if (is_dm) {
		gchar *other_person = group->group_read_state->joined_users[0]->id;
		// guint participant_num = 0;
		gchar *other_person_alias = NULL;
		
		if (purple_strequal(other_person, ha->self_gaia_id)) {
			other_person = group->group_read_state->joined_users[1]->id;
			// participant_num = 1;
		}
		
		g_hash_table_replace(ha->one_to_ones, g_strdup(conv_id), g_strdup(other_person));
		g_hash_table_replace(ha->one_to_ones_rev, g_strdup(other_person), g_strdup(conv_id));
		
		if (!purple_blist_find_buddy(ha->account, other_person)) {
			googlechat_add_person_to_blist(ha, other_person, other_person_alias);
		} else {
			// purple_serv_got_alias(ha->pc, other_person, other_person_alias);
		}
		
		if (unique_user_ids == NULL) {
			GList *user_list = g_list_prepend(NULL, other_person);
			googlechat_get_users_presence(ha, user_list);
			g_list_free(user_list);
		}
		
	} else {
		PurpleChat *chat = purple_blist_find_chat(ha->account, conv_id);
		gchar *name = group->name;
		gboolean has_name = name ? TRUE : FALSE;
		
		g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
		
		if (chat == NULL) {
			googlechat_group = purple_blist_find_group("Google Chat");
			if (!googlechat_group)
			{
				googlechat_group = purple_group_new("Google Chat");
				purple_blist_add_group(googlechat_group, NULL);
			}
			
			//TODO 
			// if (!has_name) {
			// 	name = g_strdup("Unknown");
			// }
			purple_blist_add_chat(purple_chat_new(ha->account, name, googlechat_chat_info_defaults(ha->pc, conv_id)), googlechat_group, NULL);
			// if (!has_name)
				// g_free(name);
		} else {
			if(has_name && strstr(purple_chat_get_name(chat), _("Unknown")) != NULL) {
				purple_chat_set_alias(chat, name);
			}
		}
	}
	
	
	// for (i = 0; i < conversation->n_participant_data; i++) {
		// ConversationParticipantData *participant_data = conversation->participant_data[i];
		
		// if (participant_data->participant_type != PARTICIPANT_TYPE__PARTICIPANT_TYPE_UNKNOWN) {
			// if (!purple_blist_find_buddy(ha->account, participant_data->id->gaia_id)) {
				// googlechat_add_person_to_blist(ha, participant_data->id->gaia_id, participant_data->fallback_name);
			// }
			// if (participant_data->fallback_name != NULL) {
				// purple_serv_got_alias(ha->pc, participant_data->id->gaia_id, participant_data->fallback_name);
			// }
			// if (unique_user_ids != NULL) {
				// g_hash_table_replace(unique_user_ids, participant_data->id->gaia_id, participant_data->id);
			// }
		// }
	// }
}

static void
googlechat_got_conversation_list(GoogleChatAccount *ha, PaginatedWorldResponse *response, gpointer user_data)
{
	guint i;
	GHashTable *unique_user_ids = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	GList *unique_user_ids_list;
	PurpleBlistNode *node;
	PurpleGroup *googlechat_group = NULL;
	
	for (i = 0; i < response->n_world_items; i++) {
		WorldItemLite *world_item_lite = response->world_items[i];
		GroupId *group_id = world_item_lite->group_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		gboolean is_dm = (group_id->id_case == GROUP_ID__ID_DM_ID);
#else
		gboolean is_dm = !!group_id->dm_id;
#endif
		gchar *conv_id = is_dm ? group_id->dm_id->dm_id : group_id->space_id->space_id;
		
		//purple_debug_info("googlechat", "got worlditemlite %s\n", pblite_dump_json((ProtobufCMessage *)world_item_lite));
		//googlechat_add_conversation_to_blist(ha, group_id, NULL);
		
		if (is_dm) {
			gchar *other_person = world_item_lite->dm_members->members[0]->id;
			// guint participant_num = 0;
			gchar *other_person_alias = NULL;
			
			if (purple_strequal(other_person, ha->self_gaia_id)) {
				other_person = world_item_lite->dm_members->members[1]->id;
				// participant_num = 1;
			}
			
			g_hash_table_replace(ha->one_to_ones, g_strdup(conv_id), g_strdup(other_person));
			g_hash_table_replace(ha->one_to_ones_rev, g_strdup(other_person), g_strdup(conv_id));
			
			
			if (!purple_blist_find_buddy(ha->account, other_person)) {
				googlechat_add_person_to_blist(ha, other_person, other_person_alias);
			} else {
				// purple_serv_got_alias(ha->pc, other_person, other_person_alias);
			}
			
			g_hash_table_replace(unique_user_ids, other_person, NULL);
			
		} else {
			PurpleChat *chat = purple_blist_find_chat(ha->account, conv_id);
			gchar *name = world_item_lite->room_name;
			gboolean has_name = name ? TRUE : FALSE;
			
			g_hash_table_replace(ha->group_chats, g_strdup(conv_id), NULL);
			
			if (chat == NULL) {
				googlechat_group = purple_blist_find_group("Google Chat");
				if (!googlechat_group)
				{
					googlechat_group = purple_group_new("Google Chat");
					purple_blist_add_group(googlechat_group, NULL);
				}
				
				//TODO 
				// if (!has_name) {
				// loop over name_users->name_user_ids[]
				// 	name = g_strdup("Unknown");
				// }
				purple_blist_add_chat(purple_chat_new(ha->account, name, googlechat_chat_info_defaults(ha->pc, conv_id)), googlechat_group, NULL);
				// if (!has_name)
					// g_free(name);
			} else {
				if(has_name && strstr(purple_chat_get_name(chat), _("Unknown")) != NULL) {
					purple_chat_set_alias(chat, name);
				}
			}
		}
	}
	
	//Add missing people from the buddy list
	for (node = purple_blist_get_root();
	     node != NULL;
		 node = purple_blist_node_next(node, TRUE)) {
		if (PURPLE_IS_BUDDY(node)) {
			PurpleBuddy *buddy = PURPLE_BUDDY(node);
			const gchar *name;
			if (purple_buddy_get_account(buddy) != ha->account) {
				continue;
			}
			
			name = purple_buddy_get_name(buddy);
			g_hash_table_replace(unique_user_ids, (gchar *) name, NULL);
		}
	}
	
	unique_user_ids_list = g_hash_table_get_keys(unique_user_ids);
	googlechat_get_users_presence(ha, unique_user_ids_list);
	googlechat_get_users_information(ha, unique_user_ids_list);
	g_list_free(unique_user_ids_list);
	g_hash_table_unref(unique_user_ids);
}

void
googlechat_get_conversation_list(GoogleChatAccount *ha)
{
	PaginatedWorldRequest request;
	paginated_world_request__init(&request);
	
	request.request_header = googlechat_get_request_header(ha);
	request.has_fetch_from_user_spaces = TRUE;
	request.fetch_from_user_spaces = TRUE;
	request.has_fetch_snippets_for_unnamed_rooms = TRUE;
	request.fetch_snippets_for_unnamed_rooms = TRUE;
	
	googlechat_api_paginated_world(ha, &request, googlechat_got_conversation_list, NULL);
	
	googlechat_request_header_free(request.request_header);
}


static void
googlechat_got_buddy_photo(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
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
		purple_debug_error("googlechat", "Failed to get buddy photo for %s from %s\n", name, photo_url);
		return;
	}
	
	response_str = purple_http_response_get_data(response, &response_len);
	response_dup = g_memdup(response_str, response_len);
	purple_buddy_icons_set_for_user(account, name, response_dup, response_len, photo_url);
}

static void
googlechat_got_buddy_list(PurpleHttpConnection *http_conn, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
	PurpleGroup *googlechat_group = NULL;
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
		purple_debug_error("googlechat", "Failed to download buddy list: %s\n", purple_http_response_get_error(response));
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
		gchar *reachableAppType = googlechat_json_path_query_string(node, "$.inAppReachability[*].appType", NULL);
		
		if (!purple_strequal(reachableAppType, "BABEL")) {
			//Not a googlechat user
			g_free(reachableAppType);
			continue;
		}
		g_free(reachableAppType);
		
		name = json_object_get_string_member(person, "personId");
		alias = googlechat_json_path_query_string(node, "$.name[*].displayName", NULL);
		photo = googlechat_json_path_query_string(node, "$.photo[*].url", NULL);
		buddy = purple_blist_find_buddy(ha->account, name);
		
		if (purple_account_get_bool(ha->account, "hide_self", FALSE) && purple_strequal(ha->self_gaia_id, name)) {
			if (buddy != NULL) {
				purple_blist_remove_buddy(buddy);
			}
			
			g_free(alias);
			g_free(photo);
			continue;
		}
		
		if (buddy == NULL) {
			if (googlechat_group == NULL) {
				googlechat_group = purple_blist_find_group("Google Chat");
				if (!googlechat_group)
				{
					googlechat_group = purple_group_new("Google Chat");
					purple_blist_add_group(googlechat_group, NULL);
				}
			}
			buddy = purple_buddy_new(ha->account, name, alias);
			purple_blist_add_buddy(buddy, NULL, googlechat_group, NULL);
		} else {
			purple_serv_got_alias(ha->pc, name, alias);
		}
		
		if (!purple_strequal(purple_buddy_icons_get_checksum_for_user(buddy), photo)) {
			PurpleHttpRequest *photo_request = purple_http_request_new(photo);
			
			if (ha->icons_keepalive_pool == NULL) {
				ha->icons_keepalive_pool = purple_http_keepalive_pool_new();
				purple_http_keepalive_pool_set_limit_per_host(ha->icons_keepalive_pool, 4);
			}
			purple_http_request_set_keepalive_pool(photo_request, ha->icons_keepalive_pool);
			
			purple_http_request(ha->pc, photo_request, googlechat_got_buddy_photo, buddy);
			purple_http_request_unref(photo_request);
		}
		
		g_free(alias);
		g_free(photo);
	}

	json_object_unref(obj);
}

void
googlechat_get_buddy_list(GoogleChatAccount *ha)
{
	// gchar *request_data;
	
	//TODO POST https://peoplestack-pa.googleapis.com/$rpc/peoplestack.PeopleStackAutocompleteService/Autocomplete
	// [14, [], [4]]
	
	// or maybe
	//https://people-pa.googleapis.com/v2/people?person_id=me&request_mask.include_container=ACCOUNT&request_mask.include_container=PROFILE&request_mask.include_container=DOMAIN_PROFILE&request_mask.include_field.paths=person.cover_photo&request_mask.include_field.paths=person.email&request_mask.include_field.paths=person.photo&request_mask.include_field.paths=person.metadata&request_mask.include_field.paths=person.name&request_mask.include_field.paths=person.read_only_profile_info&request_mask.include_field.paths=person.read_only_profile_info.customer_info
	
	//https://peoplestack-pa.googleapis.com/v1/autocomplete/lookup?alt=json
	// [14, [4], ["test@gmail.com"]]
	
	(void) googlechat_got_buddy_list;
}

void
googlechat_block_user(PurpleConnection *pc, const char *who)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	
	BlockEntityRequest request;
	UserId user_id;
	
	block_entity_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = (gchar *) who;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	request.entity_case = BLOCK_ENTITY_REQUEST__ENTITY_USER_ID;
#endif
	request.user_id = &user_id;
	
	request.has_blocked = TRUE;
	request.blocked = TRUE;
	
	googlechat_api_block_entity(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
}

void
googlechat_unblock_user(PurpleConnection *pc, const char *who)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	
	BlockEntityRequest request;
	UserId user_id;
	
	block_entity_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = (gchar *) who;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	request.entity_case = BLOCK_ENTITY_REQUEST__ENTITY_USER_ID;
#endif
	request.user_id = &user_id;
	
	request.has_blocked = TRUE;
	request.blocked = FALSE;
	
	googlechat_api_block_entity(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
}

//Received the photoid of the sent image to be able to attach to an outgoing message
static void
googlechat_conversation_send_image_part2_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha;
	gchar *conv_id;
	gchar *photoid;
	const gchar *response_raw;
	size_t response_len;
	JsonNode *node;
	PurpleConnection *pc = purple_http_conn_get_purple_connection(connection);
	CreateTopicRequest request;
	Annotation photo_annotation;
	Annotation *annotations;
	DriveMetadata drive_metadata;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_notify_error(pc, _("Image Send Error"), _("There was an error sending the image"), purple_http_response_get_error(response), purple_request_cpar_from_connection(pc));
		g_dataset_destroy(connection);
		return;
	}
	
	ha = user_data;
	response_raw = purple_http_response_get_data(response, &response_len);
	purple_debug_info("googlechat", "image_part2_cb %s\n", response_raw);
	node = json_decode(response_raw, response_len);
	
	photoid = googlechat_json_path_query_string(node, "$..photoid", NULL);
	conv_id = g_dataset_get_data(connection, "conv_id");
	
	create_topic_request__init(&request);
	annotation__init(&photo_annotation);
	drive_metadata__init(&drive_metadata);
	group_id__init(&group_id);
	
	request.request_header = googlechat_get_request_header(ha);
	
	request.group_id = &group_id;
	if (g_hash_table_lookup(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	drive_metadata.id = photoid;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	photo_annotation.metadata_case = ANNOTATION__METADATA_DRIVE_METADATA;
#endif
	photo_annotation.drive_metadata = &drive_metadata;
	annotations = &photo_annotation;
	request.annotations = &annotations;
	request.n_annotations = 1;
	
	googlechat_api_create_topic(ha, &request, NULL, NULL);
	
	// g_hash_table_insert(ha->sent_message_ids, g_strdup_printf("%" G_GUINT64_FORMAT, request.event_request_header->client_generated_id), NULL);
	
	g_free(photoid);
	g_dataset_destroy(connection);
	googlechat_request_header_free(request.request_header);
	json_node_free(node);
}

//Received the url to upload the image data to
static void
googlechat_conversation_send_image_part1_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha;
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
	purple_debug_info("googlechat", "image_part1_cb %s\n", response_raw);
	node = json_decode(response_raw, response_len);
	
	upload_url = googlechat_json_path_query_string(node, "$..putInfo.url", NULL);
	
	request = purple_http_request_new(upload_url);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/octet-stream");
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_contents(request, purple_image_get_data(image), purple_image_get_data_size(image));
	
	new_connection = purple_http_request(ha->pc, request, googlechat_conversation_send_image_part2_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(new_connection, "conv_id", g_strdup(conv_id), g_free);
	
	g_free(upload_url);
	g_dataset_destroy(connection);
	json_node_free(node);
}

static void
googlechat_conversation_send_image(GoogleChatAccount *ha, const gchar *conv_id, PurpleImage *image)
{
	PurpleHttpRequest *request;
	PurpleHttpConnection *connection;
	gchar *postdata;
	gchar *filename;
	
	filename = (gchar *)purple_image_get_path(image);
	if (filename != NULL) {
		filename = g_path_get_basename(filename);
	} else {
		filename = g_strdup_printf("purple%u.%s", g_random_int(), purple_image_get_extension(image));
	}
	
	postdata = g_strdup_printf("{\"protocolVersion\":\"0.8\",\"createSessionRequest\":{\"fields\":[{\"external\":{\"name\":\"file\",\"filename\":\"%s\",\"put\":{},\"size\":%" G_GSIZE_FORMAT "}},{\"inlined\":{\"name\":\"client\",\"content\":\"googlechat\",\"contentType\":\"text/plain\"}}]}}", filename, (gsize) purple_image_get_data_size(image));
	
	request = purple_http_request_new(GOOGLECHAT_IMAGE_UPLOAD_URL);
	purple_http_request_set_cookie_jar(request, ha->cookie_jar);
	purple_http_request_header_set(request, "Content-Type", "application/x-www-form-urlencoded;charset=UTF-8");
	purple_http_request_set_method(request, "POST");
	purple_http_request_set_contents(request, postdata, -1);
	purple_http_request_set_max_redirects(request, 0);
	
	connection = purple_http_request(ha->pc, request, googlechat_conversation_send_image_part1_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
	g_dataset_set_data_full(connection, "image", image, NULL);
	
	g_free(filename);
	g_free(postdata);
}

static void
googlechat_conversation_check_message_for_images(GoogleChatAccount *ha, const gchar *conv_id, const gchar *message)
{
	const gchar *img;
	
	if ((img = strstr(message, "<img ")) || (img = strstr(message, "<IMG "))) {
		const gchar *id, *src;
		const gchar *close = strchr(img, '>');
		
		if (((id = strstr(img, "ID=\"")) || (id = strstr(img, "id=\""))) &&
				id < close) {
			int imgid = atoi(id + 4);
			PurpleImage *image = purple_image_store_get(imgid);
			
			if (image != NULL) {
				googlechat_conversation_send_image(ha, conv_id, image);
			}
		} else if (((src = strstr(img, "SRC=\"")) || (src = strstr(img, "src=\""))) &&
				src < close) {
			// purple3 embeds images using src="purple-image:1"
			if (strncmp(src + 5, "purple-image:", 13) == 0) {
				int imgid = atoi(src + 5 + 13);
				PurpleImage *image = purple_image_store_get(imgid);
				
				if (image != NULL) {
					googlechat_conversation_send_image(ha, conv_id, image);
				}
			}
		}
	}
}

static char * 
rand_str(size_t length) {
    static const char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *out = g_new0(char, length + 1);
	
	char *dest = out;
    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
	
	return out;
}

static gint
googlechat_conversation_send_message(GoogleChatAccount *ha, const gchar *conv_id, const gchar *message)
{
	CreateTopicRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	
	g_return_val_if_fail(conv_id, -1);
	
	gchar *message_dup = g_strdup(message);
	gchar *message_id = rand_str(11);
	
	//Check for any images to send first
	googlechat_conversation_check_message_for_images(ha, conv_id, message_dup);
	
	create_topic_request__init(&request);
	
	request.request_header = googlechat_get_request_header(ha);
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	request.text_body = message_dup;
	request.topic_and_message_id = message_id;
	request.has_history_v2 = TRUE;
	request.history_v2 = TRUE;
	
	//purple_debug_info("googlechat", "%s\n", pblite_dump_json((ProtobufCMessage *)&request)); //leaky
	
	//TODO listen to response
	googlechat_api_create_topic(ha, &request, NULL, NULL);
	
	g_hash_table_insert(ha->sent_message_ids, message_id, NULL);
	
	googlechat_request_header_free(request.request_header);

	g_free(message_dup);
	g_free(message_id);
	
	return 1;
}


gint
googlechat_send_im(PurpleConnection *pc, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *who = purple_message_get_recipient(msg);
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
#endif
	
	GoogleChatAccount *ha;
	const gchar *conv_id;
	
	ha = purple_connection_get_protocol_data(pc);
	conv_id = g_hash_table_lookup(ha->one_to_ones_rev, who);
	if (conv_id == NULL) {
		if (G_UNLIKELY(!googlechat_is_valid_id(who))) {
			googlechat_search_users_text(ha, who);
			return -1;
		}
		
		//We don't have any known conversations for this person
		googlechat_create_conversation(ha, TRUE, who, message);
		return -1;
		
		//TODO wait for the create dm response and use that
	}
	
	return googlechat_conversation_send_message(ha, conv_id, message);
}

gint
googlechat_chat_send(PurpleConnection *pc, gint id, 
#if PURPLE_VERSION_CHECK(3, 0, 0)
PurpleMessage *msg)
{
	const gchar *message = purple_message_get_contents(msg);
#else
const gchar *message, PurpleMessageFlags flags)
{
#endif
	
	GoogleChatAccount *ha;
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
	
	ret = googlechat_conversation_send_message(ha, conv_id, message);
	if (ret > 0) {
		purple_serv_got_chat_in(pc, g_str_hash(conv_id), ha->self_gaia_id, PURPLE_MESSAGE_SEND, message, time(NULL));
	}
	return ret;
}

guint
googlechat_send_typing(PurpleConnection *pc, const gchar *who, PurpleIMTypingState state)
{
	GoogleChatAccount *ha;
	PurpleConversation *conv;
	
	ha = purple_connection_get_protocol_data(pc);
	conv = PURPLE_CONVERSATION(purple_conversations_find_im_with_account(who, purple_connection_get_account(pc)));
	g_return_val_if_fail(conv, -1);
	
	return googlechat_conv_send_typing(conv, state, ha);
}

guint
googlechat_conv_send_typing(PurpleConversation *conv, PurpleIMTypingState state, GoogleChatAccount *ha)
{
	PurpleConnection *pc;
	const gchar *conv_id;
	SetTypingStateRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	TypingContext typing_context;
	
	pc = purple_conversation_get_connection(conv);
	
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return 0;
	
	if (!purple_strequal(purple_protocol_get_id(purple_connection_get_protocol(pc)), GOOGLECHAT_PLUGIN_ID))
		return 0;
	
	if (ha == NULL) {
		ha = purple_connection_get_protocol_data(pc);
	}
	
	conv_id = purple_conversation_get_data(conv, "conv_id");
	if (conv_id == NULL) {
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			conv_id = g_hash_table_lookup(ha->one_to_ones_rev, purple_conversation_get_name(conv));
		} else {
			conv_id = purple_conversation_get_name(conv);
		}
	}
	g_return_val_if_fail(conv_id, -1); //TODO create new conversation for this new person
	
	set_typing_state_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	typing_context__init(&typing_context);
	request.context = &typing_context;
	
	group_id__init(&group_id);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	typing_context.context_case = TYPING_CONTEXT__CONTEXT_GROUP_ID;
#endif
	typing_context.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	request.has_state = TRUE;
	switch(state) {
		case PURPLE_IM_TYPING:
			request.state = TYPING_STATE__TYPING;
			break;
		
		//case PURPLE_IM_TYPED:
		case PURPLE_IM_NOT_TYPING:
		default:
			request.state = TYPING_STATE__STOPPED;
			break;
	}
	
	//TODO listen to response
	//TODO dont send STOPPED if we just sent a message
	googlechat_api_set_typing_state(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
	
	return 20;
}

void
googlechat_chat_leave_by_conv_id(PurpleConnection *pc, const gchar *conv_id, const gchar *who)
{
	GoogleChatAccount *ha;
	RemoveMembershipsRequest request;
	MemberId member_id;
	MemberId *member_ids;
	UserId user_id;
	GroupId group_id;
	SpaceId space_id;
	// DmId dm_id;
	
	g_return_if_fail(conv_id);
	ha = purple_connection_get_protocol_data(pc);
	g_return_if_fail(g_hash_table_contains(ha->group_chats, conv_id));
	
	remove_memberships_request__init(&request);
	
	if (who != NULL) {
		member_id__init(&member_id);
		user_id__init(&user_id);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		member_id.id_case = MEMBER_ID__ID_USER_ID;
#endif
		member_id.user_id = &user_id;
		
		user_id.id = (gchar *) who;
		member_ids = &member_id;
		request.member_ids = &member_ids;
		request.n_member_ids = 1;
	}
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	// if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		// dm_id__init(&dm_id);
		// dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		// group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		// group_id.dm_id = &dm_id;
	// } else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	// }
	
	request.request_header = googlechat_get_request_header(ha);
	request.has_membership_state = TRUE;
	request.membership_state = MEMBERSHIP_STATE__MEMBER_INVITED;
	
	//XX do we need to see if this was successful, or does it just come through as a new event?
	googlechat_api_remove_memberships(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
	
	if (who == NULL) {
		g_hash_table_remove(ha->group_chats, conv_id);
	}
}

void 
googlechat_chat_leave(PurpleConnection *pc, int id)
{
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	return googlechat_chat_leave_by_conv_id(pc, conv_id, NULL);
}

void
googlechat_chat_kick(PurpleConnection *pc, int id, const gchar *who)
{
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	return googlechat_chat_leave_by_conv_id(pc, conv_id, who);
}

static void 
googlechat_created_dm(GoogleChatAccount *ha, CreateDmResponse *response, gpointer user_data)
{
	Group *dm = response->dm;
	gchar *message = user_data;
	const gchar *conv_id;
	
	gchar *dump = pblite_dump_json((ProtobufCMessage *) response);
	purple_debug_info("googlechat", "%s\n", dump);
	g_free(dump);
	
	if (dm == NULL) {
		purple_debug_error("googlechat", "Could not create DM\n");
		g_free(message);
		return;
	}
	
	googlechat_add_conversation_to_blist(ha, dm, NULL);
	conv_id = dm->group_id->dm_id->dm_id;
	googlechat_get_conversation_events(ha, conv_id, 0);
	
	if (message != NULL) {
		googlechat_conversation_send_message(ha, conv_id, message);
		g_free(message);
	}
}

static void 
googlechat_created_group(GoogleChatAccount *ha, CreateGroupResponse *response, gpointer user_data)
{
	Group *group = response->group;
	gchar *message = user_data;
	const gchar *conv_id;
	
	gchar *dump = pblite_dump_json((ProtobufCMessage *) response);
	purple_debug_info("googlechat", "%s\n", dump);
	g_free(dump);
	
	if (group == NULL) {
		purple_debug_error("googlechat", "Could not create Group\n");
		g_free(message);
		return;
	}
	
	googlechat_add_conversation_to_blist(ha, group, NULL);
	conv_id = group->group_id->space_id->space_id;
	googlechat_get_conversation_events(ha, conv_id, 0);
	
	if (message != NULL) {
		googlechat_conversation_send_message(ha, conv_id, message);
		g_free(message);
	}
}

void
googlechat_create_conversation(GoogleChatAccount *ha, gboolean is_one_to_one, const char *who, const gchar *optional_message)
{
	UserId user_id;
	InviteeInfo invitee_info;
	gchar *message_dup = NULL;
	
	user_id__init(&user_id);
	user_id.id = (gchar *) who;
		
	invitee_info__init(&invitee_info);
	invitee_info.user_id = &user_id;
	//TODO
	//invitee_info.email = (gchar *) "foo@bar.com";
	
	if (optional_message != NULL) {
		message_dup = g_strdup(optional_message);
	}
	
	if (is_one_to_one) {
		CreateDmRequest request;
		UserId *members;
		InviteeInfo *invitees;
		
		create_dm_request__init(&request);
		request.request_header = googlechat_get_request_header(ha);
		
		members = &user_id;
		request.members = &members;
		request.n_members = 1;
		
		invitees = &invitee_info;
		request.invitees = &invitees;
		request.n_invitees = 1;
		
		googlechat_api_create_dm(ha, &request, googlechat_created_dm, message_dup);
		
		googlechat_request_header_free(request.request_header);
		
		
	} else {
		//group chat
		CreateGroupRequest request;
		SpaceCreationInfo space;
		InviteeMemberInfo imi;
		InviteeMemberInfo *invitee_member_infos;
		
		create_group_request__init(&request);
		request.has_should_find_existing_space = TRUE;
		request.should_find_existing_space = FALSE;
		
		space_creation_info__init(&space);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		request.creation_info_case = CREATE_GROUP_REQUEST__CREATION_INFO_SPACE;
#endif
		request.space = &space;
		//TODO
		//space.name = (gchar *) "space name";
		
		invitee_member_info__init(&imi);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		imi.id_case = INVITEE_MEMBER_INFO__ID_INVITEE_INFO;
#endif
		imi.invitee_info = &invitee_info;
		invitee_member_infos = &imi;
		space.invitee_member_infos = &invitee_member_infos;
		space.n_invitee_member_infos = 1;
		
		googlechat_api_create_group(ha, &request, googlechat_created_group, message_dup);
		
		googlechat_request_header_free(request.request_header);
		
	}
}

void
googlechat_archive_conversation(GoogleChatAccount *ha, const gchar *conv_id)
{
	HideGroupRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	
	if (conv_id == NULL) {
		return;
	}
	
	hide_group_request__init(&request);
	
	group_id__init(&group_id);
	request.id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	request.request_header = googlechat_get_request_header(ha);
	request.has_hide = TRUE;
	request.hide = TRUE;
	
	googlechat_api_hide_group(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		gchar *buddy_id = g_hash_table_lookup(ha->one_to_ones, conv_id);
		
		if (buddy_id != NULL) {
			g_hash_table_remove(ha->one_to_ones_rev, buddy_id);
		}
		g_hash_table_remove(ha->one_to_ones, conv_id);
	} else {
		g_hash_table_remove(ha->group_chats, conv_id);
	}
}

void
googlechat_initiate_chat_from_node(PurpleBlistNode *node, gpointer userdata)
{
	if(PURPLE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *) node;
		GoogleChatAccount *ha;
		
		if (userdata) {
			ha = userdata;
		} else {
			PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
			ha = purple_connection_get_protocol_data(pc);
		}
		
		googlechat_create_conversation(ha, FALSE, purple_buddy_get_name(buddy), NULL);
	}
}

void
googlechat_chat_invite(PurpleConnection *pc, int id, const char *message, const char *who)
{
	GoogleChatAccount *ha;
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	CreateMembershipRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	InviteeMemberInfo imi;
	InviteeMemberInfo *invitee_member_infos;
	InviteeInfo invitee_info;
	UserId user_id;
	
	ha = purple_connection_get_protocol_data(pc);
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	create_membership_request__init(&request);
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = (gchar *) who;
	
	invitee_info__init(&invitee_info);
	invitee_info.user_id = &user_id;
	
	invitee_member_info__init(&imi);
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
	imi.id_case = INVITEE_MEMBER_INFO__ID_INVITEE_INFO;
#endif
	imi.invitee_info = &invitee_info;
	invitee_member_infos = &imi;
	
	request.invitee_member_infos = &invitee_member_infos;
	request.n_invitee_member_infos = 1;
	
	googlechat_api_create_membership(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
}

#define PURPLE_CONVERSATION_IS_VALID(conv) (g_list_find(purple_conversations_get_all(), conv) != NULL)

void
googlechat_mark_conversation_seen(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	PurpleConnection *pc;
	GoogleChatAccount *ha;
	const gchar *conv_id;
	
	if (type != PURPLE_CONVERSATION_UPDATE_UNSEEN)
		return;
	
	pc = purple_conversation_get_connection(conv);
	if (!PURPLE_CONNECTION_IS_CONNECTED(pc))
		return;
	
	if (!purple_strequal(purple_protocol_get_id(purple_connection_get_protocol(pc)), GOOGLECHAT_PLUGIN_ID))
		return;
	
	ha = purple_connection_get_protocol_data(pc);
	
	if (!purple_presence_is_status_primitive_active(purple_account_get_presence(ha->account), PURPLE_STATUS_AVAILABLE)) {
		// We're not here
		return;
	}
	
	conv_id = purple_conversation_get_data(conv, "conv_id");
	if (conv_id == NULL) {
		if (PURPLE_IS_IM_CONVERSATION(conv)) {
			conv_id = g_hash_table_lookup(ha->one_to_ones_rev, purple_conversation_get_name(conv));
		} else {
			conv_id = purple_conversation_get_name(conv);
		}
	}
	
	if (conv_id == NULL)
		return;
	
	MarkGroupReadstateRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	
	mark_group_readstate_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	group_id__init(&group_id);
	request.id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_DM_ID;
#endif
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		group_id.id_case = GROUP_ID__ID_SPACE_ID;
#endif
		group_id.space_id = &space_id;
	}
	
	request.has_last_read_time = TRUE;
	request.last_read_time = g_get_real_time();
	
	googlechat_api_mark_group_readstate(ha, &request, NULL, NULL);
}

void
googlechat_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	
	SetPresenceSharedRequest presence_request;
	SetDndDurationRequest dnd_request;

	set_presence_shared_request__init(&presence_request);
	presence_request.request_header = googlechat_get_request_header(ha);
	set_dnd_duration_request__init(&dnd_request);
	dnd_request.request_header = googlechat_get_request_header(ha);
	
	//available:
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_AVAILABLE) {
		presence_request.has_presence_shared = TRUE;
		presence_request.presence_shared = TRUE;
	}
	
	//away
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_AWAY) {
		presence_request.has_presence_shared = TRUE;
		presence_request.presence_shared = FALSE;
	}
	
	//do-not-disturb
	if (purple_status_type_get_primitive(purple_status_get_status_type(status)) == PURPLE_STATUS_UNAVAILABLE) {
		dnd_request.has_current_dnd_state = TRUE;
		dnd_request.current_dnd_state = SET_DND_DURATION_REQUEST__STATE__DND;
#if PROTOBUF_C_VERSION_NUMBER < 1001000
		dnd_request.has_dnd_expiry_timestamp_usec = TRUE;
#else
		dnd_request.dnd_expiry_case = SET_DND_DURATION_REQUEST__DND_EXPIRY_DND_EXPIRY_TIMESTAMP_USEC;
#endif
		dnd_request.dnd_expiry_timestamp_usec = 172800000000;
	} else {
		dnd_request.has_current_dnd_state = TRUE;
		dnd_request.current_dnd_state = SET_DND_DURATION_REQUEST__STATE__AVAILABLE;
#if PROTOBUF_C_VERSION_NUMBER < 1001000
		dnd_request.has_new_dnd_duration_usec = TRUE;
#else
		dnd_request.dnd_expiry_case = SET_DND_DURATION_REQUEST__DND_EXPIRY_NEW_DND_DURATION_USEC;
#endif
		dnd_request.new_dnd_duration_usec = 0;
	}

	googlechat_api_set_presence_shared(ha, &presence_request, NULL, NULL);
	googlechat_api_set_dnd_duration(ha, &dnd_request, NULL, NULL);
	
	googlechat_request_header_free(presence_request.request_header);
	googlechat_request_header_free(dnd_request.request_header);
	
	const gchar *message = purple_status_get_attr_string(status, "message");
	if (message && *message) {
		SetCustomStatusRequest custom_status_request;
		CustomStatus custom_status;
		
		set_custom_status_request__init(&custom_status_request);
#if PROTOBUF_C_VERSION_NUMBER < 1001000
		custom_status_request.has_custom_status_remaining_duration_usec = TRUE;
#else
		custom_status_request.custom_status_timing_case = SET_CUSTOM_STATUS_REQUEST__CUSTOM_STATUS_TIMING_CUSTOM_STATUS_REMAINING_DURATION_USEC;
#endif
		custom_status_request.custom_status_remaining_duration_usec = 172800000000;
		
		custom_status__init(&custom_status);
		custom_status_request.custom_status = &custom_status;
		custom_status.status_text = (gchar *) message;
		
		googlechat_api_set_custom_status(ha, &custom_status_request, NULL, NULL);
		
		googlechat_request_header_free(custom_status_request.request_header);
	}
}


static void
googlechat_roomlist_got_list(GoogleChatAccount *ha, PaginatedWorldResponse *response, gpointer user_data)
{
	PurpleRoomlist *roomlist = user_data;
	guint i;
	
	for (i = 0; i < response->n_world_items; i++) {
		WorldItemLite *world_item_lite = response->world_items[i];
		GroupId *group_id = world_item_lite->group_id;
#if PROTOBUF_C_VERSION_NUMBER >= 1001000
		gboolean is_dm = (group_id->id_case == GROUP_ID__ID_DM_ID);
#else
		gboolean is_dm = !!group_id->dm_id;
#endif
		gchar *conv_id = is_dm ? group_id->dm_id->dm_id : group_id->space_id->space_id;
		
		if (!is_dm) {
			gchar *users = NULL;
			//gchar **users_set = g_new0(gchar *, conversation->n_participant_data + 1);
			gchar *name = world_item_lite->room_name;
			PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, conv_id, NULL);
			
			purple_roomlist_room_add_field(roomlist, room, conv_id);
			
			//TODO 
			// for (j = 0; j < conversation->n_participant_data; j++) {
				// gchar *p_name = conversation->participant_data[j]->fallback_name;
				// if (p_name != NULL) {
					// users_set[j] = p_name;
				// } else {
					// users_set[j] = _("Unknown");
				// }
			// }
			// users = g_strjoinv(", ", users_set);
			// g_free(users_set);
			purple_roomlist_room_add_field(roomlist, room, users);
			//g_free(users);
			
			purple_roomlist_room_add_field(roomlist, room, name);
			
			purple_roomlist_room_add(roomlist, room);
		}
	}
	
	purple_roomlist_set_in_progress(roomlist, FALSE);
}

PurpleRoomlist *
googlechat_roomlist_get_list(PurpleConnection *pc)
{
	GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
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
		//Stolen from googlechat_get_conversation_list()
		PaginatedWorldRequest request;
		paginated_world_request__init(&request);
		
		request.request_header = googlechat_get_request_header(ha);
		request.has_fetch_from_user_spaces = TRUE;
		request.fetch_from_user_spaces = TRUE;
		request.has_fetch_snippets_for_unnamed_rooms = TRUE;
		request.fetch_snippets_for_unnamed_rooms = TRUE;
		
		googlechat_api_paginated_world(ha, &request, googlechat_roomlist_got_list, roomlist);
		
		googlechat_request_header_free(request.request_header);
	}
	
	
	return roomlist;
}

void
googlechat_rename_conversation(GoogleChatAccount *ha, const gchar *conv_id, const gchar *alias)
{
	UpdateGroupRequest request;
	SpaceId space_id;
	UpdateGroupRequest__UpdateMask update_mask;
	
	update_group_request__init(&request);
	request.request_header = googlechat_get_request_header(ha);
	
	space_id__init(&space_id);
	space_id.space_id = (gchar *) conv_id;
	
	request.name = (gchar *) alias;
	
	update_mask = UPDATE_GROUP_REQUEST__UPDATE_MASK__NAME;
	request.n_update_masks = 1;
	request.update_masks = &update_mask;
	
	googlechat_api_update_group(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
}

