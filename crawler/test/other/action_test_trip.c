/*
OpenIO SDS crawler
Copyright (C) 2014 Worldine, original work as part of Redcurrant
Copyright (C) 2015 OpenIO, modified as part of OpenIO Software Defined Storage

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef G_LOG_DOMAIN
#define G_LOG_DOMAIN "atos.grid.action"
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sqlite3.h>
#include <autogen.h>
#include <generic.h>

#include <neon/ne_basic.h>
#include <neon/ne_request.h>
#include <neon/ne_session.h>

#include "lib/action_common.h"

#include "../meta2v2/meta2v2_remote.h"
#include "../metautils/lib/loggers.h"
#include "../metautils/lib/common_main.h"
#include "../metautils/lib/hc_url.h"
#include "../meta2v2/meta2_utils.h"
#include "../meta2v2/generic.h"
#include "../rules-motor/lib/motor.h"
#include "../rawx-lib/src/rawx.h"

static TCrawlerBus* conn;

static gboolean stop_thread;

static gchar* action_name;
static gchar* namespace_cmd_opt_name;
static gchar* dryrun_cmd_opt_name;
static gchar* timeout_cmd_opt_name;

gboolean g_dryrun_mode = FALSE;
static int service_pid;

static const gchar* occur_type_string;

static gchar     g_service_name[SERVICENAME_MAX_BYTES];
static char*     g_dbusdaemon_address = NULL;
static GMainLoop *g_main_loop = NULL;

//==============================================================================
// Listening message come from, and execute action function
//==============================================================================

/* ------- */
struct SParamMsgrx {
	gchar* namespace;
	const gchar* source_path;
	const gchar* meta2_url;
	gchar* dryrun;
	gdouble timeout_request;
};

void init_paramMsgRx(struct SParamMsgrx* pParam)
{
	if (pParam == NULL) return;

	memset(pParam, 0, sizeof(struct SParamMsgrx));
}

void clean_paramMsgRx(struct SParamMsgrx* pParam)
{
	if (pParam == NULL) return;

	if (pParam->namespace)    g_free(pParam->namespace);
	if (pParam->dryrun)       g_free(pParam->dryrun);

	init_paramMsgRx(pParam);
}

static gboolean extract_paramMsgRx(gboolean allParam,  TActParam* pActParam,
		struct SParamMsgrx* pParam)
{
	gchar* tmp = NULL;

	if (pParam == NULL)
		return FALSE;

	if (allParam == TRUE) {
		// Namespace extraction
		if (NULL == (pParam->namespace = get_argv_value(pActParam->argc, pActParam->argv, action_name,
						namespace_cmd_opt_name))) {
			GRID_TRACE("Failed to get namespace from args");
			return FALSE;
		}

		if (NULL == (pParam->dryrun = get_argv_value(pActParam->argc, pActParam->argv, action_name,
						dryrun_cmd_opt_name))) {
			g_dryrun_mode = FALSE;
		} else {
			g_dryrun_mode = TRUE;
			if (g_strcmp0(pParam->dryrun, "FALSE") == 0)
				g_dryrun_mode = FALSE;
		}

		pParam->timeout_request = 0;
		if (NULL != (tmp =  get_argv_value(pActParam->argc, pActParam->argv, action_name,
		                        timeout_cmd_opt_name))){
			int itmp = 0;
			if (sscanf(tmp, "%d", &itmp) != 1) 
				pParam->timeout_request = 0;
			else
				pParam->timeout_request = (gdouble) itmp;

			g_free(tmp);
		} 
		GRID_DEBUG("Timeout Request purge: %lf", pParam->timeout_request);		

		/* Checking occurence form */
		GVariantType* gvt = g_variant_type_new(occur_type_string);
		if (FALSE == g_variant_is_of_type(pActParam->occur, gvt)) {
			g_variant_type_free(gvt);
			return FALSE;
		}
		g_variant_type_free(gvt);
		gvt = NULL;

		/* ------- */
		/* Source path / meta2_url / ...  */
		pParam->source_path     = get_child_value_string(pActParam->occur, 0);
		pParam->meta2_url       = get_child_value_string(pActParam->occur, 1);
		/* ------- */
	} else {
		static char tmp[] = "";
		pParam->source_path = tmp;
		pParam->meta2_url   = tmp;
	}

	return TRUE;
}

gboolean action_set_data_trip_ex(TCrawlerBusObject *obj, const char* sender,
    const char *alldata, GError **error)
{
	GError* e = NULL;
	TActParam actparam;
	struct SParamMsgrx msgRx;
	int i;
	act_paramact_init(&actparam);
	init_paramMsgRx(&msgRx);

	(void) obj;

	GVariant* param = act_disassembleParam((char*) alldata, &actparam);

	GString* str = g_string_new("crawler{");
	g_string_append_printf(str, "context_id=%ld service_uid=%ld, ", 
			actparam.context_id, actparam.service_uid );
	
	g_string_append_printf(str, "argv(%d):", actparam.argc);
	for(i=0;i<actparam.argc;i++)
		g_string_append_printf(str, "[%s]", actparam.argv[i]);
	g_string_append_printf(str, "} ");

	gchar* s_signal_parameters = NULL;
	s_signal_parameters = g_variant_print(actparam.occur, FALSE);
	g_string_append_printf(str, ", trip{occur=[%s]}", s_signal_parameters);

	gchar* cstr = g_string_free(str, FALSE);
	fprintf(stdout, "> %s\n", cstr);
	g_free(cstr);
	

	// save response
	char* temp_msg = (char*)g_malloc0((SHORT_BUFFER_SIZE*sizeof(char)) + sizeof(guint64));
	sprintf(temp_msg, "%s on %s for the context %llu and the file %s",
			((!e)?ACK_OK:ACK_KO), action_name,
			(long long unsigned)actparam.context_id, msgRx.source_path);
	char *status = act_buildResponse(action_name, service_pid, actparam.context_id, temp_msg);
	g_free(temp_msg);

	// send it
    static TCrawlerReq* req = NULL;
    if (req)
        crawler_bus_req_clear(&req);

    GError* err = crawler_bus_req_init(conn, &req, sender, SERVICE_PATH, SERVICE_IFACE_CONTROL);
    if (err) {
        g_prefix_error(&err, "Failed to connectd to crawler services %s : ",
                        sender);
		GRID_WARN("Failed to send ack [%s]: %s", msgRx.source_path, err->message);
		g_clear_error(&err);
   }

    tlc_Send_Ack_noreply(req, NULL, ((!e)?ACK_OK:ACK_KO), status);

	g_free(status);

	act_paramact_clean(&actparam);
	clean_paramMsgRx(&msgRx);
	g_variant_unref(param);

	return TRUE;
}

gboolean action_command(TCrawlerBusObject *obj, const char* cmd, const char *alldata,
		char** status, GError **error)
{
	TActParam actparam;
	struct SParamMsgrx msgRx;
	act_paramact_init(&actparam);
	init_paramMsgRx(&msgRx);

	(void) obj;
	(void) status;

	GRID_DEBUG("%s...\n", __FUNCTION__);
	GVariant* param = act_disassembleParam((char*) alldata, &actparam);
	if (extract_paramMsgRx(FALSE, &actparam, &msgRx) == FALSE) {
		act_paramact_clean(&actparam);
		clean_paramMsgRx(&msgRx);
		g_variant_unref(param);
		*error = NEWERROR(1, "Bad format for received data");
		GRID_ERROR((*error)->message);
		return FALSE;
	}

	if (g_strcmp0(cmd, CMD_STARTTRIP) == 0) {
		//-----------------------
		// start process crawling		        
		GRID_INFO("start process's crawler");

		/* code here */

	} else  if (g_strcmp0(cmd, CMD_STOPTRIP) == 0) {
		//----------------------
		// end process crawling
		GRID_INFO("stop process's crawler");
		sleep(1);

		/* code here */

	} else {
		if (cmd)
			GRID_INFO("%s process's crawler", cmd);
		else
			GRID_INFO("%s process's crawler", "Unknown command");
	}

	GRID_DEBUG(">%s process's crawler\n", cmd);

	act_paramact_clean(&actparam);
	clean_paramMsgRx(&msgRx);
	 g_variant_unref(param);

	return TRUE;
}

/* GRID COMMON MAIN */
static struct grid_main_option_s *main_get_options(void)
{
	static struct grid_main_option_s options[] = {
		{ NULL, 0, {.b=NULL}, NULL }
	};

	return options;
}

static void main_action(void)
{
	GError* error = NULL;

	g_type_init();

	g_main_loop = g_main_loop_new (NULL, FALSE);

	/* DBus connexion */
	error = tlc_init_connection(&conn, g_service_name, SERVICE_PATH, 
								"" /*g_dbusdaemon_address*/ /*pour le bus system: =""*/, 
								(TCrawlerBusObjectInfo*) act_getObjectInfo());
	if (error) {
		GRID_ERROR("System D-Bus connection failed: %s",
				/*g_cfg_action_name, g_service_pid,*/ error->message);
		exit(EXIT_FAILURE);
	}

	GRID_INFO("%s (%d): System D-Bus %s action signal listening thread started...",
			action_name, service_pid, action_name);

	g_main_loop_run (g_main_loop);

	crawler_bus_Close(&conn);

	exit(EXIT_SUCCESS);
}

	static void
main_set_defaults(void)
{
	conn = NULL;
	stop_thread = FALSE;
	action_name = "action_test_trip";
	namespace_cmd_opt_name = "n";
	dryrun_cmd_opt_name = "dryrun";
	timeout_cmd_opt_name = "t";
	g_dryrun_mode = FALSE;
	service_pid = getpid();
	occur_type_string = "(ss)";

	buildServiceName(g_service_name, SERVICENAME_MAX_BYTES, 
					SERVICE_ACTION_NAME, action_name, service_pid, FALSE);
}

static void
main_specific_fini(void)
{
}

	static gboolean
main_configure(int argc, char **args)
{
	argc = argc;
	args = args;

    if (argc >= 1)         
		g_dbusdaemon_address = getBusAddress(args[0]);
   	GRID_DEBUG("dbus_daemon address:\"%s\"", g_dbusdaemon_address);

	return TRUE;
}

	static const gchar*
main_usage(void)
{
	return "";
}

	static void
main_specific_stop(void)
{
	stop_thread = TRUE;
	g_main_loop_quit(g_main_loop);
	GRID_INFO("%s (%d): System D-Bus %s action signal listening thread stopped...",
			action_name, service_pid, action_name);
}

static struct grid_main_callbacks cb = {
	.options = main_get_options,
	.action = main_action,
	.set_defaults = main_set_defaults,
	.specific_fini = main_specific_fini,
	.configure = main_configure,
	.usage = main_usage,
	.specific_stop = main_specific_stop
};

	int
main(int argc, char **argv)
{
	g_thread_init(NULL);
	dbus_g_thread_init();
	dbus_threads_init_default();

	return grid_main(argc, argv, &cb);
}

