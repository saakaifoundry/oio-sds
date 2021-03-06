/*
OpenIO SDS meta1v2
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sqlite3.h>

#include <metautils/lib/metautils.h>
#include <cluster/lib/gridcluster.h>
#include <meta0v2/meta0_remote.h>
#include <meta0v2/meta0_utils.h>
#include <sqliterepo/sqliterepo.h>

#include "./internals.h"
#include "./internals_sqlite.h"
#include "./meta1_prefixes.h"
#include "./meta1_backend.h"

#define SWAP_PTR(P0,PNEW) do { \
	if (!(P0)) { (P0) = (PNEW); (PNEW) = NULL; } \
	else { void *p = (P0); (P0) = (PNEW); (PNEW) = p; } } while (0)

struct meta1_prefixes_set_s
{
	guint8 *cache;
	GPtrArray *by_prefix;
	GMutex lock;
};

/* CACHE operations --------------------------------------------------------- */

static void
_cache_manage(guint8 *cache, const guint8 *prefix)
{
	guint16 slot = meta0_utils_bytes_to_prefix(prefix);
	cache[ slot / 8 ] |= (0x01 << (slot % 8));
}

static gboolean
_cache_is_managed(guint8 *cache, const guint8 *prefix)
{
	guint16 slot = meta0_utils_bytes_to_prefix(prefix);
	return cache[ slot / 8 ] & (0x01 << (slot % 8));
}

/* NS operations ------------------------------------------------------------ */

static guint8*
_cache_from_m0l(GSList *l, const struct addr_info_s *ai)
{
	guint8 *result;

	result = g_malloc0(8192);

	for (; l ;l=l->next) {
		struct meta0_info_s *m0info;

		if (!(m0info = l->data))
			continue;

		if (addr_info_equal(&(m0info->addr), ai)) {
			guint16 *p, *max;
			p = (guint16*) m0info->prefixes;
			max = (guint16*) (m0info->prefixes + m0info->prefixes_size);
			for (; p<max ;p++)
				_cache_manage(result, (guint8*)p);
		}
	}

	return result;
}

static GError*
_cache_load_from_m0(struct meta1_prefixes_set_s *m1ps,
		const gchar *ns_name,
		const struct addr_info_s *local_addr,
		struct addr_info_s *m0_addr,
		GArray **updated_prefixes,
		gboolean *meta0_ok)
{
	GError *err = NULL;
	GSList *m0info_list = NULL;

	EXTRA_ASSERT(m1ps != NULL);
	GRID_TRACE2("%s(%p,%s,%p,%p)", __FUNCTION__, m1ps, ns_name, local_addr,
			m0_addr);

	(void)ns_name;
	gchar m0[STRLEN_ADDRINFO];
	grid_addrinfo_to_string (m0_addr, m0, sizeof(m0));
	err = meta0_remote_get_meta1_all(m0, &m0info_list);
	if (err) {
		g_prefix_error(&err, "Remote error: ");
		return err;
	}
	if (!m0info_list) {
		GRID_DEBUG("META0 has no prefix configured!");
		return NULL;
	}

	*meta0_ok = TRUE;
	guint8 *cache = _cache_from_m0l(m0info_list, local_addr);
	GPtrArray *by_prefix = meta0_utils_list_to_array(m0info_list);

	g_mutex_lock(&m1ps->lock);
	GRID_DEBUG("Got %u prefixes from M0, %u in place",
			by_prefix->len, m1ps->by_prefix ? m1ps->by_prefix->len : 0);

	if ( m1ps->by_prefix ) {
		guint prefix;
		*updated_prefixes = g_array_new(FALSE, FALSE, sizeof(guint16));
		for( prefix=0 ; prefix <65536 ;prefix++) {
			if ( _cache_is_managed(m1ps->cache,(guint8 *)&prefix) != _cache_is_managed( cache,(guint8 *)&prefix)) {
				g_array_append_vals(*updated_prefixes, &prefix, 1);
			}
		}
	}

	SWAP_PTR(m1ps->by_prefix, by_prefix);
	SWAP_PTR(m1ps->cache, cache);
	g_mutex_unlock(&m1ps->lock);

	if (by_prefix)
		meta0_utils_array_clean(by_prefix);
	by_prefix = NULL;

	if (cache)
		g_free(cache);
	cache = NULL;

	g_slist_foreach(m0info_list, meta0_info_gclean, NULL);
	g_slist_free(m0info_list);
	return NULL;
}

static GError*
_cache_load_from_ns(struct meta1_prefixes_set_s *m1ps, const gchar *ns_name,
		const gchar *local_url, GArray **updated_prefixes, gboolean *meta0_ok)
{
	struct addr_info_s local_ai;
	GError *err = NULL;
	GSList *l, *m0_list = NULL;
	guint idx = rand();
	gboolean done = FALSE;

	EXTRA_ASSERT(m1ps != NULL);

	if (!ns_name || !local_url) {
		GRID_TRACE("META1 prefix set not configured to be reloaded from a namespace");
		return NULL;
	}

	memset(&local_ai, 0, sizeof(local_ai));
	grid_string_to_addrinfo(local_url, &local_ai);

	/* Get the META0 address */
	err = conscience_get_services (ns_name, NAME_SRVTYPE_META0, FALSE, &m0_list);
	if (err != NULL) {
		g_prefix_error(&err, "META0 locate error : ");
		return err;
	}

	if (!m0_list)
		return NEWERROR(0, "No META0 available in the namespace");;

	/* Get the prefixes list */
	guint max = g_slist_length(m0_list);
	guint nb_retry = 0;
	while (nb_retry < max) {
		struct service_info_s *si;

		l = g_slist_nth(m0_list, idx%max);
		if (!(si = l->data)) {
			nb_retry++;
			idx++;
			continue;
		}

		err = _cache_load_from_m0(m1ps, ns_name, &local_ai,
				&(si->addr), updated_prefixes, meta0_ok);
		if (!err) {
			done = TRUE;
			break;
		}

		GRID_WARN("M0 cache loading error : (%d) %s", err->code, err->message);
		if (!CODE_IS_NETWORK_ERROR(err->code))
			break;

		g_clear_error(&err);
		nb_retry++;
		idx++;
	}

	g_slist_foreach(m0_list, service_info_gclean, NULL);
	g_slist_free(m0_list);
	if (!err && !done)
		err = NEWERROR(0, "No META0 replied");
	return err;
}

/* Public API --------------------------------------------------------------- */

gboolean
meta1_prefixes_is_managed(struct meta1_prefixes_set_s *m1ps,
		const guint8 *bytes)
{
	gboolean rc = FALSE;
	if (!m1ps || !m1ps->cache || !bytes)
		return FALSE;
	g_mutex_lock(&m1ps->lock);
	rc = _cache_is_managed(m1ps->cache, bytes);
	g_mutex_unlock(&m1ps->lock);
	return rc;
}

void
meta1_prefixes_clean(struct meta1_prefixes_set_s *m1ps)
{
	if (!m1ps)
		return;
	if (m1ps->cache)
		g_free(m1ps->cache);
	if (m1ps->by_prefix)
		meta0_utils_array_clean(m1ps->by_prefix);
	g_mutex_clear(&m1ps->lock);
	memset(m1ps, 0, sizeof(*m1ps));
	g_free(m1ps);
}

GError*
meta1_prefixes_manage_all(struct meta1_prefixes_set_s *m1ps,
		const gchar *local_url)
{
	gint32 i32;
	guint16 u16;

	EXTRA_ASSERT(m1ps != NULL);

	g_mutex_lock(&m1ps->lock);
	memset(m1ps->cache, ~0, 8192);
	if (m1ps->by_prefix)
		meta0_utils_array_clean(m1ps->by_prefix);
	m1ps->by_prefix = meta0_utils_array_create();
	for (i32=0; i32<65536 ;i32++) {
		u16 = i32;
		meta0_utils_array_add(m1ps->by_prefix, (guint8*)(&u16), local_url);
	}
	g_mutex_unlock(&m1ps->lock);

	return NULL;
}

GError*
meta1_prefixes_load(struct meta1_prefixes_set_s *m1ps,
		const gchar *ns_name, const gchar *local_url, GArray **updated_prefixes, gboolean *meta0_ok)
{
	GError *err = NULL;

	EXTRA_ASSERT(m1ps != NULL);
	EXTRA_ASSERT(ns_name != NULL);
	EXTRA_ASSERT(local_url != NULL);

	err = _cache_load_from_ns(m1ps, ns_name, local_url, updated_prefixes, meta0_ok);
	if (NULL != err)
		g_prefix_error(&err, "NS loading error : ");
	else
		GRID_DEBUG("Prefixes reloaded for NS[%s]", ns_name);

	return err;
}

gchar**
meta1_prefixes_get_all(struct meta1_prefixes_set_s *m1ps)
{
	EXTRA_ASSERT(m1ps != NULL);

	int i,done;
	union {
		guint16 prefix;
		guint8 b[2];
	} u;
	gchar **result = g_malloc0(sizeof(gchar*)* (65536 + 1));
	gchar name[8];

	u.prefix = 0;

	for (i=done=0; i<65536 ;i++,u.prefix++) {
		if (meta1_prefixes_is_managed(m1ps, u.b)) {
			g_snprintf(name, sizeof(name), "%02X%02X", u.b[0], u.b[1]);
			result[done] = g_strdup(name);

			done++;
		}
	}
	result[done] = NULL;

	return result;
}

guint8*
meta1_prefixes_get_cache(struct meta1_prefixes_set_s *m1ps)
{
	EXTRA_ASSERT(m1ps != NULL);

	g_mutex_lock(&m1ps->lock);
	guint8* result = g_memdup(m1ps->cache,8192);
	g_mutex_unlock(&m1ps->lock);
	return result;
}

struct meta1_prefixes_set_s *
meta1_prefixes_init(void)
{
	struct meta1_prefixes_set_s *m1ps;

	m1ps = g_malloc0(sizeof(struct meta1_prefixes_set_s));
	m1ps->cache = g_malloc0(8192);
	g_mutex_init(&m1ps->lock);
	m1ps->by_prefix = NULL;
	return m1ps;
}

gchar **
meta1_prefixes_get_peers(struct meta1_prefixes_set_s *m1ps,
		const guint8 *bytes)
{
	gchar **a = NULL;

	EXTRA_ASSERT(m1ps != NULL);

	g_mutex_lock(&m1ps->lock);
	a = meta0_utils_array_get_urlv(m1ps->by_prefix, bytes);
	g_mutex_unlock(&m1ps->lock);

	return a;
}

