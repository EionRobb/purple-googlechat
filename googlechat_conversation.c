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
	ClientFeatureCapabilities *cfc = g_new0(ClientFeatureCapabilities, 1);
	
	request_header__init(header);
	
	header->has_client_type = TRUE;
	header->client_type = REQUEST_HEADER__CLIENT_TYPE__IOS;
	
	header->has_client_version = TRUE;
	header->client_version = 2440378181258;
	
	header->has_trace_id = TRUE;
	header->trace_id = g_random_int();
	
	client_feature_capabilities__init(cfc);
	header->client_feature_capabilities = cfc;
	
	cfc->has_spam_room_invites_level = TRUE;
	cfc->spam_room_invites_level = CLIENT_FEATURE_CAPABILITIES__CAPABILITY_LEVEL__FULLY_SUPPORTED;
	
	return header;
}

void
googlechat_request_header_free(RequestHeader *header)
{
	g_free(header->client_feature_capabilities);
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
	User *user = member->user;
	const gchar *gaia_id = user->user_id ? user->user_id->id : NULL;
	
	if (gaia_id != NULL) {
		PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, gaia_id);
		
		if (buddy == NULL) {
			//TODO add to buddy list?
			return;
		}
		
		// Give a best-guess for the buddy's alias
		if (user->name)
			purple_blist_server_alias_buddy(buddy, user->name);
		else if (user->email)
			purple_blist_server_alias_buddy(buddy, user->email);
		//TODO first+last name
		
		const gchar *balias = purple_buddy_get_local_buddy_alias(buddy);
		const gchar *salias = purple_buddy_get_server_alias(buddy);
		if ((!balias || !*balias) && !purple_strequal(balias, salias)) {
			purple_blist_alias_buddy(buddy, salias);
		}
		
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

static void
googlechat_get_users_information_internal(GoogleChatAccount *ha, GList *user_ids, GoogleChatApiGetMembersResponseFunc callback, gpointer userdata)
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
			continue;
		}
		
		member_ids[i] = g_new0(MemberId, 1);
		member_id__init(member_ids[i]);
		
		member_ids[i]->user_id = g_new0(UserId, 1);
		user_id__init(member_ids[i]->user_id);
		member_ids[i]->user_id->id = (gchar *) cur->data;
		
	}
	
	request.member_ids = member_ids;
	request.n_member_ids = n_member_ids;
	
	googlechat_api_get_members(ha, &request, callback, userdata);
	
	googlechat_request_header_free(request.request_header);
	for (i = 0; i < n_member_ids; i++) {
		g_free(member_ids[i]->user_id);
		g_free(member_ids[i]);
	}
	g_free(member_ids);
}

void
googlechat_get_users_information(GoogleChatAccount *ha, GList *user_ids)
{
	googlechat_get_users_information_internal(ha, user_ids, googlechat_got_users_information, NULL);
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
	
	request.has_page_size = TRUE;
	request.page_size = 500;
	request.has_cutoff_size = TRUE;
	request.cutoff_size = 500;
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	catch_up_range__init(&range);
	request.range = &range;
	
	range.has_from_revision_timestamp = TRUE;
	range.from_revision_timestamp = since_timestamp;
	
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
	
	request.has_page_size = TRUE;
	request.page_size = 500;
	request.has_cutoff_size = TRUE;
	request.cutoff_size = 500;
	
	catch_up_range__init(&range);
	range.has_from_revision_timestamp = TRUE;
	range.from_revision_timestamp = since_timestamp;
	request.range = &range;
	
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

static void
googlechat_alias_group_user_hack(PurpleChatConversation *chat, const char *who, const char *alias)
{
	PurpleChatUser *cb;
	gboolean ui_update_sent = FALSE;
	PurpleConversation *conv = PURPLE_CONVERSATION(chat);
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleConversationUiOps *convuiops = purple_conversation_get_ui_ops(conv);
	
	cb = purple_chat_conversation_find_user(chat, who);
	if (cb == NULL) {
		return;
	}
	purple_chat_user_set_alias(cb, g_strdup(alias));
	
	if (convuiops != NULL) {
		// Horrible hack.  Don't try this at home!
		if (convuiops->chat_rename_user != NULL) {
#if PURPLE_VERSION_CHECK(3, 0, 0)
			convuiops->chat_rename_user(chat, who, who, alias);
#else
			convuiops->chat_rename_user(conv, who, who, alias);
#endif
			ui_update_sent = TRUE;
		} else if (convuiops->chat_update_user != NULL) {
#if PURPLE_VERSION_CHECK(3, 0, 0)
			convuiops->chat_update_user(cb);
#else
			convuiops->chat_update_user(conv, who);
#endif
			ui_update_sent = TRUE;
		}
	}
	
	if (ui_update_sent == FALSE) {
		// Bitlbee doesn't have the above two functions, lets do an even worse hack
		PurpleBuddy *fakebuddy;
		PurpleGroup *temp_group = purple_blist_find_group("Google Chat Temporary Chat Buddies");
		if (!temp_group)
		{
			temp_group = purple_group_new("Google Chat Temporary Chat Buddies");
			purple_blist_add_group(temp_group, NULL);
		}
		
		fakebuddy = purple_buddy_new(account, who, alias);
		purple_blist_node_set_transient(PURPLE_BLIST_NODE(fakebuddy), TRUE);
		purple_blist_add_buddy(fakebuddy, NULL, temp_group, NULL);
	}
}

static void
googlechat_got_group_users(GoogleChatAccount *ha, GetMembersResponse *response, gpointer user_data)
{
	gchar *conv_id = user_data;
	
	if (response != NULL) {
		guint i;
		PurpleChatConversation *chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
		
		for (i = 0; i < response->n_members; i++) {
			Member *member = response->members[i];
			User *user = member ? member->user : NULL;
			const gchar *user_id = user && user->user_id ? user->user_id->id : NULL;
			
			if (user_id && user && user->name && !purple_strequal(ha->self_gaia_id, user_id)) {
				googlechat_alias_group_user_hack(chatconv, user_id, user->name);
			}
		}
		
	}
	
	g_free(conv_id);
}

static void
googlechat_got_group_info(GoogleChatAccount *ha, GetGroupResponse *response, gpointer user_data)
{
	Group *group = response->group;
	Membership **memberships = response->memberships;
	guint i;
	PurpleChatConversation *chatconv;
	gchar *conv_id;
	GList *unknown_user_ids = NULL;
	
	g_return_if_fail(group != NULL);
	
	GroupId *group_id = group->group_id;
	gboolean is_dm = !!group_id->dm_id;
	if (is_dm) {
		conv_id = group_id->dm_id->dm_id;
	} else {
		conv_id = group_id->space_id->space_id;
	}
	
	chatconv = purple_conversations_find_chat_with_account(conv_id, ha->account);
	
	for (i = 0; i < response->n_memberships; i++) {
		Membership *membership = memberships[i];
		const gchar *user_id = membership->id->member_id->user_id->id;
		PurpleChatUserFlags cbflags = PURPLE_CHAT_USER_NONE;
		
		if (membership->membership_role == MEMBERSHIP_ROLE__ROLE_OWNER) {
			cbflags = PURPLE_CHAT_USER_OP;
		}
		
		PurpleChatUser *chat_user = purple_chat_conversation_find_user(chatconv, user_id);
		if (chat_user) {
			purple_chat_user_set_flags(chat_user, cbflags);
		} else {
			purple_chat_conversation_add_user(chatconv, user_id, NULL, cbflags, FALSE);
		}
		
		if (!purple_blist_find_buddy(ha->account, user_id)) {
			unknown_user_ids = g_list_append(unknown_user_ids, (gchar *) user_id);
		}
	}
	
	if (group->name) {
		(void) group->name; //TODO - rename buddy list
	}
	
	if (unknown_user_ids != NULL) {
		googlechat_get_users_information_internal(ha, unknown_user_ids, googlechat_got_group_users, g_strdup(conv_id));
	}
}

void
googlechat_lookup_group_info(GoogleChatAccount *ha, const gchar *conv_id)
{
	GetGroupRequest request;
	GroupId group_id;
	DmId dm_id;
	SpaceId space_id;
	
	get_group_request__init(&request);
	group_id__init(&group_id);
	
	request.request_header = googlechat_get_request_header(ha);
	
	request.group_id = &group_id;
	if (g_hash_table_lookup(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	request.has_include_invite_dms = TRUE;
	request.include_invite_dms = TRUE;
	
	GetGroupRequest__FetchOptions fetch_options = GET_GROUP_REQUEST__FETCH_OPTIONS__MEMBERS;
	request.n_fetch_options = 1;
	request.fetch_options = &fetch_options;
	
	googlechat_api_get_group(ha, &request, googlechat_got_group_info, NULL);
	
	googlechat_request_header_free(request.request_header);
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
	//googlechat_get_conversation_events(ha, conv_id, ha->last_event_timestamp);
	googlechat_lookup_group_info(ha, conv_id);
	
	// Forcibly join the chat, even if we're already in it
	CreateMembershipRequest request;
	MemberId member_id, *member_ids;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	UserId user_id;
	
	create_membership_request__init(&request);
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = (gchar *) ha->self_gaia_id;
	
	member_id__init(&member_id);
	member_id.user_id = &user_id;
	
	member_ids = &member_id;
	
	request.member_ids = &member_ids;
	request.n_member_ids = 1;
	
	googlechat_api_create_membership(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
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
googlechat_add_person_to_blist(GoogleChatAccount *ha, const gchar *gaia_id, const gchar *alias)
{
	PurpleGroup *googlechat_group = purple_blist_find_group("Google Chat");
	PurpleContact *old_contact = NULL;
	PurpleBuddy *old_buddy = NULL;
	PurpleAccount *old_account = NULL;
	
	if (purple_account_get_bool(ha->account, "hide_self", FALSE) && purple_strequal(gaia_id, ha->self_gaia_id)) {
		return;
	}
	
	if (!googlechat_group)
	{
		googlechat_group = purple_group_new("Google Chat");
		purple_blist_add_group(googlechat_group, NULL);
	}
	
	old_account = purple_accounts_find(purple_account_get_username(ha->account), "prpl-hangouts");
	if (old_account) {
		old_buddy = purple_blist_find_buddy(old_account, gaia_id);
	}
	if (old_buddy) {
		old_contact = purple_buddy_get_contact(old_buddy);
		if (!alias || !*alias) {
			alias = purple_buddy_get_alias(old_buddy);
		}
	}
	
	purple_blist_add_buddy(purple_buddy_new(ha->account, gaia_id, alias), old_contact, googlechat_group, NULL);
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
		
		PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, other_person);
		if (!buddy) {
			googlechat_add_person_to_blist(ha, other_person, other_person_alias);
		} else {
			if (other_person_alias && *other_person_alias) {
				purple_blist_server_alias_buddy(buddy, other_person_alias);
		
				const gchar *balias = purple_buddy_get_local_buddy_alias(buddy);
				if ((!balias || !*balias) && !purple_strequal(balias, other_person_alias)) {
					purple_blist_alias_buddy(buddy, other_person_alias);
				}
			}
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
		gboolean is_dm = !!group_id->dm_id;
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

			PurpleBuddy *buddy = purple_blist_find_buddy(ha->account, other_person);
			
			if (!world_item_lite->read_state->hide_timestamp) {
				g_hash_table_replace(ha->one_to_ones, g_strdup(conv_id), g_strdup(other_person));
				g_hash_table_replace(ha->one_to_ones_rev, g_strdup(other_person), g_strdup(conv_id));
				if (!buddy) {
					googlechat_add_person_to_blist(ha, other_person, other_person_alias);
				} else {
					if (other_person_alias && *other_person_alias) {
						purple_blist_server_alias_buddy(buddy, other_person_alias);
			
						const gchar *balias = purple_buddy_get_local_buddy_alias(buddy);
						if ((!balias || !*balias) && !purple_strequal(balias, other_person_alias)) {
							purple_blist_alias_buddy(buddy, other_person_alias);
						}
					}
				}

				g_hash_table_replace(unique_user_ids, other_person, NULL);
			} else if (buddy) {
				purple_blist_remove_buddy(buddy);
			}
			
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
		
		if (world_item_lite->read_state->last_read_time > ha->last_event_timestamp) {
			googlechat_get_conversation_events(ha, conv_id, ha->last_event_timestamp);
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
			googlechat_add_person_to_blist(ha, name, alias);
		} else {
			if (alias && *alias) {
				purple_blist_server_alias_buddy(buddy, alias);
			}
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
	request.user_id = &user_id;
	
	request.has_blocked = TRUE;
	request.blocked = FALSE;
	
	googlechat_api_block_entity(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
}

// Annotation ***annotations  - a pointer to an array of annotation objects
static gchar *
googlechat_html_to_text(const gchar *html_message, Annotation ***annotations, guint *annotations_count)
{
	GString *text_content;
	const gchar *c = html_message;
	GList *annotation_list = NULL;
	guint n_annotations = 0;
	guint i;
	gint bold_pos_start, italic_pos_start, strikethrough_pos_start, underline_pos_start, link_pos_start;
	Annotation *ann;
	FormatMetadata *format;
	gchar *last_link = NULL;
	
	if (c == NULL || *c == '\0') {
		g_warn_if_reached();
		
		if (annotations_count != NULL) {
			*annotations_count = 0;
		}
		if (annotations != NULL) {
			*annotations = NULL;
		}
		return NULL;
	}
	
	bold_pos_start = italic_pos_start = strikethrough_pos_start = underline_pos_start = link_pos_start = -1;
	text_content = g_string_new("");
	
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

#define GOOGLECHAT_CREATE_ANNOTATION(formatType, formatTypeUPPER) \
	ann = g_new0(Annotation, 1); \
	annotation__init(ann); \
	format = g_new0(FormatMetadata, 1); \
	format_metadata__init(format); \
	 \
	ann->has_type = TRUE; \
	ann->type = ANNOTATION_TYPE__FORMAT_DATA; \
	ann->has_chip_render_type = TRUE; \
	ann->chip_render_type = ANNOTATION__CHIP_RENDER_TYPE__DO_NOT_RENDER; \
	ann->has_start_index = TRUE; \
	ann->start_index = formatType##_pos_start; \
	ann->has_length = TRUE; \
	ann->length = text_content->len - formatType##_pos_start; \
	 \
	format->has_format_type = TRUE; \
	format->format_type = FORMAT_METADATA__FORMAT_TYPE__##formatTypeUPPER; \
	ann->format_metadata = format; \
	 \
	annotation_list = g_list_append(annotation_list, ann); \
	formatType##_pos_start = -1;
			
			if (!g_ascii_strcasecmp(tag->str, "BR") ||
					!g_ascii_strcasecmp(tag->str, "BR/")) {
				g_string_append_c(text_content, '\n');
				
			} else if (!g_ascii_strcasecmp(tag->str, "B") ||
					!g_ascii_strcasecmp(tag->str, "BOLD") ||
					!g_ascii_strcasecmp(tag->str, "STRONG")) {
				if (opening) {
					bold_pos_start = text_content->len;
				} else if (bold_pos_start > -1) {
					GOOGLECHAT_CREATE_ANNOTATION(bold, BOLD);
				}
				
			} else if (!g_ascii_strcasecmp(tag->str, "I") ||
					!g_ascii_strcasecmp(tag->str, "ITALIC") ||
					!g_ascii_strcasecmp(tag->str, "EM")) {
				if (opening) {
					italic_pos_start = text_content->len;
				} else if (italic_pos_start > -1) {
					GOOGLECHAT_CREATE_ANNOTATION(italic, ITALIC);
				}
				
			} else if (!g_ascii_strcasecmp(tag->str, "S") ||
					!g_ascii_strcasecmp(tag->str, "STRIKE")) {
				if (opening) {
					strikethrough_pos_start = text_content->len;
				} else if (strikethrough_pos_start > -1) {
					GOOGLECHAT_CREATE_ANNOTATION(strikethrough, STRIKE);
				}
				
			} else if (!g_ascii_strcasecmp(tag->str, "U") ||
					!g_ascii_strcasecmp(tag->str, "UNDERLINE")) {
				if (opening) {
					underline_pos_start = text_content->len;
				} else if (underline_pos_start > -1) {
					GOOGLECHAT_CREATE_ANNOTATION(underline, UNDERLINE);
				}
				
			} else if (!g_ascii_strcasecmp(tag->str, "A")) {
				if (opening) {
					link_pos_start = text_content->len;
					
					while (*c != '>') {
						//Grab HREF for the A
						if (g_ascii_strncasecmp(c, " HREF=", 6) == 0) {
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
					UrlMetadata *url = g_new0(UrlMetadata, 1);
					url_metadata__init(url);
					
					ann = g_new0(Annotation, 1);
					annotation__init(ann);
					
					ann->has_type = TRUE;
					ann->type = ANNOTATION_TYPE__URL;
					ann->has_chip_render_type = TRUE;
					ann->chip_render_type = ANNOTATION__CHIP_RENDER_TYPE__RENDER_IF_POSSIBLE;
					ann->has_start_index = TRUE;
					ann->start_index = link_pos_start;
					ann->has_length = TRUE;
					ann->length = text_content->len - link_pos_start;
					
					url->url = url->gws_url = url->redirect_url = g_new0(Url, 1);
					url__init(url->url);
					url->url->url = last_link;
					ann->url_metadata = url;
					
					annotation_list = g_list_append(annotation_list, ann);
					link_pos_start = -1;
					
					last_link = NULL;
				}
#undef GOOGLECHAT_CREATE_ANNOTATION
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
	
	if (annotations != NULL) {
		n_annotations = g_list_length(annotation_list);
		Annotation **annotations_out = g_new0(Annotation *, n_annotations + 1);
		
		for (i = 0; annotation_list && annotation_list->data; i++) {
			annotations_out[i] = (Annotation *) annotation_list->data;
			
			annotation_list = g_list_delete_link(annotation_list, annotation_list);
		}
		
		*annotations = annotations_out;
	}
	
	if (annotations_count != NULL) {
		*annotations_count = n_annotations;
	}
	
	return g_string_free(text_content, FALSE);
}

static void
googlechat_free_annotations(Annotation **annotations)
{
	guint i;
	
	if (annotations == NULL) {
		// Our work here is done
		return;
	}
	
	for (i = 0; annotations[i]; i++) {
		if (annotations[i]->format_metadata) {
			g_free(annotations[i]->format_metadata);
			
		} else if (annotations[i]->url_metadata) {
			g_free(annotations[i]->url_metadata->url->url);
			g_free(annotations[i]->url_metadata->url);
			g_free(annotations[i]->url_metadata);
		}
		
		g_free(annotations[i]);
	}
	
	g_free(annotations);
}

//Received the upload metadata of the sent image to be able to attach to an outgoing message
static void
googlechat_conversation_send_image_part2_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha;
	gchar *conv_id;
	const gchar *response_raw;
	guchar *decoded_response;
	size_t response_len;
	PurpleConnection *pc = purple_http_conn_get_purple_connection(connection);
	CreateTopicRequest request;
	Annotation photo_annotation;
	Annotation *annotations;
	UploadMetadata *upload_metadata;
	ProtobufCMessage *unpacked_message;
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
	
	decoded_response = g_base64_decode(response_raw, &response_len);
	unpacked_message = protobuf_c_message_unpack(&upload_metadata__descriptor, NULL, response_len, decoded_response);
			
	if (unpacked_message != NULL) {
		upload_metadata = (UploadMetadata *) unpacked_message;
		
		conv_id = g_dataset_get_data(connection, "conv_id");
		
		create_topic_request__init(&request);
		annotation__init(&photo_annotation);
		group_id__init(&group_id);
		
		request.request_header = googlechat_get_request_header(ha);
		
		gchar *message_id = g_strdup_printf("purple%" G_GUINT32_FORMAT, (guint32) g_random_int());
		request.local_id = message_id;
		request.has_history_v2 = TRUE;
		request.history_v2 = TRUE;
		request.text_body = (gchar *) "";
		
		request.group_id = &group_id;
		if (g_hash_table_lookup(ha->one_to_ones, conv_id)) {
			dm_id__init(&dm_id);
			dm_id.dm_id = conv_id;
			group_id.dm_id = &dm_id;
		} else {
			space_id__init(&space_id);
			space_id.space_id = conv_id;
			group_id.space_id = &space_id;
		}
		
		photo_annotation.has_type = TRUE;
		photo_annotation.type = ANNOTATION_TYPE__UPLOAD_METADATA;
		photo_annotation.upload_metadata = upload_metadata;
		photo_annotation.has_chip_render_type = TRUE;
		photo_annotation.chip_render_type = ANNOTATION__CHIP_RENDER_TYPE__RENDER;
		
		annotations = &photo_annotation;
		request.annotations = &annotations;
		request.n_annotations = 1;
		
		googlechat_api_create_topic(ha, &request, NULL, NULL);
		
		g_hash_table_insert(ha->sent_message_ids, message_id, NULL);
		
		g_dataset_destroy(connection);
		googlechat_request_header_free(request.request_header);
		protobuf_c_message_free_unpacked(unpacked_message, NULL);
	
	}
}

//Received the url to upload the image data to
static void
googlechat_conversation_send_image_part1_cb(PurpleHttpConnection *connection, PurpleHttpResponse *response, gpointer user_data)
{
	GoogleChatAccount *ha;
	gchar *conv_id;
	PurpleImage *image;
	const gchar *upload_url;
	PurpleHttpRequest *request;
	PurpleHttpConnection *new_connection;
	PurpleConnection *pc = purple_http_conn_get_purple_connection(connection);
	
	if (purple_http_response_get_error(response) != NULL) {
		purple_notify_error(pc, _("Image Send Error"), _("There was an error sending the image"), purple_http_response_get_error(response), purple_request_cpar_from_connection(pc));
		g_dataset_destroy(connection);
		return;
	}
	
	ha = user_data;
	conv_id = g_dataset_get_data(connection, "conv_id");
	image = g_dataset_get_data(connection, "image");
	
	//x-guploader-uploadid: ADP...
	//x-goog-upload-status: active
	//x-goog-upload-control-url: same as upload-url ?
	//x-goog-upload-chunk-granularity: 1048576 //TODO
	upload_url = purple_http_response_get_header(response, "x-goog-upload-url");
	
	request = purple_http_request_new(upload_url);
	purple_http_request_set_keepalive_pool(request, ha->api_keepalive_pool);
	purple_http_request_header_set(request, "x-goog-upload-command", "upload, finalize");
	purple_http_request_header_set(request, "x-goog-upload-protocol", "resumable");
	purple_http_request_header_set(request, "x-goog-upload-offset", "0"); //TODO
	purple_http_request_set_method(request, "PUT");
	purple_http_request_set_contents(request, purple_image_get_data(image), purple_image_get_data_size(image));

	purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->access_token);
	new_connection = purple_http_request(ha->pc, request, googlechat_conversation_send_image_part2_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(new_connection, "conv_id", g_strdup(conv_id), g_free);
	
	g_dataset_destroy(connection);
}

static void
googlechat_conversation_send_image(GoogleChatAccount *ha, const gchar *conv_id, PurpleImage *image)
{
	PurpleHttpRequest *request;
	PurpleHttpConnection *connection;
	gchar *filename;
	gchar *url;
	
	filename = (gchar *)purple_image_get_path(image);
	if (filename != NULL) {
		filename = g_path_get_basename(filename);
	} else {
		filename = g_strdup_printf("purple%u.%s", g_random_int(), purple_image_get_extension(image));
	}
	
	url = g_strdup_printf("https://chat.google.com/uploads?group_id=%s", purple_url_encode(conv_id));
	request = purple_http_request_new(url);
	purple_http_request_set_method(request, "POST");
	purple_http_request_header_set(request, "x-goog-upload-protocol", "resumable");
	purple_http_request_header_set(request, "x-goog-upload-command", "start");
	purple_http_request_header_set_printf(request, "x-goog-upload-content-length", "%" G_GSIZE_FORMAT, (gsize) purple_image_get_data_size(image));
	purple_http_request_header_set_printf(request, "x-goog-upload-content-type", "image/%s", purple_image_get_extension(image));
	purple_http_request_header_set(request, "x-goog-upload-file-name", filename);
	purple_http_request_set_keepalive_pool(request, ha->api_keepalive_pool);
	
	purple_http_request_header_set_printf(request, "Authorization", "Bearer %s", ha->access_token);
	connection = purple_http_request(ha->pc, request, googlechat_conversation_send_image_part1_cb, ha);
	purple_http_request_unref(request);
	
	g_dataset_set_data_full(connection, "conv_id", g_strdup(conv_id), g_free);
	g_dataset_set_data_full(connection, "image", image, NULL);
	
	g_free(filename);
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

static gint
googlechat_conversation_send_message(GoogleChatAccount *ha, const gchar *conv_id, const gchar *message)
{
	CreateTopicRequest request;
	GroupId group_id;
	SpaceId space_id;
	DmId dm_id;
	RetentionSettings retention_settings;
	MessageInfo message_info;
	Annotation **annotations = NULL;
	guint n_annotations = 0;
	
	g_return_val_if_fail(conv_id, -1);
	
	gchar *message_id = g_strdup_printf("purple%" G_GUINT32_FORMAT, (guint32) g_random_int());
	
	//Check for any images to send first
	googlechat_conversation_check_message_for_images(ha, conv_id, message);
	
	gchar *message_dup = googlechat_html_to_text(message, &annotations, &n_annotations);
	
	create_topic_request__init(&request);
	
	request.request_header = googlechat_get_request_header(ha);
	
	group_id__init(&group_id);
	request.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	request.text_body = message_dup;
	request.local_id = message_id;
	request.has_history_v2 = TRUE;
	request.history_v2 = TRUE;
	
	request.annotations = annotations;
	request.n_annotations = n_annotations;
	
	retention_settings__init(&retention_settings);
	request.retention_settings = &retention_settings;
	// TODO: set retention state based on conversation's history setting?
	retention_settings.has_state = FALSE;
	
	message_info__init(&message_info);
	request.message_info = &message_info;
	message_info.has_accept_format_annotations = TRUE;
	message_info.accept_format_annotations = TRUE; //false = treat message as markdown
	
	purple_debug_info("googlechat", "%s\n", pblite_dump_json((ProtobufCMessage *)&request)); //leaky
	
	//TODO listen to response
	googlechat_api_create_topic(ha, &request, NULL, NULL);
	
	g_hash_table_insert(ha->sent_message_ids, message_id, NULL);
	
	googlechat_request_header_free(request.request_header);
	googlechat_free_annotations(annotations);

	g_free(message_dup);
	
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
		return 0;
		
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
	typing_context.group_id = &group_id;
	
	if (g_hash_table_contains(ha->one_to_ones, conv_id)) {
		dm_id__init(&dm_id);
		dm_id.dm_id = (gchar *) conv_id;
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
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
		// group_id.dm_id = &dm_id;
	// } else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
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
		if (googlechat_conversation_send_message(ha, conv_id, message) > 0) {
			//TODO write into chat
			// if (sender_id) {
				// imconv = purple_conversations_find_im_with_account(sender_id, ha->account);
				// PurpleMessage *pmessage = purple_message_new_outgoing(sender_id, msg, msg_flags);
				// if (imconv == NULL)
				// {
					// imconv = purple_im_conversation_new(ha->account, sender_id);
				// }
				// purple_message_set_time(pmessage, message_timestamp);
				// purple_conversation_write_message(PURPLE_CONVERSATION(imconv), pmessage);
			// }
		}
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
		RetentionSettings retention_settings;
		
		create_dm_request__init(&request);
		request.request_header = googlechat_get_request_header(ha);
		
		members = &user_id;
		request.members = &members;
		request.n_members = 1;
		
		invitees = &invitee_info;
		request.invitees = &invitees;
		request.n_invitees = 1;
		
		retention_settings__init(&retention_settings);
		request.retention_settings = &retention_settings;
		retention_settings.has_state = TRUE;
		retention_settings.state = RETENTION_SETTINGS__RETENTION_STATE__PERMANENT;
		
		googlechat_api_create_dm(ha, &request, googlechat_created_dm, message_dup);
		
		googlechat_request_header_free(request.request_header);
		
		
		GList tmp_usr_list;
		tmp_usr_list.data = (gpointer) who;
		googlechat_get_users_information(ha, &tmp_usr_list);
		
		
	} else {
		//group chat
		CreateGroupRequest request;
		SpaceCreationInfo space;
		InviteeMemberInfo imi;
		InviteeMemberInfo *invitee_member_infos;
		
		create_group_request__init(&request);
		request.request_header = googlechat_get_request_header(ha);
		request.has_should_find_existing_space = TRUE;
		request.should_find_existing_space = FALSE;
		
		space_creation_info__init(&space);
		request.space = &space;
		//TODO
		//space.name = (gchar *) "space name";
		
		invitee_member_info__init(&imi);
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
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
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
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	request.request_header = googlechat_get_request_header(ha);
	
	user_id__init(&user_id);
	user_id.id = (gchar *) who;
	
	invitee_info__init(&invitee_info);
	invitee_info.user_id = &user_id;
	
	invitee_member_info__init(&imi);
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
	
	if (!purple_conversation_has_focus(conv))
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
		group_id.dm_id = &dm_id;
	} else {
		space_id__init(&space_id);
		space_id.space_id = (gchar *) conv_id;
		group_id.space_id = &space_id;
	}
	
	request.has_last_read_time = TRUE;
	request.last_read_time = g_get_real_time();
	
	googlechat_api_mark_group_readstate(ha, &request, NULL, NULL);
	
	googlechat_request_header_free(request.request_header);
	
	googlechat_subscribe_to_group(ha, &group_id);
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
		dnd_request.has_dnd_expiry_timestamp_usec = TRUE;
		dnd_request.dnd_expiry_timestamp_usec = 172800000000;
	} else {
		dnd_request.has_current_dnd_state = TRUE;
		dnd_request.current_dnd_state = SET_DND_DURATION_REQUEST__STATE__AVAILABLE;
		dnd_request.has_new_dnd_duration_usec = TRUE;
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
		Emoji emoji;
		
		set_custom_status_request__init(&custom_status_request);
		custom_status_request.request_header = googlechat_get_request_header(ha);
		
		custom_status_request.has_custom_status_remaining_duration_usec = TRUE;
		custom_status_request.custom_status_remaining_duration_usec = 172800000000;
		
		custom_status__init(&custom_status);
		custom_status_request.custom_status = &custom_status;
		custom_status.status_text = (gchar *) message;
		
		emoji__init(&emoji);
		emoji.unicode = "";
		custom_status.emoji = &emoji;
		
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
		gboolean is_dm = !!group_id->dm_id;
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

