
#include "libhangouts.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "debug.h"
#include "plugins.h"
#include "request.h"
#include "version.h"

#include "hangouts_auth.h"
#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"
#include "hangouts_events.h"
#include "hangouts_conversation.h"


/*****************************************************************************/
//TODO move to nicer place



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
	PurpleConnectionFlags pc_flags;

	pc = purple_account_get_connection(account);
	password = purple_connection_get_password(pc);
	
	pc_flags = purple_connection_get_flags(pc);
	purple_connection_set_flags(pc, pc_flags | PURPLE_CONNECTION_FLAG_HTML | PURPLE_CONNECTION_FLAG_NO_FONTSIZE | PURPLE_CONNECTION_FLAG_NO_BGCOLOR);
	
	ha = g_new0(HangoutsAccount, 1);
	ha->account = account;
	ha->pc = pc;
	ha->cookie_jar = purple_http_cookie_jar_new();
	ha->channel_buffer = purple_circular_buffer_new(0);
	ha->channel_keepalive_pool = purple_http_keepalive_pool_new();
	ha->sent_message_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
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
}

static void
hangouts_close(PurpleConnection *pc)
{
	HangoutsAccount *ha; //not so funny anymore
	
	ha = purple_connection_get_protocol_data(pc);
	
	purple_http_conn_cancel_all(pc);
	
	purple_http_keepalive_pool_unref(ha->channel_keepalive_pool);
	g_free(ha->self_gaia_id);
	g_free(ha->refresh_token);
	g_free(ha->access_token);
	g_free(ha->gsessionid_param);
	g_free(ha->sid_param);
	g_free(ha->client_id);
	purple_http_cookie_jar_unref(ha->cookie_jar);
	purple_circular_buffer_destroy(ha->channel_buffer);
	
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
hangouts_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "hangouts";
}


GList *
hangouts_status_types(PurpleAccount *account)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_AWAY, NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_EXTENDED_AWAY, NULL, NULL, FALSE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}




/*****************************************************************************/

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_http_init();
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_http_uninit();
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
hangouts_protocol_im_iface_init(PurpleProtocolIMIface *prpl_info)
{
	prpl_info->send = hangouts_send_im;
	prpl_info->send_typing = hangouts_send_typing;
}

static void 
hangouts_protocol_chat_iface_init(PurpleProtocolChatIface *prpl_info)
{
	prpl_info->send = hangouts_chat_send;
	prpl_info->info_defaults = hangouts_chat_info_defaults;
}

static PurpleProtocol *hangouts_protocol;

PURPLE_DEFINE_TYPE_EXTENDED(
	HangoutsProtocol, hangouts_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  hangouts_protocol_im_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  hangouts_protocol_chat_iface_init)
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
		"id",		HANGOUTS_PLUGIN_ID,
		"name",		"Hangouts",
		"version",	HANGOUTS_PLUGIN_VERSION,
		"category",	N_("Protocol"),
		"summary",	N_("Hangouts Protocol Plugins."),
		"description",	N_("Adds Hangouts protocol support to libpurple."),
		"website",	"TODO",
		"abi-version",	PURPLE_ABI_VERSION,
		"flags",	PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
				PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

PURPLE_PLUGIN_INIT(hangouts, plugin_query,
		libpurple3_plugin_load, libpurple3_plugin_unload);

#else

static gboolean
libpurple2_plugin_load(PurplePlugin *plugin)
{
	return plugin_load(plugin, NULL);
}

static gboolean
libpurple2_plugin_unload(PurplePlugin *plugin)
{
	return plugin_unload(plugin, NULL);
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	HANGOUTS_PLUGIN_ID,                               /**< id             */
	N_("Hangouts"),                           /**< name           */
	HANGOUTS_PLUGIN_VERSION,                          /**< version        */
	                                                  /**  summary        */
	N_("Hangouts Protocol Plugins."),
	                                                  /**  description    */
	N_("Adds Hangouts protocol support to libpurple."),
	"Eion Robb <eionrobb+hangouts@gmail.com>",             /**< author         */
	"TODO",                                     /**< homepage       */

	libpurple2_plugin_load,                           /**< load           */
	libpurple2_plugin_unload,                         /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                      /**< prefs_info     */
	NULL,                                             /**< actions        */

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
	
	prpl_info->send_im = hangouts_send_im;
	prpl_info->send_typing = hangouts_send_typing;
	prpl_info->chat_send = hangouts_chat_send;
	prpl_info->chat_info_defaults = hangouts_chat_info_defaults;
	
	info->extra_info = prpl_info;
	#if PURPLE_MINOR_VERSION >= 5
		prpl_info->struct_size = sizeof(PurplePluginProtocolInfo);
	#endif
	#if PURPLE_MINOR_VERSION >= 8
		//prpl_info->add_buddy_with_invite = skypeweb_add_buddy_with_invite;
	#endif
}
	
PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

#endif
