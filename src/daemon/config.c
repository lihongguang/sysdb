/*
 * syscollector - src/daemon_config.c
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

#include "syscollector.h"
#include "core/plugin.h"
#include "utils/time.h"

#include "daemon/config.h"

#include "liboconfig/oconfig.h"
#include "liboconfig/utils.h"

#include <assert.h>
#include <strings.h>

/*
 * private variables
 */

static sc_time_t default_interval = 0;

/*
 * private helper functions
 */

static int
config_get_interval(oconfig_item_t *ci, sc_time_t *interval)
{
	double interval_dbl = 0.0;

	assert(ci && interval);

	if (oconfig_get_number(ci, &interval_dbl)) {
		fprintf(stderr, "config: Interval requires "
				"a single numeric argument\n"
				"\tUsage: Interval SECONDS\n");
		return -1;
	}

	if (interval_dbl <= 0.0) {
		fprintf(stderr, "config: Invalid interval: %f\n"
				"\tInterval may not be less than or equal to zero.\n",
				interval_dbl);
		return -1;
	}

	*interval = DOUBLE_TO_SC_TIME(interval_dbl);
	return 0;
} /* config_get_interval */

/*
 * token parser
 */

typedef struct {
	char *name;
	int (*dispatcher)(oconfig_item_t *);
} token_parser_t;

static int
daemon_set_interval(oconfig_item_t *ci)
{
	return config_get_interval(ci, &default_interval);
} /* daemon_set_interval */

static int
daemon_load_backend(oconfig_item_t *ci)
{
	char  plugin_name[1024];
	char *name;

	sc_plugin_ctx_t ctx = SC_PLUGIN_CTX_INIT;
	sc_plugin_ctx_t old_ctx;

	int status, i;

	ctx.interval = default_interval;

	if (oconfig_get_string(ci, &name)) {
		fprintf(stderr, "config: LoadBackend requires a single "
				"string argument\n"
				"\tUsage: LoadBackend BACKEND\n");
		return -1;
	}

	snprintf(plugin_name, sizeof(plugin_name), "backend/%s", name);

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;

		if (! strcasecmp(child->key, "Interval")) {
			if (config_get_interval(child, &ctx.interval))
				return -1;
		}
		else {
			fprintf(stderr, "config: Unknown option '%s' inside 'LoadBackend' "
					"-- see the documentation for details.\n", child->key);
			return -1;
		}
	}

	old_ctx = sc_plugin_set_ctx(ctx);
	status = sc_plugin_load(plugin_name);
	sc_plugin_set_ctx(old_ctx);
	return status;
} /* daemon_load_backend */

static int
daemon_configure_plugin(oconfig_item_t *ci)
{
	char *name;

	assert(ci);

	if (oconfig_get_string(ci, &name)) {
		fprintf(stderr, "config: %s requires a single "
				"string argument\n"
				"\tUsage: LoadBackend BACKEND\n",
				ci->key);
		return -1;
	}

	return sc_plugin_configure(name, ci);
} /* daemon_configure_backend */

static token_parser_t token_parser_list[] = {
	{ "Interval", daemon_set_interval },
	{ "LoadBackend", daemon_load_backend },
	{ "Backend", daemon_configure_plugin },
	{ "Plugin", daemon_configure_plugin },
	{ NULL, NULL },
};

/*
 * public API
 */

int
daemon_parse_config(const char *filename)
{
	oconfig_item_t *ci;
	int retval = 0, i;

	ci = oconfig_parse_file(filename);
	if (! ci)
		return -1;

	for (i = 0; i < ci->children_num; ++i) {
		oconfig_item_t *child = ci->children + i;
		int status = 1, j;

		for (j = 0; token_parser_list[j].name; ++j) {
			if (! strcasecmp(token_parser_list[j].name, child->key))
				status = token_parser_list[j].dispatcher(child);
		}

		if (status) {
			fprintf(stderr, "config: Failed to parse option '%s'\n",
					child->key);
			if (status > 0)
				fprintf(stderr, "\tUnknown option '%s' -- "
						"see the documentation for details\n",
						child->key);
			retval = -1;
		}
	}
	return retval;
} /* daemon_parse_config */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
