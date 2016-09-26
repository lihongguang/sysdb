/*
 * SysDB - src/tools/sysdb/main.c
 * Copyright (C) 2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "tools/sysdb/command.h"
#include "tools/sysdb/input.h"

#include "client/sysdb.h"
#include "client/sock.h"
#include "utils/error.h"
#include "utils/llist.h"
#include "utils/strbuf.h"
#include "utils/os.h"
#include "utils/ssl.h"

#include <errno.h>
#include <time.h>

#if HAVE_LIBGEN_H
#	include <libgen.h>
#else /* HAVE_LIBGEN_H */
#	define basename(path) (path)
#endif /* ! HAVE_LIBGEN_H */

#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>

#include <pwd.h>

#if HAVE_EDITLINE_READLINE_H
#	include <editline/readline.h>
#	if HAVE_EDITLINE_HISTORY_H
#		include <editline/history.h>
#	endif
#elif HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#	if HAVE_READLINE_HISTORY_H
#		include <readline/history.h>
#	endif
#elif HAVE_READLINE_H
#	include <readline.h>
#	if HAVE_HISTORY_H
#		include <history.h>
#	endif
#endif /* READLINEs */

#ifndef DEFAULT_SOCKET
#	define DEFAULT_SOCKET "unix:"LOCALSTATEDIR"/run/sysdbd.sock"
#endif

static sdb_ssl_options_t ssl_options = {
	/* ca_file */   SDB_SSL_CAFILE,
	/* key_file */  "~/.config/sysdb/ssl/key.pem",
	/* cert_file */ "~/.config/sysdb/ssl/cert.pem",
	/* crl_file */  "~/.config/sysdb/ssl/crl.pem",
};

static void
canonicalize_ssl_options(void)
{
	char *tmp;
	if (ssl_options.ca_file) {
		tmp = sdb_realpath(ssl_options.ca_file);
		ssl_options.ca_file = tmp ? tmp : strdup(ssl_options.ca_file);
	}
	if (ssl_options.key_file) {
		tmp = sdb_realpath(ssl_options.key_file);
		ssl_options.key_file = tmp ? tmp : strdup(ssl_options.key_file);
	}
	if (ssl_options.cert_file) {
		tmp = sdb_realpath(ssl_options.cert_file);
		ssl_options.cert_file = tmp ? tmp : strdup(ssl_options.cert_file);
	}
	if (ssl_options.crl_file) {
		tmp = sdb_realpath(ssl_options.crl_file);
		ssl_options.crl_file = tmp ? tmp : strdup(ssl_options.crl_file);
	}
} /* canonicalize_ssl_options */

static void
exit_usage(char *name, int status)
{
	char *user = sdb_get_current_user();
	printf(
"Usage: %s <options>\n"

"Connection options:\n"
"  -H HOST      the host to connect to\n"
"               default: "DEFAULT_SOCKET"\n"
"  -U USER      the username to connect as\n"
"               default: %s\n"
"  -c CMD       execute the specified command and then exit\n"
"\n"
"SSL options:\n"
"  -K KEYFILE   private key file name\n"
"               default: %s\n"
"  -C CERTFILE  client certificate file name\n"
"               default: %s\n"
"  -A CAFILE    CA certificates file name\n"
"               default: %s\n"
"\n"
"General options:\n"
"\n"
"  -h           display this help and exit\n"
"  -V           display the version number and copyright\n"

"\nSysDB client "SDB_CLIENT_VERSION_STRING SDB_CLIENT_VERSION_EXTRA", "
PACKAGE_URL"\n", basename(name), user,
			ssl_options.key_file, ssl_options.cert_file, ssl_options.ca_file);

	free(user);
	exit(status);
} /* exit_usage */

static void
exit_version(void)
{
	printf("SysDB version "SDB_CLIENT_VERSION_STRING
			SDB_CLIENT_VERSION_EXTRA", built "BUILD_DATE"\n"
			"using libsysdbclient version %s%s\n"
			"Copyright (C) 2012-2014 "PACKAGE_MAINTAINER"\n"

			"\nThis is free software under the terms of the BSD license, see "
			"the source for\ncopying conditions. There is NO WARRANTY; not "
			"even for MERCHANTABILITY or\nFITNESS FOR A PARTICULAR "
			"PURPOSE.\n", sdb_client_version_string(),
			sdb_client_version_extra());
	exit(0);
} /* exit_version */

static int
execute_commands(sdb_input_t *input, sdb_llist_t *commands)
{
	sdb_llist_iter_t *iter;
	int status = 0;

	iter = sdb_llist_get_iter(commands);
	if (! iter) {
		sdb_log(SDB_LOG_ERR, "Failed to iterate commands");
		return 1;
	}

	while (sdb_llist_iter_has_next(iter)) {
		sdb_object_t *obj = sdb_llist_iter_get_next(iter);

		if (sdb_client_send(input->client, SDB_CONNECTION_QUERY,
					(uint32_t)strlen(obj->name), obj->name) <= 0) {
			sdb_log(SDB_LOG_ERR, "Failed to send command '%s' to server",
					obj->name);
			status = 1;
			break;
		}

		/* Wait for server replies. We might get any number of log messages
		 * but eventually see the reply to the query, which is either DATA or
		 * ERROR. */
		while (42) {
			status = sdb_command_print_reply(input);
			if (status < 0) {
				sdb_log(SDB_LOG_ERR, "Failed to read reply from server");
				break;
			}

			if ((status == SDB_CONNECTION_DATA)
					|| (status == SDB_CONNECTION_ERROR))
				break;
			if (status == SDB_CONNECTION_OK) {
				/* pre 0.4 versions used OK instead of DATA */
				sdb_log(SDB_LOG_WARNING, "Received unexpected OK status from "
						"server in response to a QUERY (expected DATA); "
						"assuming we're talking to an old server");
				break;
			}
		}

		if ((status != SDB_CONNECTION_OK) && (status != SDB_CONNECTION_DATA))
			break; /* error */
	}

	sdb_llist_iter_destroy(iter);
	return status;
} /* execute_commands */

int
main(int argc, char **argv)
{
	const char *host = NULL;

	char *homedir;
	char hist_file[1024] = "";

	sdb_input_t input = SDB_INPUT_INIT;
	sdb_llist_t *commands = NULL;

	while (42) {
		int opt = getopt(argc, argv, "H:U:c:C:K:A:hV");

		if (-1 == opt)
			break;

		switch (opt) {
			case 'H':
				host = optarg;
				break;
			case 'U':
				input.user = optarg;
				break;

			case 'c':
				{
					sdb_object_t *obj;

					if (! commands)
						commands = sdb_llist_create();
					if (! commands) {
						sdb_log(SDB_LOG_ERR, "Failed to create list object");
						exit(1);
					}

					if (! (obj = sdb_object_create_T(optarg, sdb_object_t))) {
						sdb_log(SDB_LOG_ERR, "Failed to create object");
						exit(1);
					}
					if (sdb_llist_append(commands, obj)) {
						sdb_log(SDB_LOG_ERR, "Failed to append command to list");
						sdb_object_deref(obj);
						exit(1);
					}
					sdb_object_deref(obj);
				}
				break;

			case 'C':
				ssl_options.cert_file = optarg;
				break;
			case 'K':
				ssl_options.key_file = optarg;
				break;
			case 'A':
				ssl_options.ca_file = optarg;
				break;

			case 'h':
				exit_usage(argv[0], 0);
				break;
			case 'V':
				exit_version();
				break;
			default:
				exit_usage(argv[0], 1);
		}
	}

	if (optind < argc)
		exit_usage(argv[0], 1);

	if (! host)
		host = DEFAULT_SOCKET;
	if (! input.user)
		input.user = sdb_get_current_user();
	else
		input.user = strdup(input.user);
	if (! input.user)
		exit(1);

	if (sdb_ssl_init())
		exit(1);

	input.client = sdb_client_create(host);
	if (! input.client) {
		sdb_log(SDB_LOG_ERR, "Failed to create client object");
		sdb_input_reset(&input);
		exit(1);
	}
	input.input = sdb_strbuf_create(2048);
	sdb_input_init(&input);

	canonicalize_ssl_options();
	if (sdb_client_set_ssl_options(input.client, &ssl_options)) {
		sdb_log(SDB_LOG_ERR, "Failed to apply SSL options");
		sdb_input_reset(&input);
		sdb_ssl_free_options(&ssl_options);
		exit(1);
	}
	sdb_ssl_free_options(&ssl_options);
	if (sdb_client_connect(input.client, input.user)) {
		sdb_log(SDB_LOG_ERR, "Failed to connect to SysDBd");
		sdb_input_reset(&input);
		exit(1);
	}

	if (commands) {
		int status;
		input.interactive = 0;
		status = execute_commands(&input, commands);
		sdb_llist_destroy(commands);
		sdb_input_reset(&input);
		if ((status != SDB_CONNECTION_OK) && (status != SDB_CONNECTION_DATA))
			exit(1);
		exit(0);
	}

	sdb_log(SDB_LOG_INFO, "SysDB client "SDB_CLIENT_VERSION_STRING
			SDB_CLIENT_VERSION_EXTRA" (libsysdbclient %s%s)",
			sdb_client_version_string(), sdb_client_version_extra());
	sdb_command_print_server_version(&input);
	printf("\n");

	using_history();

	if ((homedir = sdb_get_homedir())) {
		snprintf(hist_file, sizeof(hist_file) - 1,
				"%s/.sysdb_history", homedir);
		hist_file[sizeof(hist_file) - 1] = '\0';
		free(homedir);
		homedir = NULL;

		errno = 0;
		if (read_history(hist_file) && (errno != ENOENT)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_WARNING, "Failed to load history (%s): %s",
					hist_file, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
	}

	sdb_input_mainloop();

	sdb_client_shutdown(input.client, SHUT_WR);
	while (! sdb_client_eof(input.client)) {
		/* wait for remaining data to arrive */
		sdb_command_print_reply(&input);
	}

	if (hist_file[0] != '\0') {
		errno = 0;
		if (write_history(hist_file)) {
			char errbuf[1024];
			sdb_log(SDB_LOG_WARNING, "Failed to store history (%s): %s",
					hist_file, sdb_strerror(errno, errbuf, sizeof(errbuf)));
		}
	}

	sdb_input_reset(&input);
	sdb_ssl_shutdown();
	return 0;
} /* main */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

