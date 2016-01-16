
#include "libhangouts.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "debug.h"
#include "plugin.h"
#include "version.h"
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangouts.pb-c.h"

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
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
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
}
	

PURPLE_INIT_PLUGIN(hangouts, init_plugin, info);

