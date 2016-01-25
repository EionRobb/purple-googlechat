
#include "libhangouts.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "debug.h"
#include "plugin.h"
#include "request.h"
#include "version.h"
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#include "hangouts_auth.h"
#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"


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

	pc = purple_account_get_connection(account);
	password = purple_connection_get_password(pc);
	
	ha = g_new0(HangoutsAccount, 1);
	ha->account = account;
	ha->pc = pc;
	ha->cookie_jar = purple_http_cookie_jar_new();
	ha->channel_buffer = purple_circular_buffer_new(0);
	ha->channel_keepalive_pool = purple_http_keepalive_pool_new();
	
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
	g_free(ha->refresh_token);
	g_free(ha->access_token);
	g_free(ha->gsessionid_param);
	g_free(ha->sid_param);
	g_free(ha->client_id);
	purple_http_cookie_jar_unref(ha->cookie_jar);
	purple_circular_buffer_destroy(ha->channel_buffer);
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
	
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;
}




/*****************************************************************************/

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_http_init();
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_http_uninit();
	return TRUE;
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
	"0.1",                                  /**< version        */
	                                                  /**  summary        */
	N_("Hangouts Protocol Plugins."),
	                                                  /**  description    */
	N_("Adds Hangouts protocol support to libpurple."),
	"Eion Robb <eionrobb+hangouts@gmail.com>",             /**< author         */
	"TODO",                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
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
	prpl_info->login = hangouts_login;
	prpl_info->close = hangouts_close;
	prpl_info->status_types = hangouts_status_types;
	prpl_info->list_icon = hangouts_list_icon;
	
	info->extra_info = prpl_info;
	#if PURPLE_MINOR_VERSION >= 5
		prpl_info->struct_size = sizeof(PurplePluginProtocolInfo);
	#endif
	#if PURPLE_MINOR_VERSION >= 8
		//prpl_info->add_buddy_with_invite = skypeweb_add_buddy_with_invite;
	#endif
	
	purple_signal_register(plugin, "hangouts-received-stateupdate",
			purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			purple_value_new(PURPLE_TYPE_SUBTYPE, PURPLE_SUBTYPE_CONNECTION),
			purple_value_new_outgoing(PURPLE_TYPE_OBJECT));
}
	

PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

