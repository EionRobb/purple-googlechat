
#include "libhangouts.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "cmds.h"
#include "debug.h"
#include "plugins.h"
#include "request.h"
#include "version.h"

#include "hangouts_auth.h"
#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts_events.h"
#include "hangouts_connection.h"
#include "hangouts_conversation.h"


/*****************************************************************************/
//TODO move to nicer place

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

static void
hangouts_blist_node_removed(PurpleBlistNode *node)
{
	PurpleChat *chat;
	PurpleAccount *account;
	const gchar *conv_id;
	
	if (!PURPLE_IS_CHAT(node)) {
		return;
	}
	chat = PURPLE_CHAT(node);
	account = purple_chat_get_account(chat);
	
	if (g_strcmp0(purple_account_get_protocol_id(account), HANGOUTS_PLUGIN_ID)) {
		return;
	}
	
    GHashTable *components = purple_chat_get_components(chat);
	conv_id = g_hash_table_lookup(components, "conv_id");
	if (conv_id == NULL) {
		conv_id = purple_chat_get_name_only(chat);
	}
	
	hangouts_chat_leave_by_conv_id(purple_account_get_connection(account), conv_id);
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
	purple_connection_set_flags(pc, pc_flags | PURPLE_CONNECTION_FLAG_HTML | PURPLE_CONNECTION_FLAG_NO_FONTSIZE | PURPLE_CONNECTION_FLAG_NO_BGCOLOR);
	
	ha = g_new0(HangoutsAccount, 1);
	ha->account = account;
	ha->pc = pc;
	ha->cookie_jar = purple_http_cookie_jar_new();
	ha->channel_buffer = g_byte_array_sized_new(HANGOUTS_BUFFER_DEFAULT_SIZE);
	ha->channel_keepalive_pool = purple_http_keepalive_pool_new();
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
		purple_notify_uri(pc, HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL);
		purple_request_input(pc, _("Authorization Code"), HANGOUTS_API_OAUTH2_AUTHORIZATION_CODE_URL,
			_ ("Please paste the Google OAuth code here"),
			NULL, FALSE, FALSE, NULL, 
			_("OK"), G_CALLBACK(hangouts_authcode_input_cb), 
			_("Cancel"), G_CALLBACK(hangouts_authcode_input_cancel_cb), 
			purple_request_cpar_from_connection(pc), ha);
	}
	
	purple_signal_connect(purple_blist_get_handle(), "blist-node-removed", account, PURPLE_CALLBACK(hangouts_blist_node_removed), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "conversation-updated", account, PURPLE_CALLBACK(hangouts_mark_conversation_seen), NULL);
	
	purple_timeout_add_seconds(HANGOUTS_ACTIVE_CLIENT_TIMEOUT, ((GSourceFunc) hangouts_set_active_client), pc);
}

static void
hangouts_close(PurpleConnection *pc)
{
	HangoutsAccount *ha; //not so funny anymore
	
	ha = purple_connection_get_protocol_data(pc);
	purple_signals_disconnect_by_handle(ha->account);
	
	purple_http_conn_cancel_all(pc);
	
	purple_http_keepalive_pool_unref(ha->channel_keepalive_pool);
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



/*****************************************************************************/

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_cmd_register("leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
						PURPLE_CMD_FLAG_PROTOCOL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
						HANGOUTS_PLUGIN_ID, hangouts_cmd_leave,
						_("leave:  Leave the group chat"), NULL);
	
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

	prpl_info->options = OPT_PROTO_NO_PASSWORD;

	
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
}

static void
hangouts_protocol_server_iface_init(PurpleProtocolServerIface *prpl_info)
{
	prpl_info->set_status = hangouts_set_status;
	prpl_info->set_idle = hangouts_set_idle;
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
	
	prpl_info->options = OPT_PROTO_NO_PASSWORD;

	
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
