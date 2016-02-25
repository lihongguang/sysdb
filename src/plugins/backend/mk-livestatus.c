/*
 * SysDB - src/plugins/backend/mk-livestatus.c
 * Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_CONFIG_H
#	include "config.h"
#endif /* HAVE_CONFIG_H */

#include "sysdb.h"
#include "core/plugin.h"
#include "core/store.h"
#include "utils/error.h"
#include "utils/unixsock.h"

#include "liboconfig/utils.h"

#include <assert.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

SDB_PLUGIN_MAGIC;

/*
 * private helper functions
 */

static int
sdb_livestatus_get_host(sdb_unixsock_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data,
		sdb_object_t __attribute__((unused)) *user_data)
{
	const char *hostname;
	sdb_time_t timestamp;

	int status;

	assert(n == 2);
	assert((data[0].type == SDB_TYPE_STRING)
			&& (data[1].type == SDB_TYPE_DATETIME));

	hostname  = data[0].data.string;
	timestamp = data[1].data.datetime;

	status = sdb_plugin_store_host(hostname, timestamp);

	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to "
				"store/update host '%s'.", hostname);
		return -1;
	}
	else if (status > 0) /* value too old */
		return 0;

	sdb_log(SDB_LOG_DEBUG, "MK Livestatus backend: Added/updated "
			"host '%s' (last update timestamp = %"PRIsdbTIME").",
			hostname, timestamp);
	return 0;
} /* sdb_livestatus_get_host */

static int
sdb_livestatus_get_svc(sdb_unixsock_client_t __attribute__((unused)) *client,
		size_t n, sdb_data_t *data,
		sdb_object_t __attribute__((unused)) *user_data)
{
	const char *hostname = NULL;
	const char *svcname = NULL;
	sdb_time_t timestamp = 0;

	int status;

	assert(n == 3);
	assert((data[0].type == SDB_TYPE_STRING)
			&& (data[1].type == SDB_TYPE_STRING)
			&& (data[2].type == SDB_TYPE_DATETIME));

	hostname  = data[0].data.string;
	svcname   = data[1].data.string;
	timestamp = data[2].data.datetime;

	status = sdb_plugin_store_service(hostname, svcname, timestamp);

	if (status < 0) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to "
				"store/update service '%s / %s'.", hostname, svcname);
		return -1;
	}
	else if (status > 0) /* value too old */
		return 0;

	sdb_log(SDB_LOG_DEBUG, "MK Livestatus backend: Added/updated "
			"service '%s / %s' (last update timestamp = %"PRIsdbTIME").",
			hostname, svcname, timestamp);
	return 0;
} /* sdb_livestatus_get_svc */

/*
 * plugin API
 */

static int
sdb_livestatus_init(sdb_object_t *user_data)
{
	sdb_unixsock_client_t *client;

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;
	if (sdb_unixsock_client_connect(client)) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: "
				"Failed to connect to livestatus @ %s.",
				sdb_unixsock_client_path(client));
		return -1;
	}

	sdb_log(SDB_LOG_INFO, "MK Livestatus backend: Successfully "
			"connected to livestatus @ %s.",
			sdb_unixsock_client_path(client));
	return 0;
} /* sdb_livestatus_init */

static int
sdb_livestatus_shutdown(sdb_object_t *user_data)
{
	if (! user_data)
		return -1;

	sdb_unixsock_client_destroy(SDB_OBJ_WRAPPER(user_data)->data);
	SDB_OBJ_WRAPPER(user_data)->data = NULL;
	return 0;
} /* sdb_livestatus_shutdown */

static int
sdb_livestatus_collect(sdb_object_t *user_data)
{
	sdb_unixsock_client_t *client;

	int status;

	if (! user_data)
		return -1;

	client = SDB_OBJ_WRAPPER(user_data)->data;

	status = sdb_unixsock_client_send(client, "GET hosts\r\n"
			"Columns: name last_check");
	if (status <= 0) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to send "
				"'GET hosts' command to livestatus @ %s.",
				sdb_unixsock_client_path(client));
		return -1;
	}

	sdb_unixsock_client_shutdown(client, SHUT_WR);

	if (sdb_unixsock_client_process_lines(client, sdb_livestatus_get_host,
				/* user data */ NULL, /* -> EOF */ -1, /* delim */ ";",
				/* column count */ 2, SDB_TYPE_STRING, SDB_TYPE_DATETIME)) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to read "
				"response from livestatus @ %s while reading hosts.",
				sdb_unixsock_client_path(client));
		return -1;
	}

	if ((! sdb_unixsock_client_eof(client))
			|| sdb_unixsock_client_error(client)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to read "
				"host from livestatus @ %s: %s",
				sdb_unixsock_client_path(client),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	status = sdb_unixsock_client_send(client, "GET services\r\n"
			"Columns: host_name description last_check");
	if (status <= 0) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to send "
				"'GET services' command to livestatus @ %s.",
				sdb_unixsock_client_path(client));
		return -1;
	}

	sdb_unixsock_client_shutdown(client, SHUT_WR);

	if (sdb_unixsock_client_process_lines(client, sdb_livestatus_get_svc,
				/* user data */ NULL, /* -> EOF */ -1, /* delim */ ";",
				/* column count */ 3, SDB_TYPE_STRING, SDB_TYPE_STRING,
				SDB_TYPE_DATETIME)) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to read "
				"response from livestatus @ %s while reading services.",
				sdb_unixsock_client_path(client));
		return -1;
	}

	if ((! sdb_unixsock_client_eof(client))
			|| sdb_unixsock_client_error(client)) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to read "
				"services from livestatus @ %s: %s",
				sdb_unixsock_client_path(client),
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}
	return 0;
} /* sdb_livestatus_collect */

static int
sdb_livestatus_config_instance(oconfig_item_t *ci)
{
	char *name = NULL;
	char *socket_path = NULL;

	sdb_object_t *user_data;
	sdb_unixsock_client_t *client;

	int i;

	if (oconfig_get_string(ci, &name)) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Instance requires "
				"a single string argument\n\tUsage: <Instance NAME>");
		return -1;
	}

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Socket"))
			oconfig_get_string(child, &socket_path);
		else
			sdb_log(SDB_LOG_WARNING, "MK Livestatus backend: Ignoring "
					"unknown config option '%s' inside <Instance %s>.",
					child->key, name);
	}

	if (! socket_path) {
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Instance '%s' "
				"missing the 'Socket' option.", name);
		return -1;
	}

	client = sdb_unixsock_client_create(socket_path);
	if (! client) {
		char errbuf[1024];
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to create "
				"unixsock client: %s",
				sdb_strerror(errno, errbuf, sizeof(errbuf)));
		return -1;
	}

	user_data = sdb_object_create_wrapper("unixsock-client", client,
			(void (*)(void *))sdb_unixsock_client_destroy);
	if (! user_data) {
		sdb_unixsock_client_destroy(client);
		sdb_log(SDB_LOG_ERR, "MK Livestatus backend: Failed to "
				"allocate sdb_object_t");
		return -1;
	}

	sdb_plugin_register_init(name, sdb_livestatus_init, user_data);
	sdb_plugin_register_shutdown(name, sdb_livestatus_shutdown, user_data);
	sdb_plugin_register_collector(name, sdb_livestatus_collect,
			/* interval */ NULL, user_data);

	/* pass control to the list */
	sdb_object_deref(user_data);
	return 0;
} /* sdb_livestatus_config_instance */

static int
sdb_livestatus_config(oconfig_item_t *ci)
{
	int i;

	if (! ci) /* nothing to do to deconfigure this plugin */
		return 0;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Instance"))
			sdb_livestatus_config_instance(child);
		else
			sdb_log(SDB_LOG_WARNING, "MK Livestatus backend: Ignoring "
					"unknown config option '%s'.", child->key);
	}
	return 0;
} /* sdb_livestatus_config */

int
sdb_module_init(sdb_plugin_info_t *info)
{
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_DESC,
			"backend accessing Nagios/Icinga/Shinken using MK Livestatus");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_COPYRIGHT,
			"Copyright (C) 2012 Sebastian 'tokkee' Harl <sh@tokkee.org>");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_LICENSE, "BSD");
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_VERSION, SDB_VERSION);
	sdb_plugin_set_info(info, SDB_PLUGIN_INFO_PLUGIN_VERSION, SDB_VERSION);

	sdb_plugin_register_config(sdb_livestatus_config);
	return 0;
} /* sdb_module_init */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

