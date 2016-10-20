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

#include "libhangouts.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "accountopt.h"
#include "cmds.h"
#include "debug.h"
#include "mediamanager.h"
#include "plugins.h"
#include "request.h"
#include "version.h"

#include "hangouts_auth.h"
#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts_events.h"
#include "hangouts_connection.h"
#include "hangouts_conversation.h"
#include "hangouts_media.h"


/*****************************************************************************/
//TODO move to nicer place

gboolean
hangouts_is_valid_id(const gchar *id)
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
hangouts_get_media_caps(PurpleAccount *account, const char *who)
{
	return PURPLE_MEDIA_CAPS_AUDIO | PURPLE_MEDIA_CAPS_VIDEO | PURPLE_MEDIA_CAPS_AUDIO_VIDEO | PURPLE_MEDIA_CAPS_MODIFY_SESSION;
}

void
hangouts_set_idle(PurpleConnection *pc, int time)
{
	HangoutsAccount *ha;
	
	ha = purple_connection_get_protocol_data(pc);
	
	if (time < HANGOUTS_ACTIVE_CLIENT_TIMEOUT) {
		hangouts_set_active_client(pc);
	}
	ha->idle_time = time;
}

static GList *
hangouts_add_account_options(GList *account_options)
{
	PurpleAccountOption *option;
	
	option = purple_account_option_bool_new(N_("Show call links in chat"), "show-call-links", !purple_media_manager_get());
	account_options = g_list_append(account_options, option);
	
	option = purple_account_option_bool_new(N_("Un-Googlify URLs"), "unravel_google_url", FALSE);
	account_options = g_list_append(account_options, option);
	
	return account_options;
}

static void
hangouts_blist_node_removed(PurpleBlistNode *node)
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
	
	if (g_strcmp0(purple_account_get_protocol_id(account), HANGOUTS_PLUGIN_ID)) {
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
		
		hangouts_chat_leave_by_conv_id(pc, conv_id, NULL);
	} else {
		HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
		conv_id = g_hash_table_lookup(ha->one_to_ones_rev, purple_buddy_get_name(buddy));
		
		hangouts_archive_conversation(ha, conv_id);
	}
}

static void
hangouts_blist_node_aliased(PurpleBlistNode *node, const char *old_alias)
{
	PurpleChat *chat = NULL;
	PurpleAccount *account = NULL;
	PurpleConnection *pc;
	const gchar *conv_id;
	GHashTable *components;
	HangoutsAccount *ha;
	const gchar *new_alias;
	
	if (PURPLE_IS_CHAT(node)) {
		chat = PURPLE_CHAT(node);
		account = purple_chat_get_account(chat);
	}
	
	if (account == NULL) {
		return;
	}
	
	if (g_strcmp0(purple_account_get_protocol_id(account), HANGOUTS_PLUGIN_ID)) {
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
		if (g_strcmp0(old_alias, new_alias)) {
			components = purple_chat_get_components(chat);
			conv_id = g_hash_table_lookup(components, "conv_id");
			if (conv_id == NULL) {
				conv_id = purple_chat_get_name_only(chat);
			}
			
			hangouts_rename_conversation(ha, conv_id, new_alias);
		}
	}
}

static void
hangouts_chat_set_topic(PurpleConnection *pc, int id, const char *topic)
{
	const gchar *conv_id;
	PurpleChatConversation *chatconv;
	HangoutsAccount *ha;
	
	ha = purple_connection_get_protocol_data(pc);
	chatconv = purple_conversations_find_chat(pc, id);
	conv_id = purple_conversation_get_data(PURPLE_CONVERSATION(chatconv), "conv_id");
	if (conv_id == NULL) {
		// Fix for a race condition around the chat data and serv_got_joined_chat()
		conv_id = purple_conversation_get_name(PURPLE_CONVERSATION(chatconv));
	}
	
	return hangouts_rename_conversation(ha, conv_id, topic);
}

static PurpleCmdRet
hangouts_cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	hangouts_chat_leave(pc, id);
	
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
hangouts_cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleConnection *pc = NULL;
	int id = -1;
	
	pc = purple_conversation_get_connection(conv);
	id = purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv));
	
	if (pc == NULL || id == -1)
		return PURPLE_CMD_RET_FAILED;
	
	hangouts_chat_kick(pc, id, args[0]);
	
	return PURPLE_CMD_RET_OK;
}

static GList *
hangouts_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	
	if(PURPLE_IS_BUDDY(node))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
					PURPLE_CALLBACK(hangouts_initiate_chat_from_node),
					NULL, NULL);
		m = g_list_append(m, act);
	} else if (PURPLE_IS_CHAT(node)) {
		act = purple_menu_action_new(_("_Leave Chat"),
					PURPLE_CALLBACK(hangouts_blist_node_removed), // A strange coinkidink
					NULL, NULL);
		m = g_list_append(m, act);
	}
	
	return m;
}

static GList *
hangouts_actions(
#if !PURPLE_VERSION_CHECK(3, 0, 0)
PurplePlugin *plugin, gpointer context
#else
PurpleConnection *pc
#endif
)
{
	GList *m = NULL;
	PurpleProtocolAction *act;

	act = purple_protocol_action_new(_("Search for friends..."), hangouts_search_users);
	m = g_list_append(m, act);

	return m;
}

static void
hangouts_authcode_input_cb(gpointer user_data, const gchar *auth_code)
{
	HangoutsAccount *ha = user_data;
	PurpleConnection *pc = ha->pc;

	purple_connection_update_progress(pc, _("Authenticating"), 1, 3);
	hangouts_oauth_with_code(ha, auth_code);
}

static void
hangouts_authcode_input_cancel_cb(gpointer user_data)
{
	HangoutsAccount *ha = user_data;
	purple_connection_error(ha->pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE, 
		_("User cancelled authorization"));
}

static void
hangouts_login(PurpleAccount *account)
{
	PurpleConnection *pc;
	HangoutsAccount *ha; //hahaha
	const gchar *password;
	const gchar *self_gaia_id;
	PurpleConnectionFlags pc_flags;

	pc = purple_account_get_connection(account);
	password = purple_connection_get_password(pc);
	
	pc_flags = purple_connection_get_flags(pc);
	pc_flags |= PURPLE_CONNECTION_FLAG_HTML;
	pc_flags |= PURPLE_CONNECTION_FLAG_NO_FONTSIZE;
	pc_flags |= PURPLE_CONNECTION_FLAG_NO_BGCOLOR;
	pc_flags &= ~PURPLE_CONNECTION_FLAG_NO_IMAGES;
	purple_connection_set_flags(pc, pc_flags);
	
	ha = g_new0(HangoutsAccount, 1);
	ha->account = account;
	ha->pc = pc;
	ha->cookie_jar = purple_http_cookie_jar_new();
	ha->channel_buffer = g_byte_array_sized_new(HANGOUTS_BUFFER_DEFAULT_SIZE);
	ha->channel_keepalive_pool = purple_http_keepalive_pool_new();
	ha->client6_keepalive_pool = purple_http_keepalive_pool_new();
	ha->sent_message_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	ha->one_to_ones = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	ha->one_to_ones_rev = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	ha->group_chats = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ha->google_voice_conversations = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	self_gaia_id = purple_account_get_string(account, "self_gaia_id", NULL);
	if (self_gaia_id != NULL) {
		ha->self_gaia_id = g_strdup(self_gaia_id);
		purple_connection_set_display_name(pc, ha->self_gaia_id);
	}
	
	purple_connection_set_protocol_data(pc, ha);
	
	if (password && *password) {
		ha->refresh_token = g_strdup(password);
		purple_connection_update_progress(pc, _("Authenticating"), 1, 3);
		hangouts_oauth_refresh_token(ha);
	} else {
		//TODO get this code automatically
		purple_notify_uri(pc, "https://www.youtube.com/watch?v=hlDhp-eNLMU");
		purple_request_input(pc, _("Authorization Code"), "https://www.youtube.com/watch?v=hlDhp-eNLMU",
			_ ("Please follow the YouTube video to get the OAuth code"),
			_ ("and then paste the Google OAuth code here"), FALSE, FALSE, NULL, 
			_("OK"), G_CALLBACK(hangouts_authcode_input_cb), 
			_("Cancel"), G_CALLBACK(hangouts_authcode_input_cancel_cb), 
			purple_request_cpar_from_connection(pc), ha);
	}
	
	purple_signal_connect(purple_blist_get_handle(), "blist-node-removed", account, PURPLE_CALLBACK(hangouts_blist_node_removed), NULL);
	purple_signal_connect(purple_blist_get_handle(), "blist-node-aliased", account, PURPLE_CALLBACK(hangouts_blist_node_aliased), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", account, PURPLE_CALLBACK(hangouts_mark_conversation_seen), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "chat-conversation-typing", account, PURPLE_CALLBACK(hangouts_conv_send_typing), ha);
	
	ha->active_client_timeout = purple_timeout_add_seconds(HANGOUTS_ACTIVE_CLIENT_TIMEOUT, ((GSourceFunc) hangouts_set_active_client), pc);
}

static void
hangouts_close(PurpleConnection *pc)
{
	HangoutsAccount *ha; //not so funny anymore
	
	ha = purple_connection_get_protocol_data(pc);
	purple_signals_disconnect_by_handle(ha->account);
	
	purple_timeout_remove(ha->active_client_timeout);
	purple_timeout_remove(ha->channel_watchdog);
	
	purple_http_conn_cancel_all(pc);
	
	purple_http_keepalive_pool_unref(ha->channel_keepalive_pool);
	purple_http_keepalive_pool_unref(ha->client6_keepalive_pool);
	g_free(ha->self_gaia_id);
	g_free(ha->self_phone);
	g_free(ha->refresh_token);
	g_free(ha->access_token);
	g_free(ha->gsessionid_param);
	g_free(ha->sid_param);
	g_free(ha->client_id);
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
	g_hash_table_remove_all(ha->google_voice_conversations);
	g_hash_table_unref(ha->google_voice_conversations);
	
	g_free(ha);
}


static const char *
hangouts_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "hangouts";
}

static void
hangouts_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const gchar *message;
	HangoutsBuddy *hbuddy;
	
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
			const time_t last_seen = hbuddy->last_seen;
			purple_notify_user_info_add_pair_html(user_info, _("Last seen"), purple_date_format_full(localtime(&last_seen)));
		}
		
		if (hbuddy->in_call) {
			purple_notify_user_info_add_pair_html(user_info, _("In call"), NULL);
		}
		
		if (hbuddy->device_type) {
			purple_notify_user_info_add_pair_html(user_info, _("Device Type"), 
				hbuddy->device_type & HANGOUTS_DEVICE_TYPE_DESKTOP ? _("Desktop") :
				hbuddy->device_type & HANGOUTS_DEVICE_TYPE_TABLET ? _("Tablet") :
				hbuddy->device_type & HANGOUTS_DEVICE_TYPE_MOBILE ? _("Mobile") :
				_("Unknown"));
		}
	}
}

GList *
hangouts_status_types(PurpleAccount *account)
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
	
	status = purple_status_type_new_with_attrs(PURPLE_STATUS_INVISIBLE, NULL, NULL, FALSE, FALSE, FALSE, "message", _("Mood"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, status);

	return types;
}

static gchar *
hangouts_status_text(PurpleBuddy *buddy)
{
	const gchar *message = purple_status_get_attr_string(purple_presence_get_active_status(purple_buddy_get_presence(buddy)), "message");
	
	if (message == NULL) {
		return NULL;
	}
	
	return g_markup_printf_escaped("%s", message);
}

static void
hangouts_buddy_free(PurpleBuddy *buddy)
{
	HangoutsBuddy *hbuddy = purple_buddy_get_protocol_data(buddy);
	
	g_return_if_fail(hbuddy != NULL);
	
	g_free(hbuddy);
}


/*****************************************************************************/

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_cmd_register("leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						HANGOUTS_PLUGIN_ID, hangouts_cmd_leave,
						_("leave:  Leave the group chat"), NULL);
						
	purple_cmd_register("kick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						HANGOUTS_PLUGIN_ID, hangouts_cmd_kick,
						_("kick <user>:  Kick a user from the room."), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_signals_disconnect_by_handle(plugin);
	
	return TRUE;
}

#if PURPLE_VERSION_CHECK(3, 0, 0)

G_MODULE_EXPORT GType hangouts_protocol_get_type(void);
#define HANGOUTS_TYPE_PROTOCOL			(hangouts_protocol_get_type())
#define HANGOUTS_PROTOCOL(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocol))
#define HANGOUTS_PROTOCOL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocolClass))
#define HANGOUTS_IS_PROTOCOL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), HANGOUTS_TYPE_PROTOCOL))
#define HANGOUTS_IS_PROTOCOL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), HANGOUTS_TYPE_PROTOCOL))
#define HANGOUTS_PROTOCOL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), HANGOUTS_TYPE_PROTOCOL, HangoutsProtocolClass))

typedef struct _HangoutsProtocol
{
	PurpleProtocol parent;
} HangoutsProtocol;

typedef struct _HangoutsProtocolClass
{
	PurpleProtocolClass parent_class;
} HangoutsProtocolClass;

static void
hangouts_protocol_init(PurpleProtocol *prpl_info)
{
	PurpleProtocol *plugin = prpl_info, *info = prpl_info;

	info->id = HANGOUTS_PLUGIN_ID;
	info->name = "Hangouts";

	prpl_info->options = OPT_PROTO_NO_PASSWORD | OPT_PROTO_CHAT_TOPIC;
	prpl_info->account_options = hangouts_add_account_options(prpl_info->account_options);
	
	purple_signal_register(plugin, "hangouts-received-stateupdate",
			purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			PURPLE_TYPE_CONNECTION,
			G_TYPE_OBJECT);

	hangouts_register_events(plugin);
}

static void
hangouts_protocol_class_init(PurpleProtocolClass *prpl_info)
{
	prpl_info->login = hangouts_login;
	prpl_info->close = hangouts_close;
	prpl_info->status_types = hangouts_status_types;
	prpl_info->list_icon = hangouts_list_icon;
}

static void
hangouts_protocol_client_iface_init(PurpleProtocolClientIface *prpl_info)
{
	prpl_info->get_actions = hangouts_actions;
	prpl_info->blist_node_menu = hangouts_node_menu;
	prpl_info->status_text = hangouts_status_text;
	prpl_info->tooltip_text = hangouts_tooltip_text;
	prpl_info->buddy_free = hangouts_buddy_free;
}

static void
hangouts_protocol_server_iface_init(PurpleProtocolServerIface *prpl_info)
{
	prpl_info->get_info = hangouts_get_info;
	prpl_info->set_status = hangouts_set_status;
	prpl_info->set_idle = hangouts_set_idle;
}

static void
hangouts_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *prpl_info)
{
	prpl_info->add_deny = hangouts_block_user;
	prpl_info->rem_deny = hangouts_unblock_user;
}

static void 
hangouts_protocol_im_iface_init(PurpleProtocolIMIface *prpl_info)
{
	prpl_info->send = hangouts_send_im;
	prpl_info->send_typing = hangouts_send_typing;
}

static void 
hangouts_protocol_chat_iface_init(PurpleProtocolChatIface *prpl_info)
{
	prpl_info->send = hangouts_chat_send;
	prpl_info->info = hangouts_chat_info;
	prpl_info->info_defaults = hangouts_chat_info_defaults;
	prpl_info->join = hangouts_join_chat;
	prpl_info->get_name = hangouts_get_chat_name;
	prpl_info->invite = hangouts_chat_invite;
	prpl_info->set_topic = hangouts_chat_set_topic;
}

static void 
hangouts_protocol_media_iface_init(PurpleProtocolMediaIface *prpl_info)
{
	prpl_info->get_caps = hangouts_get_media_caps;
	prpl_info->initiate_session = hangouts_initiate_media;
}

static void 
hangouts_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *prpl_info)
{
	prpl_info->get_list = hangouts_roomlist_get_list;
}

static PurpleProtocol *hangouts_protocol;

PURPLE_DEFINE_TYPE_EXTENDED(
	HangoutsProtocol, hangouts_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  hangouts_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  hangouts_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  hangouts_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  hangouts_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  hangouts_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_MEDIA_IFACE,
	                                  hangouts_protocol_media_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  hangouts_protocol_roomlist_iface_init)
);

static gboolean
libpurple3_plugin_load(PurplePlugin *plugin, GError **error)
{
	hangouts_protocol_register_type(plugin);
	hangouts_protocol = purple_protocols_add(HANGOUTS_TYPE_PROTOCOL, error);
	if (!hangouts_protocol)
		return FALSE;

	return plugin_load(plugin, error);
}

static gboolean
libpurple3_plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (!plugin_unload(plugin, error))
		return FALSE;

	if (!purple_protocols_remove(hangouts_protocol, error))
		return FALSE;

	return TRUE;
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",          HANGOUTS_PLUGIN_ID,
		"name",        "Hangouts",
		"version",     HANGOUTS_PLUGIN_VERSION,
		"category",    N_("Protocol"),
		"summary",     N_("Hangouts Protocol Plugins."),
		"description", N_("Adds Hangouts protocol support to libpurple."),
		"website",     "https://bitbucket.org/EionRobb/purple-hangouts/",
		"abi-version", PURPLE_ABI_VERSION,
		"flags",       PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		               PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

PURPLE_PLUGIN_INIT(hangouts, plugin_query,
		libpurple3_plugin_load, libpurple3_plugin_unload);

#else
	
// Normally set in core.c in purple3
void _purple_socket_init(void);
void _purple_socket_uninit(void);

// See workaround for purple_chat_conversation_find_user() in purplecompat.h
static void
hangouts_deleting_chat_buddy(PurpleConvChatBuddy *cb)
{
	if (g_dataset_get_data(cb, "chat") != NULL) {
		g_dataset_destroy(cb);
	}
}

static gboolean
libpurple2_plugin_load(PurplePlugin *plugin)
{
	_purple_socket_init();
	purple_http_init();
	
	purple_signal_connect(purple_conversations_get_handle(), "deleting-chat-buddy", plugin, PURPLE_CALLBACK(hangouts_deleting_chat_buddy), NULL);
	
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

	HANGOUTS_PLUGIN_ID,                                 /**< id             */
	N_("Hangouts"),                                     /**< name           */
	HANGOUTS_PLUGIN_VERSION,                            /**< version        */
	                                 
	N_("Hangouts Protocol Plugins."),                   /**< summary        */
	                                                  
	N_("Adds Hangouts protocol support to libpurple."), /**< description    */
	"Eion Robb <eionrobb+hangouts@gmail.com>",          /**< author         */
	"https://bitbucket.org/EionRobb/purple-hangouts/",  /**< homepage       */

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
	
	info = plugin->info;
	if (info == NULL) {
		plugin->info = info = g_new0(PurplePluginInfo, 1);
	}
	
	prpl_info->options = OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE | OPT_PROTO_CHAT_TOPIC;
	prpl_info->protocol_options = hangouts_add_account_options(prpl_info->protocol_options);
	
	purple_signal_register(plugin, "hangouts-received-stateupdate",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			PURPLE_TYPE_CONNECTION,
			purple_value_new_outgoing(PURPLE_TYPE_OBJECT));

	hangouts_register_events(plugin);

	prpl_info->login = hangouts_login;
	prpl_info->close = hangouts_close;
	prpl_info->status_types = hangouts_status_types;
	prpl_info->list_icon = hangouts_list_icon;
	prpl_info->status_text = hangouts_status_text;
	prpl_info->tooltip_text = hangouts_tooltip_text;
	prpl_info->buddy_free = hangouts_buddy_free;
	
	prpl_info->get_info = hangouts_get_info;
	prpl_info->set_status = hangouts_set_status;
	prpl_info->set_idle = hangouts_set_idle;
	
	prpl_info->blist_node_menu = hangouts_node_menu;
	
	prpl_info->send_im = hangouts_send_im;
	prpl_info->send_typing = hangouts_send_typing;
	prpl_info->chat_send = hangouts_chat_send;
	prpl_info->chat_info = hangouts_chat_info;
	prpl_info->chat_info_defaults = hangouts_chat_info_defaults;
	prpl_info->join_chat = hangouts_join_chat;
	prpl_info->get_chat_name = hangouts_get_chat_name;
	prpl_info->chat_invite = hangouts_chat_invite;
	prpl_info->set_chat_topic = hangouts_chat_set_topic;
	
	prpl_info->get_media_caps = hangouts_get_media_caps;
	prpl_info->initiate_media = hangouts_initiate_media;
	
	prpl_info->add_deny = hangouts_block_user;
	prpl_info->rem_deny = hangouts_unblock_user;
	
	prpl_info->roomlist_get_list = hangouts_roomlist_get_list;
	
	info->extra_info = prpl_info;
	#if PURPLE_MINOR_VERSION >= 5
		prpl_info->struct_size = sizeof(PurplePluginProtocolInfo);
	#endif
	#if PURPLE_MINOR_VERSION >= 8
		//prpl_info->add_buddy_with_invite = skypeweb_add_buddy_with_invite;
	#endif
	
	info->actions = hangouts_actions;
}
	
PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

#endif
