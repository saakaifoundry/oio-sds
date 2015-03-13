/*
OpenIO SDS cluster
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
# define G_LOG_DOMAIN "gridcluster.remote"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <metautils/lib/metacomm.h>
#include <cluster/module/module.h>
#include <cluster/events/gridcluster_events.h>
#include <cluster/events/gridcluster_eventsremote.h>

#include "gridcluster_remote.h"

static MESSAGE
build_request(gchar * req_name, void *body, gsize body_size)
{
	MESSAGE req = message_create();
	if (body)
		message_set_BODY(req, body, body_size, NULL);
	message_set_NAME(req, req_name, strlen(req_name), NULL);
	return (req);
}

static gboolean
namespace_info_full_handler(GError ** error, gpointer udata, gint code, guint8 * body, gsize bodySize)
{
	(void) error, (void) code, (void) bodySize;
	namespace_info_t **ns_info = (namespace_info_t **) udata;
	*ns_info = namespace_info_unmarshall(body, bodySize, error);
	return (TRUE);
}

static gboolean
container_list_content_handler(GError ** error, gpointer udata, gint code, guint8 * body, gsize bodySize)
{
	(void) code;
	GSList **the_list = (GSList **) udata;
	GSList *list_from_body = meta2_maintenance_names_unmarshall_buffer(body, bodySize, error);
	if (!list_from_body) {
		GSETERROR(error, "Cannot unmarshall body of message as broken containers");
		return (FALSE);
	}

	*the_list = g_slist_concat(*the_list, list_from_body);
	return (TRUE);
}

meta0_info_t *
gcluster_get_meta0_2timeouts(addr_info_t * addr, long to_cnx, long to_req, GError ** error)
{
	meta0_info_t *m0 = NULL;
	GSList *result = gcluster_get_services2(addr, to_cnx, to_req, "meta0", error);
	if (result) {
		m0 = service_info_convert_to_m0info(g_slist_nth_data(result,rand()%g_slist_length(result)));
		g_slist_foreach(result, service_info_gclean, NULL);
		g_slist_free(result);
	}

	return m0;
}

meta0_info_t *
gcluster_get_meta0(addr_info_t * addr, long timeout, GError ** error)
{
	return gcluster_get_meta0_2timeouts(addr, timeout, timeout, error);
}

namespace_info_t *
gcluster_get_namespace_info_full(addr_info_t * addr, long timeout, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, &namespace_info_full_handler, NULL},
		{0, 0, NULL, NULL}
	};
	namespace_info_t *ns_info = NULL;
	struct reply_sequence_data_s data = { &ns_info, 0, codes };
	MESSAGE req = build_request(NAME_MSGNAME_CS_GET_NSINFO, NULL, 0);

	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses", NAME_MSGNAME_CS_GET_NSINFO);
		goto error_reply;
	}

	message_destroy(req);
	return (ns_info);

error_reply:
	message_destroy(req);
	return (NULL);
}

gint
gcluster_push_broken_container(addr_info_t * addr, long timeout, GSList * container_list, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, NULL, NULL},
		{0, 0, NULL, NULL}
	};
	struct reply_sequence_data_s data = { NULL, 0, codes };

	GByteArray *buf = meta2_maintenance_names_marshall(container_list, error);
	if (!buf) {
		GSETERROR(error, "Failed to marshall container list");
		return 0;
	}

	MESSAGE req = build_request(NAME_MSGNAME_CS_PUSH_BROKEN_CONT, buf->data, buf->len);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses",
				NAME_MSGNAME_CS_PUSH_BROKEN_CONT);
		goto error_reply;
	}

	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (1);

error_reply:
	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (0);
}

gint
gcluster_push_virtual_ns_space_used(addr_info_t * addr, long timeout, GHashTable *space_used, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, NULL, NULL},
		{0, 0, NULL, NULL}
	};
	struct reply_sequence_data_s data = { NULL, 0, codes };
	MESSAGE req = NULL;
	GSList *kv_list = NULL;

	/* send the hashtable in the body */
	kv_list = key_value_pairs_convert_from_map(space_used, FALSE, error);
	if (!kv_list) {
		GSETERROR(error, "Conversion HashTable->List failure");
		return (0);
	}

	/*encode the list */
	GByteArray *buf = key_value_pairs_marshall_gba(kv_list, error);
	if (!buf) {
		GSETERROR(error, "Failed to marshall kv list");
		goto error_marshall;
	}

	req = build_request(NAME_MSGNAME_CS_PUSH_VNS_SPACE_USED, buf->data, buf->len);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses",
				NAME_MSGNAME_CS_PUSH_BROKEN_CONT);
		goto error_reply;
	}

	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	g_slist_free_full (kv_list, g_free0);
	return (1);

error_reply:
	message_destroy(req);
	g_byte_array_free(buf, TRUE);
error_marshall:
	g_slist_free_full(kv_list, g_free0);
	return (0);
}

gint
gcluster_v2_push_broken_container(struct metacnx_ctx_s *cnx, const gchar *ns_name, const container_id_t cid, GError **error)
{
	GError *event_error = NULL;
	gchar str_ueid[256];
	gboolean rc;
	gridcluster_event_t *event;

	event = gridcluster_create_event();
	gridcluster_event_set_type(event, "broken.CONTAINER");
	gridcluster_event_add_buffer(event,"CONTAINER", cid, sizeof(container_id_t));
	gridcluster_event_add_string(event,"NAMESPACE", ns_name);
	g_snprintf(str_ueid, sizeof(str_ueid), "%ld_%d_%p", random(), getpid(), &str_ueid);
	rc = gridcluster_push_event(cnx, str_ueid, event, &event_error, error);
	gridcluster_destroy_event(event);
	if (!rc) {
		GSETERROR(error,"Failed to push the broken container event with UEID=%s", str_ueid);
		if (event_error)
			g_error_free(event_error);
		return FALSE;
	}
	if (!event_error) {
		GSETERROR(error,"No status received for the event with UEID=%s", str_ueid);
		return FALSE;
	}
	g_error_free(event_error);
	DEBUG("Content marked broken, with UEID=[%s]", str_ueid);
	return TRUE;
}

gint
gcluster_v2_push_broken_content(struct metacnx_ctx_s *cnx, const gchar *ns_name, const container_id_t cid, const gchar *path, GError **error)
{
	GError *event_error = NULL;
	gchar str_ueid[256];
	gboolean rc;
	gridcluster_event_t *event;

	event = gridcluster_create_event();
	gridcluster_event_set_type(event, "broken.CONTAINER");
	gridcluster_event_add_string(event,"NAMESPACE", ns_name);
	gridcluster_event_add_buffer(event,"CONTAINER", cid, sizeof(container_id_t));
	gridcluster_event_add_string(event,"CONTENT", path);
	g_snprintf(str_ueid, sizeof(str_ueid), "%ld_%d_%p", random(), getpid(), &str_ueid);
	rc = gridcluster_push_event(cnx, str_ueid, event, &event_error, error);
	gridcluster_destroy_event(event);
	if (!rc) {
		GSETERROR(error,"Failed to push the broken container event with UEID=%s", str_ueid);
		if (event_error)
			g_error_free(event_error);
		return FALSE;
	}
	if (!event_error) {
		GSETERROR(error,"No status received for the event with UEID=%s", str_ueid);
		return FALSE;
	}
	g_error_free(event_error);
	DEBUG("Content marked broken, with UEID=[%s]", str_ueid);
	return TRUE;
}

gint
gcluster_fix_broken_container(addr_info_t * addr, long timeout, GSList * container_list, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, NULL, NULL},
		{0, 0, NULL, NULL}
	};
	struct reply_sequence_data_s data = { NULL, 0, codes };

	GByteArray *buf = meta2_maintenance_names_marshall(container_list, error);
	if (!buf) {
		GSETERROR(error, "Failed to marshall container list");
		return 0;
	}

	MESSAGE req = build_request(NAME_MSGNAME_CS_FIX_BROKEN_CONT, buf->data, buf->len);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses",
				NAME_MSGNAME_CS_FIX_BROKEN_CONT);
		goto error_reply;
	}

	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (1);

error_reply:
	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (0);
}

gint
gcluster_rm_broken_container(addr_info_t * addr, long timeout, GSList * container_list, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, NULL, NULL},
		{0, 0, NULL, NULL}
	};
	struct reply_sequence_data_s data = { NULL, 0, codes };

	GByteArray *buf = meta2_maintenance_names_marshall(container_list, error);
	if (!buf) {
		GSETERROR(error, "Failed to marshall container list");
		return 0;
	}

	MESSAGE req = build_request(NAME_MSGNAME_CS_RM_BROKEN_CONT, buf->data, buf->len);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses",
				NAME_MSGNAME_CS_RM_BROKEN_CONT);
		goto error_reply;
	}

	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (1);

error_reply:
	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (0);
}

GSList *
gcluster_get_broken_container(addr_info_t * addr, long timeout, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_PARTIAL_CONTENT, REPSEQ_BODYMANDATORY, &container_list_content_handler, NULL},
		{CODE_FINAL_OK, REPSEQ_FINAL, &container_list_content_handler, NULL},
		{0, 0, NULL, NULL}
	};
	GSList *containers = NULL;
	struct reply_sequence_data_s data = { &containers, 0, codes };

	MESSAGE req = build_request(NAME_MSGNAME_CS_GET_BROKEN_CONT, NULL, 0);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses",
				NAME_MSGNAME_CS_GET_BROKEN_CONT);
		goto error_reply;
	}

	message_destroy(req);
	return (containers);

error_reply:
	message_destroy(req);
	return (NULL);
}

static GSList *
_get_services(struct metacnx_ctx_s *ctx, const gchar * type,
		GError ** error, gboolean full)
{
	GByteArray *gba0 = NULL, *gba1 = NULL;
	GSList *result;

	gba0 = metautils_gba_from_string(type);
	if (full)
		gba1 = metautils_gba_from_string("1");
	result = service_info_sequence_request(ctx, error,
			NAME_MSGNAME_CS_GET_SRV, NULL, "TYPENAME", gba0,
			"FULL", gba1,
			NULL);

	g_byte_array_free(gba0, TRUE);
	if (gba1)
		g_byte_array_free(gba1, TRUE);

	return result;
}

GSList *
gcluster_get_services_from_ctx(struct metacnx_ctx_s *ctx, const gchar * type,
		GError ** error)
{
	return _get_services(ctx, type, error, FALSE);
}

GSList *
gcluster_get_services_full(struct metacnx_ctx_s *ctx, const gchar * type,
		GError ** error)
{
	return _get_services(ctx, type, error, TRUE);
}

GSList *
gcluster_get_services2(addr_info_t * addr, long to_cnx, long to_req,
		const gchar * type, GError ** error)
{
	GSList *result;
	struct metacnx_ctx_s cnx_ctx;

	(void) type;
	metacnx_clear(&cnx_ctx);
	memcpy(&(cnx_ctx.addr), addr, sizeof(addr_info_t));
	cnx_ctx.timeout.cnx = to_cnx;
	cnx_ctx.timeout.req = to_req;

	result = _get_services(&cnx_ctx, type, error, FALSE);
	metacnx_close(&cnx_ctx);
	return result;
}

GSList *
gcluster_get_services(addr_info_t * addr, long timeout, const gchar * type, GError ** error)
{
	return gcluster_get_services2(addr, timeout, timeout, type, error);
}

GSList *
gcluster_get_service_types(addr_info_t *addr, long timeout, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_PARTIAL_CONTENT, REPSEQ_BODYMANDATORY, &container_list_content_handler, NULL},
		{CODE_FINAL_OK, REPSEQ_FINAL, &container_list_content_handler, NULL},
		{0, 0, NULL, NULL}};
	GSList *srvtypes = NULL;
	struct reply_sequence_data_s data = { &srvtypes, 0, codes };

	MESSAGE req = build_request(NAME_MSGNAME_CS_GET_SRVNAMES, NULL, 0);
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses", NAME_MSGNAME_CS_GET_SRVNAMES);
		goto error_reply;
	}

	message_destroy(req);
	return (srvtypes);

error_reply:
	message_destroy(req);
	return (NULL);
}

gint
gcluster_push_services(addr_info_t * addr, long timeout, GSList * services_list, gboolean lock_action, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, NULL, NULL},
		{0, 0, NULL, NULL}
	};
	struct reply_sequence_data_s data = { NULL, 0, codes };

	GByteArray *buf = service_info_marshall_gba(services_list, error);
	if (!buf) {
		GSETERROR(error, "Failed to marshall services list");
		return 0;
	}

	MESSAGE req = build_request(NAME_MSGNAME_CS_PUSH_SRV, buf->data, buf->len);
	if (lock_action)
		message_add_field(req, "LOCK", "true", sizeof("true") - 1);

	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, timeout, &data)) {
		GSETERROR(error, "Cannot execute the query %s and receive all the responses", NAME_MSGNAME_CS_PUSH_SRV);
		goto error_reply;
	}

	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (1);

error_reply:
	message_destroy(req);
	g_byte_array_free(buf, TRUE);
	return (0);
}

static gboolean
body_to_gba_handler(GError ** error, gpointer udata, gint code, guint8 * body, gsize body_size)
{
	GByteArray **gba_result = (GByteArray **) udata;
	(void) error, (void) code;
	if (gba_result && *gba_result)
		g_byte_array_free(*gba_result, TRUE);
	*gba_result = g_byte_array_append(g_byte_array_new(), body, body_size);
	return TRUE;
}

GByteArray *
gcluster_get_srvtype_event_config(addr_info_t * addr, long to, gchar * name, GError ** error)
{
	static struct code_handler_s codes[] = {
		{CODE_FINAL_OK, REPSEQ_FINAL, &body_to_gba_handler, NULL},
		{0, 0, NULL, NULL}
	};
	GByteArray *result = NULL;
	struct reply_sequence_data_s data = { &result, 0, codes };

	if (!addr || !name || to<0L) {
		GSETERROR(error,"Invalid parameter (%p %p)", addr, name);
		return NULL;
	}

	MESSAGE req = build_request(NAME_MSGNAME_CS_GET_EVENT_CONFIG, NULL, 0);
	message_add_field(req, "TYPENAME", name, strlen(name));

	result = g_byte_array_new();
	if (!metaXClient_reply_sequence_run_from_addrinfo(error, req, addr, to, &data)) {
		GSETERROR(error, "Cannot execute the query "NAME_MSGNAME_CS_GET_EVENT_CONFIG" and receive all the responses");
		g_byte_array_free(result, TRUE);
		message_destroy(req);
		return NULL;
	}

	message_destroy(req);
	return result;
}

