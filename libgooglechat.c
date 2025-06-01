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

#include "libgooglechat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

#include <purple.h>

#include "googlechat_auth.h"
#include "googlechat_pblite.h"
#include "googlechat_json.h"
#include "googlechat_events.h"
#include "googlechat_connection.h"
#include "googlechat_conversation.h"
#include "http.h"


/*****************************************************************************/
//TODO move to nicer place

gboolean
googlechat_is_valid_id(const gchar *id)
{
	gint i;
	
	for (i = strlen(id) - 1; i >= 0; i--) {
		if (!g_ascii_isdigit(id[i])) {
			return FALSE;
		}
	}
	
	return TRUE;
}

PurpleMediaCaps
googlechat_get_media_caps(PurpleAccount *account, const char *who)
{
	return PURPLE_MEDIA_CAPS_AUDIO | PURPLE_MEDIA_CAPS_VIDEO | PURPLE_MEDIA_CAPS_AUDIO_VIDEO | PURPLE_MEDIA_CAPS_MODIFY_SESSION;
}

void
googlechat_set_idle(PurpleConnection *pc, int time)
{
	GoogleChatAccount *ha;
	
	ha = purple_connection_get_protocol_data(pc);
	
	ha->idle_time = time;
	googlechat_set_active_client(pc);
}

static GList *
googlechat_add_account_options(GList *account_options)
{
	PurpleAccountOption *option;
	
	option = purple_account_option_bool_new(N_("Show call links in chat"), "show-call-links", !purple_media_manager_get());
	account_options = g_list_append(account_options, option);
	
	option = purple_account_option_bool_new(N_("Un-Googlify URLs"), "unravel_google_url", FALSE);
	account_options = g_list_append(account_options, option);
	
	option = purple_account_option_bool_new(N_("Treat invisible users as offline"), "treat_invisible_as_offline", FALSE);
	account_options = g_list_append(account_options, option);
	
	option = purple_account_option_bool_new(N_("Hide self from buddy list (requires reconnect)"), "hide_self", FALSE);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_bool_new(N_("Fetch image history when opening group chats"), "fetch_image_history", TRUE);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_string_new(N_("COMPASS Cookie"), "COMPASS_token", NULL);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_string_new(N_("SSID Cookie"), "SSID_token", NULL);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_string_new(N_("SID Cookie"), "SID_token", NULL);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_string_new(N_("OSID Cookie"), "OSID_token", NULL);
	account_options = g_list_append(account_options, option);

	option = purple_account_option_string_new(N_("HSID Cookie"), "HSID_token", NULL);
	account_options = g_list_append(account_options, option);
	
	return account_options;
}

static void
googlechat_blist_node_removed(PurpleBlistNode *node)
{
	PurpleChat *chat = NULL;
	PurpleBuddy *buddy = NULL;
	PurpleAccount *account = NULL;
	PurpleConnection *pc;
	const gchar *conv_id;
	GHashTable *components;
	
	if (PURPLE_IS_CHAT(node)) {
		chat = PURPLE_CHAT(node);
		account = purple_chat_get_account(chat);
	} else if (PURPLE_IS_BUDDY(node)) {
		buddy = PURPLE_BUDDY(node);
		account = purple_buddy_get_account(buddy);
	}
	
	if (account == NULL) {
		return;
	}
	
	if (!purple_strequal(purple_account_get_protocol_id(account), GOOGLECHAT_PLUGIN_ID)) {
		return;
	}
	
	pc = purple_account_get_connection(account);
	if (pc == NULL) {
		return;
	}
	
	if (chat != NULL) {
		components = purple_chat_get_components(chat);
		conv_id = g_hash_table_lookup(components, "conv_id");
		if (conv_id == NULL) {
			conv_id = purple_chat_get_name_only(chat);
		}
		
		googlechat_chat_leave_by_conv_id(pc, conv_id, NULL);
	} else {
		GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
		const gchar *gaia_id = purple_buddy_get_name(buddy);
		conv_id = g_hash_table_lookup(ha->one_to_ones_rev, gaia_id);
		
		googlechat_archive_conversation(ha, conv_id);
		
		if (purple_strequal(gaia_id, ha->self_gaia_id)) {
			purple_account_set_bool(account, "hide_self", TRUE);
		}
	}
}

static void
googlechat_blist_node_aliased(PurpleBlistNode *node, const char *old_alias)
{
	PurpleChat *chat = NULL;
	PurpleAccount *account = NULL;
	PurpleConnection *pc;
	const gchar *conv_id;
	GHashTable *components;
	GoogleChatAccount *ha;
	const gchar *new_alias;
	
	if (PURPLE_IS_CHAT(node)) {
		chat = PURPLE_CHAT(node);
		account = purple_chat_get_account(chat);
	}
	
	if (account == NULL) {
		return;
	}
	
	if (!purple_strequal(purple_account_get_protocol_id(account), GOOGLECHAT_PLUGIN_ID)) {
		return;
	}
	
	pc = purple_account_get_connection(account);
	if (pc == NULL) {
		return;
	}
	ha = purple_connection_get_protocol_data(pc);
	
	if (g_dataset_get_data(ha, "ignore_set_alias")) {
		return;
	}
	
	if (chat != NULL) {
		new_alias = purple_chat_get_alias(chat);
		
		// Don't send update to existing update
		if (!purple_strequal(old_alias, new_alias)) {
			components = purple_chat_get_components(chat);
			conv_id = g_hash_table_lookup(components, "conv_id");
			if (conv_id == NULL) {
				conv_id = purple_chat_get_name_only(chat);
			}
			
			googlechat_rename_conversation(ha, conv_id, new_alias);
		}
	}
}

static void
googlechat_chat_set_topic(PurpleConnection *pc, int id, const char *topic)
{
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	GoogleChatAccount *ha;
	
	ha = purple_connection_get_protocol_data(pc);
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	return googlechat_rename_conversation(ha, conv_id, topic);
}

static PurpleCmdRet
googlechat_cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	googlechat_chat_leave(pc, id);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
googlechat_cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	googlechat_chat_kick(pc, id, args[0]);
	
	return PURPLE_CMD_RET_OK;
}

static GList *
googlechat_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	
	if(PURPLE_IS_BUDDY(node))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
					PURPLE_CALLBACK(googlechat_initiate_chat_from_node),
					NULL, NULL);
		m = g_list_append(m, act);
	} else if (PURPLE_IS_CHAT(node)) {
		act = purple_menu_action_new(_("_Leave Chat"),
					PURPLE_CALLBACK(googlechat_blist_node_removed), // A strange coinkidink
					NULL, NULL);
		m = g_list_append(m, act);
	}
	
	return m;
}

// static void
// googlechat_join_chat_by_url_action(PurpleProtocolAction *action)
// {
	// PurpleConnection *pc = purple_protocol_action_get_connection(action);
	// GoogleChatAccount *ha = purple_connection_get_protocol_data(pc);
	
	// purple_request_input(pc, _("Join chat..."),
					   // _("Join a GoogleChat group chat from the invite URL..."),
					   // NULL,
					   // NULL, FALSE, FALSE, "https://googlechat.google.com/group/...",
					   // _("_Join"), G_CALLBACK(googlechat_join_chat_from_url),
					   // _("_Cancel"), NULL,
					   // purple_request_cpar_from_connection(pc),
					   // ha);

// }

static GList *
googlechat_actions(
#if !PURPLE_VERSION_CHECK(3, 0, 0)
PurplePlugin *plugin, gpointer context
#else
PurpleConnection *pc
#endif
)
{
	GList *m = NULL;
	PurpleProtocolAction *act;

	act = purple_protocol_action_new(_("Search for friends..."), googlechat_search_users);
	m = g_list_append(m, act);

	// act = purple_protocol_action_new(_("Join a group chat by URL..."), googlechat_join_chat_by_url_action);
	// m = g_list_append(m, act);

	return m;
}

static void
googlechat_authcode_input_cb(gpointer user_data, const gchar *auth_code)
{
	GoogleChatAccount *ha = user_data;
	PurpleConnection *pc = ha->pc;

	purple_connection_update_progress(pc, _("Authenticating"), 1, 3);
	googlechat_oauth_with_code(ha, auth_code);
}

static void
googlechat_authcode_input_cancel_cb(gpointer user_data)
{
	GoogleChatAccount *ha = user_data;
	purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE, 
		_("User cancelled authorization"));
}

static gulong chat_conversation_typing_signal = 0;

#if !PURPLE_VERSION_CHECK(3, 0, 0)
static gulong deleting_chat_buddy_signal = 0;

// See workaround for purple_chat_conversation_find_user() in purplecompat.h
static void
googlechat_deleting_chat_buddy(PurpleConvChatBuddy *cb)
{
	if (g_dataset_get_data(cb, "chat") != NULL) {
		g_dataset_destroy(cb);
	}
}
#endif

static void
googlechat_login(PurpleAccount *account)
{
	PurpleConnection *pc;
	GoogleChatAccount *ha; //hahaha
	const gchar *password;
	const gchar *self_gaia_id;
	PurpleConnectionFlags pc_flags;

	pc = purple_account_get_connection(account);
	password = purple_connection_get_password(pc);
	
	// TODO 
	pc_flags = purple_connection_get_flags(pc);
	pc_flags |= PURPLE_CONNECTION_FLAG_HTML;
	pc_flags |= PURPLE_CONNECTION_FLAG_NO_FONTSIZE;
	pc_flags |= PURPLE_CONNECTION_FLAG_NO_BGCOLOR;
	// pc_flags &= ~PURPLE_CONNECTION_FLAG_NO_IMAGES;
	purple_connection_set_flags(pc, pc_flags);
	
	ha = g_new0(GoogleChatAccount, 1);
	ha->account = account;
	ha->pc = pc;
	ha->cookie_jar = purple_http_cookie_jar_new();
	ha->channel_buffer = g_byte_array_sized_new(GOOGLECHAT_BUFFER_DEFAULT_SIZE);
	ha->channel_keepalive_pool = purple_http_keepalive_pool_new();
	ha->api_keepalive_pool = purple_http_keepalive_pool_new();
	ha->sent_message_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	ha->one_to_ones = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	ha->one_to_ones_rev = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	ha->group_chats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	self_gaia_id = purple_account_get_string(account, "self_gaia_id", NULL);
	if (self_gaia_id != NULL) {
		ha->self_gaia_id = g_strdup(self_gaia_id);
		purple_connection_set_display_name(pc, ha->self_gaia_id);
	}
	
	purple_connection_set_protocol_data(pc, ha);
	
	// Cache intermediate certificates
	googlechat_cache_ssl_certs(ha);
	
	if (password && *password) {
		ha->refresh_token = g_strdup(password);
		purple_connection_update_progress(pc, _("Authenticating"), 1, 3);
		googlechat_oauth_refresh_token(ha);
	} else {
		//TODO remove this as it doesn't work
		(void) googlechat_authcode_input_cb;
		(void) googlechat_authcode_input_cancel_cb;
		/*
		purple_notify_uri(pc, "https://www.youtube.com/watch?v=hlDhp-eNLMU");
		purple_request_input(pc, _("Authorization Code"), "https://www.youtube.com/watch?v=hlDhp-eNLMU",
			_ ("Please follow the YouTube video to get the OAuth code"),
			_ ("and then paste the Google OAuth code here"), FALSE, FALSE, NULL, 
			_("OK"), G_CALLBACK(googlechat_authcode_input_cb), 
			_("Cancel"), G_CALLBACK(googlechat_authcode_input_cancel_cb), 
			purple_request_cpar_from_connection(pc), ha);
		*/
#define googlechat_try_get_cookie_value(cookie_name) \
		if (purple_account_get_string(account, cookie_name "_token", NULL) != NULL) { \
			purple_http_cookie_jar_set(ha->cookie_jar, cookie_name, purple_account_get_string(account, cookie_name "_token", NULL)); \
		} else { \
			purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE, N_("Cookie " cookie_name " is required")); \
			return; \
		}

		googlechat_try_get_cookie_value("COMPASS");
		googlechat_try_get_cookie_value("SSID");
		googlechat_try_get_cookie_value("SID");
		googlechat_try_get_cookie_value("OSID");
		googlechat_try_get_cookie_value("HSID");

		googlechat_auth_refresh_xsrf_token(ha);
	}
	
	purple_signal_connect(purple_blist_get_handle(), "blist-node-removed", account, PURPLE_CALLBACK(googlechat_blist_node_removed), NULL);
	purple_signal_connect(purple_blist_get_handle(), "blist-node-aliased", account, PURPLE_CALLBACK(googlechat_blist_node_aliased), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", account, PURPLE_CALLBACK(googlechat_mark_conversation_seen), NULL);
	if (!chat_conversation_typing_signal) {
		chat_conversation_typing_signal = purple_signal_connect(purple_conversations_get_handle(), "chat-conversation-typing", purple_connection_get_protocol(pc), PURPLE_CALLBACK(googlechat_conv_send_typing), NULL);
	}

#if !PURPLE_VERSION_CHECK(3, 0, 0)
	if (!deleting_chat_buddy_signal) {
		deleting_chat_buddy_signal = purple_signal_connect(purple_conversations_get_handle(), "deleting-chat-buddy", purple_connection_get_protocol(pc), PURPLE_CALLBACK(googlechat_deleting_chat_buddy), NULL);
	}
#endif
	
	ha->active_client_timeout = g_timeout_add_seconds(GOOGLECHAT_ACTIVE_CLIENT_TIMEOUT, ((GSourceFunc) googlechat_set_active_client), pc);
}

static void
googlechat_close(PurpleConnection *pc)
{
	GoogleChatAccount *ha; //not so funny anymore
	
	ha = purple_connection_get_protocol_data(pc);
	purple_signals_disconnect_by_handle(ha->account);
	
	g_source_remove(ha->active_client_timeout);
	g_source_remove(ha->channel_watchdog);
	g_source_remove(ha->poll_buddy_status_timeout);
	g_source_remove(ha->refresh_token_timeout);
	g_source_remove(ha->dynamite_token_timeout);
	
	purple_http_conn_cancel_all(pc);
	
	purple_http_keepalive_pool_unref(ha->channel_keepalive_pool);
	purple_http_keepalive_pool_unref(ha->api_keepalive_pool);
	g_free(ha->self_gaia_id);
	g_free(ha->self_phone);
	g_free(ha->id_token);
	g_free(ha->refresh_token);
	g_free(ha->access_token);
	g_free(ha->csessionid_param);
	g_free(ha->sid_param);
	g_free(ha->client_id);
	g_free(ha->xsrf_token);
	purple_http_cookie_jar_unref(ha->cookie_jar);
	g_byte_array_free(ha->channel_buffer, TRUE);
	
	g_hash_table_remove_all(ha->sent_message_ids);
	g_hash_table_unref(ha->sent_message_ids);
	g_hash_table_remove_all(ha->one_to_ones);
	g_hash_table_unref(ha->one_to_ones);
	g_hash_table_remove_all(ha->one_to_ones_rev);
	g_hash_table_unref(ha->one_to_ones_rev);
	g_hash_table_remove_all(ha->group_chats);
	g_hash_table_unref(ha->group_chats);
	
	g_free(ha);
}


static const char *
googlechat_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "googlechat";
}

static void
googlechat_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const gchar *message;
	GoogleChatBuddy *hbuddy;
	
	g_return_if_fail(buddy != NULL);
	
	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);
	purple_notify_user_info_add_pair_html(user_info, _("Status"), purple_status_get_name(status));
	
	message = purple_status_get_attr_string(status, "message");
	if (message != NULL) {
		purple_notify_user_info_add_pair_html(user_info, _("Message"), message);
	}
	
	hbuddy = purple_buddy_get_protocol_data(buddy);
	if (hbuddy != NULL) {
		if (hbuddy->last_seen != 0) {
			gchar *seen = purple_str_seconds_to_string(time(NULL) - hbuddy->last_seen);
			purple_notify_user_info_add_pair_html(user_info, _("Last seen"), seen);
			g_free(seen);
		}
		
		if (hbuddy->in_call) {
			purple_notify_user_info_add_pair_html(user_info, _("In call"), NULL);
		}
		
		if (hbuddy->device_type) {
			purple_notify_user_info_add_pair_html(user_info, _("Device Type"), 
				hbuddy->device_type & GOOGLECHAT_DEVICE_TYPE_DESKTOP ? _("Desktop") :
				hbuddy->device_type & GOOGLECHAT_DEVICE_TYPE_TABLET ? _("Tablet") :
				hbuddy->device_type & GOOGLECHAT_DEVICE_TYPE_MOBILE ? _("Mobile") :
				_("Unknown"));
		}
	}
}

GList *
googlechat_status_types(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, NULL, NULL, FALSE, TRUE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_EXTENDED_AWAY, NULL, NULL, FALSE, FALSE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_UNAVAILABLE, NULL, _("Do Not Disturb"), FALSE, TRUE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_full(PURPLE_STATUS_MOBILE, "mobile", _("Phone"), FALSE, FALSE, FALSE);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE, "gone", NULL, FALSE, FALSE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_INVISIBLE, NULL, NULL, FALSE, FALSE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);

	return types;
}

static gchar *
googlechat_status_text(PurpleBuddy *buddy)
{
	const gchar *message = purple_status_get_attr_string(purple_presence_get_active_status(purple_buddy_get_presence(buddy)), "message");
	
	if (message == NULL) {
		return NULL;
	}
	
	return g_markup_printf_escaped("%s", message);
}

static void
googlechat_buddy_free(PurpleBuddy *buddy)
{
	GoogleChatBuddy *hbuddy = purple_buddy_get_protocol_data(buddy);
	
	if (hbuddy == NULL) {
		return;
	}
	
	g_free(hbuddy);
}

static gboolean
googlechat_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}


/*****************************************************************************/

static gboolean
googlechat_create_account_from_hangouts_account(PurpleAccount *hangouts_account)
{
	PurpleAccount *new_account;
	const gchar *username;
	const gchar *password;
	
	g_return_val_if_fail(g_strcmp0(purple_account_get_protocol_id(hangouts_account), "prpl-hangouts") == 0, FALSE);
	
	username = purple_account_get_username(hangouts_account);
	password = purple_account_get_password(hangouts_account);
	
	g_return_val_if_fail(username && *username, FALSE);
	g_return_val_if_fail(password && *password, FALSE);
	
	new_account = purple_account_new(username, GOOGLECHAT_PLUGIN_ID);
	purple_account_set_remember_password(new_account, TRUE);
	purple_account_set_password(new_account, password, NULL, NULL);
	
	purple_account_set_alias(new_account, purple_account_get_alias(hangouts_account));
	
	//TODO enable the new, disable the old
	purple_account_set_enabled(new_account, purple_core_get_ui(), purple_account_get_enabled(hangouts_account, purple_core_get_ui()));
	purple_account_set_enabled(hangouts_account, purple_core_get_ui(), FALSE);
	
	purple_account_set_string(new_account, "self_gaia_id", purple_account_get_string(hangouts_account, "self_gaia_id", NULL));
	
	purple_account_set_bool(new_account, "unravel_google_url", purple_account_get_bool(hangouts_account, "unravel_google_url", TRUE));
	purple_account_set_bool(new_account, "fetch_image_history", purple_account_get_bool(hangouts_account, "fetch_image_history", TRUE));
	purple_account_set_bool(new_account, "treat_invisible_as_offline", purple_account_get_bool(hangouts_account, "treat_invisible_as_offline", FALSE));
	purple_account_set_bool(new_account, "show-call-links", purple_account_get_bool(hangouts_account, "show-call-links", FALSE));
	purple_account_set_bool(new_account, "hide_self", purple_account_get_bool(hangouts_account, "hide_self", FALSE));
	
	purple_account_set_int(new_account, "last_event_timestamp_high", purple_account_get_int(hangouts_account, "last_event_timestamp_high", 0));
	purple_account_set_int(new_account, "last_event_timestamp_low", purple_account_get_int(hangouts_account, "last_event_timestamp_low", 0));
	
	purple_accounts_add(new_account);
	
	return !!purple_accounts_find(username, GOOGLECHAT_PLUGIN_ID);
}

static void
googlechat_account_added(PurpleAccount *account, gpointer userdata)
{
	if (g_strcmp0(purple_account_get_protocol_id(account), "prpl-hangouts") == 0) {
		if (purple_account_get_bool(account, "offered_to_move_to_googlechat", FALSE) == FALSE) {
			
			const gchar *username = purple_account_get_username(account);
			purple_account_set_bool(account, "offered_to_move_to_googlechat", TRUE);
			if (!purple_accounts_find(username, GOOGLECHAT_PLUGIN_ID)) {
				gchar *msg = g_strdup_printf(_("Existing Hangouts account for %s found.  Convert to a Google Chat account?"), username);
				purple_request_yes_no(NULL, _("Account Migration"),
						_("Account Migration"),
						msg,
						1,
						account, NULL, NULL,
						account, googlechat_create_account_from_hangouts_account,
						NULL);
				g_free(msg);
			}
			
		}
	}
}

static gboolean
googlechat_check_legacy_hangouts_accounts(gpointer ignored)
{
	PurpleAccount *account;
	GList *cur;
	
	for(cur = purple_accounts_get_all(); cur; cur = cur->next) {
		account = cur->data;
		googlechat_account_added(account, NULL);
	}
	
	return FALSE;
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_cmd_register("leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						GOOGLECHAT_PLUGIN_ID, googlechat_cmd_leave,
						_("leave:  Leave the group chat"), NULL);
						
	purple_cmd_register("kick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						GOOGLECHAT_PLUGIN_ID, googlechat_cmd_kick,
						_("kick <user>:  Kick a user from the room."), NULL);
	
	
	if (purple_accounts_get_all()) {
		googlechat_check_legacy_hangouts_accounts(NULL);
	} else {
		// The accounts (and their signals) get loaded up after plugins (so we have to wait a bit and try again)
		g_timeout_add_seconds(5, googlechat_check_legacy_hangouts_accounts, NULL);
	}
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_signals_disconnect_by_handle(plugin);
	
	return TRUE;
}

#if PURPLE_VERSION_CHECK(3, 0, 0)

G_MODULE_EXPORT GType googlechat_protocol_get_type(void);
#define GOOGLECHAT_TYPE_PROTOCOL			(googlechat_protocol_get_type())
#define GOOGLECHAT_PROTOCOL(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), GOOGLECHAT_TYPE_PROTOCOL, GoogleChatProtocol))
#define GOOGLECHAT_PROTOCOL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GOOGLECHAT_TYPE_PROTOCOL, GoogleChatProtocolClass))
#define GOOGLECHAT_IS_PROTOCOL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GOOGLECHAT_TYPE_PROTOCOL))
#define GOOGLECHAT_IS_PROTOCOL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GOOGLECHAT_TYPE_PROTOCOL))
#define GOOGLECHAT_PROTOCOL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GOOGLECHAT_TYPE_PROTOCOL, GoogleChatProtocolClass))

typedef struct _GoogleChatProtocol
{
	PurpleProtocol parent;
} GoogleChatProtocol;

typedef struct _GoogleChatProtocolClass
{
	PurpleProtocolClass parent_class;
} GoogleChatProtocolClass;

static void
googlechat_protocol_init(PurpleProtocol *prpl_info)
{
	PurpleProtocol *plugin = prpl_info, *info = prpl_info;

	info->id = GOOGLECHAT_PLUGIN_ID;
	info->name = "Google Chat";

	prpl_info->options = OPT_PROTO_NO_PASSWORD | OPT_PROTO_CHAT_TOPIC | OPT_PROTO_MAIL_CHECK;
	prpl_info->account_options = googlechat_add_account_options(prpl_info->account_options);
	
	purple_signal_register(plugin, "googlechat-received-event",
			purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_OBJECT);

	googlechat_register_events(plugin);
}

static void
googlechat_protocol_class_init(PurpleProtocolClass *prpl_info)
{
	prpl_info->login = googlechat_login;
	prpl_info->close = googlechat_close;
	prpl_info->status_types = googlechat_status_types;
	prpl_info->list_icon = googlechat_list_icon;
}

static void
googlechat_protocol_client_iface_init(PurpleProtocolClientIface *prpl_info)
{
	prpl_info->get_actions = googlechat_actions;
	prpl_info->blist_node_menu = googlechat_node_menu;
	prpl_info->status_text = googlechat_status_text;
	prpl_info->tooltip_text = googlechat_tooltip_text;
	prpl_info->buddy_free = googlechat_buddy_free;
 	prpl_info->offline_message = googlechat_offline_message;
}

static void
googlechat_protocol_server_iface_init(PurpleProtocolServerIface *prpl_info)
{
	prpl_info->get_info = googlechat_get_info;
	prpl_info->set_status = googlechat_set_status;
	prpl_info->set_idle = googlechat_set_idle;
}

static void
googlechat_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *prpl_info)
{
	prpl_info->add_deny = googlechat_block_user;
	prpl_info->rem_deny = googlechat_unblock_user;
}

static void 
googlechat_protocol_im_iface_init(PurpleProtocolIMIface *prpl_info)
{
	prpl_info->send = googlechat_send_im;
	prpl_info->send_typing = googlechat_send_typing;
}

static void 
googlechat_protocol_chat_iface_init(PurpleProtocolChatIface *prpl_info)
{
	prpl_info->send = googlechat_chat_send;
	prpl_info->info = googlechat_chat_info;
	prpl_info->info_defaults = googlechat_chat_info_defaults;
	prpl_info->join = googlechat_join_chat;
	prpl_info->get_name = googlechat_get_chat_name;
	prpl_info->invite = googlechat_chat_invite;
	prpl_info->set_topic = googlechat_chat_set_topic;
}

static void 
googlechat_protocol_media_iface_init(PurpleProtocolMediaIface *prpl_info)
{
	//prpl_info->get_caps = googlechat_get_media_caps;
	//prpl_info->initiate_session = googlechat_initiate_media;
}

static void 
googlechat_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *prpl_info)
{
	prpl_info->get_list = googlechat_roomlist_get_list;
}

static PurpleProtocol *googlechat_protocol;

PURPLE_DEFINE_TYPE_EXTENDED(
	GoogleChatProtocol, googlechat_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  googlechat_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  googlechat_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  googlechat_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  googlechat_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  googlechat_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_MEDIA_IFACE,
	                                  googlechat_protocol_media_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  googlechat_protocol_roomlist_iface_init)
);

static gboolean
libpurple3_plugin_load(PurplePlugin *plugin, GError **error)
{
	googlechat_protocol_register_type(plugin);
	googlechat_protocol = purple_protocols_add(GOOGLECHAT_TYPE_PROTOCOL, error);
	if (!googlechat_protocol)
		return FALSE;

	return plugin_load(plugin, error);
}

static gboolean
libpurple3_plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (!plugin_unload(plugin, error))
		return FALSE;

	if (!purple_protocols_remove(googlechat_protocol, error))
		return FALSE;

	return TRUE;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

	return purple_plugin_info_new(
		"id",          GOOGLECHAT_PLUGIN_ID,
		"name",        "Google Chat",
		"version",     GOOGLECHAT_PLUGIN_VERSION,
		"category",    N_("Protocol"),
		"summary",     N_("Google Chat Protocol Plugin."),
		"description", N_("Adds Google Chat support to libpurple."),
		"website",     "https://github.com/EionRobb/purple-googlechat/",
		"abi-version", PURPLE_ABI_VERSION,
		"flags",       PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		               PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

PURPLE_PLUGIN_INIT(googlechat, plugin_query,
		libpurple3_plugin_load, libpurple3_plugin_unload);

#else
	
// Normally set in core.c in purple3
void _purple_socket_init(void);
void _purple_socket_uninit(void);


static gboolean
libpurple2_plugin_load(PurplePlugin *plugin)
{
	_purple_socket_init();
	purple_http_init();
	
	return plugin_load(plugin, NULL);
}

static gboolean
libpurple2_plugin_unload(PurplePlugin *plugin)
{
	_purple_socket_uninit();
	purple_http_uninit();
	
	return plugin_unload(plugin, NULL);
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                               /**< ui_requirement */
	0,                                                  /**< flags          */
	NULL,                                               /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	GOOGLECHAT_PLUGIN_ID,                               /**< id             */
	N_("Google Chat"),                                  /**< name           */
	GOOGLECHAT_PLUGIN_VERSION,                          /**< version        */
	                                 
	N_("Google Chat Protocol Plugin."),                 /**< summary        */
	                                                  
	N_("Adds Google Chat support to libpurple."),       /**< description    */
	"Eion Robb <eionrobb+googlechat@gmail.com>",        /**< author         */
	"https://github.com/EionRobb/purple-googlechat/",   /**< homepage       */

	libpurple2_plugin_load,                             /**< load           */
	libpurple2_plugin_unload,                           /**< unload         */
	NULL,                                               /**< destroy        */

	NULL,                                               /**< ui_info        */
	NULL,                                               /**< extra_info     */
	NULL,                                               /**< prefs_info     */
	NULL,                                               /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	PurplePluginInfo *info;
	PurplePluginProtocolInfo *prpl_info = g_new0(PurplePluginProtocolInfo, 1);
	
#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif
	
	info = plugin->info;
	if (info == NULL) {
		plugin->info = info = g_new0(PurplePluginInfo, 1);
	}
	
	prpl_info->options = OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE | OPT_PROTO_CHAT_TOPIC | OPT_PROTO_MAIL_CHECK;
	prpl_info->protocol_options = googlechat_add_account_options(prpl_info->protocol_options);
	
	purple_signal_register(plugin, "googlechat-received-event",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			PURPLE_TYPE_CONNECTION,
			purple_value_new(PURPLE_TYPE_OBJECT));

	googlechat_register_events(plugin);

	prpl_info->login = googlechat_login;
	prpl_info->close = googlechat_close;
	prpl_info->status_types = googlechat_status_types;
	prpl_info->list_icon = googlechat_list_icon;
	prpl_info->status_text = googlechat_status_text;
	prpl_info->tooltip_text = googlechat_tooltip_text;
	prpl_info->buddy_free = googlechat_buddy_free;
	prpl_info->offline_message = googlechat_offline_message;
	
	prpl_info->get_info = googlechat_get_info;
	prpl_info->set_status = googlechat_set_status;
	prpl_info->set_idle = googlechat_set_idle;
	
	prpl_info->blist_node_menu = googlechat_node_menu;
	
	prpl_info->send_im = googlechat_send_im;
	prpl_info->send_typing = googlechat_send_typing;
	prpl_info->chat_send = googlechat_chat_send;
	prpl_info->chat_info = googlechat_chat_info;
	prpl_info->chat_info_defaults = googlechat_chat_info_defaults;
	prpl_info->join_chat = googlechat_join_chat;
	prpl_info->get_chat_name = googlechat_get_chat_name;
	prpl_info->chat_invite = googlechat_chat_invite;
	prpl_info->set_chat_topic = googlechat_chat_set_topic;
	
	//prpl_info->get_media_caps = googlechat_get_media_caps;
	//prpl_info->initiate_media = googlechat_initiate_media;
	
	prpl_info->add_deny = googlechat_block_user;
	prpl_info->rem_deny = googlechat_unblock_user;
	
	prpl_info->roomlist_get_list = googlechat_roomlist_get_list;
	
	info->extra_info = prpl_info;
	#if PURPLE_MINOR_VERSION >= 5
		prpl_info->struct_size = sizeof(PurplePluginProtocolInfo);
	#endif
	#if PURPLE_MINOR_VERSION >= 8
		//prpl_info->add_buddy_with_invite = skypeweb_add_buddy_with_invite;
	#endif
	
	info->actions = googlechat_actions;
}
	
PURPLE_INIT_PLUGIN(googlechat, init_plugin, info);

#endif
