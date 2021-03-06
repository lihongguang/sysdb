/*
 * SysDB - src/include/frontend/connection.h
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

#ifndef SDB_FRONTEND_CONNECTION_H
#define SDB_FRONTEND_CONNECTION_H 1

#include "frontend/proto.h"
#include "core/store.h"
#include "utils/llist.h"
#include "utils/strbuf.h"
#include "utils/proto.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sdb_conn_t represents an open connection from a client. It inherits from
 * sdb_object_t and may safely be cast to a generic object.
 */
typedef struct sdb_conn sdb_conn_t;

/*
 * sdb_conn_node_t represents an interface for the result of parsing a command
 * string. The first field of an actual implementation of the interface is a
 * sdb_conn_state_t in order to fascilitate casting to and from the interface
 * type to the implementation type.
 */
typedef struct {
	sdb_object_t super;
	sdb_conn_state_t cmd;
} sdb_conn_node_t;
#define SDB_CONN_NODE(obj) ((sdb_conn_node_t *)(obj))

/*
 * sdb_conn_setup_cb is a callback function used to setup a connection. For
 * example, it may be used to initialize session information.
 */
typedef int (*sdb_conn_setup_cb)(sdb_conn_t *, void *);

/*
 * sdb_connection_enable_logging:
 * Enable logging of connection-related messages to the current client
 * connection. After this function has been called all log messages
 * originating from the thread handling the current client connection will
 * also be sent to the client.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_connection_enable_logging(void);

/*
 * sdb_connection_accpet:
 * Accept a new connection on the specified file-descriptor 'fd' and return a
 * newly allocated connection object. If specified, the setup callback is used
 * to setup the newly created connection. It will receive the connection
 * object and the specified user data as its arguments.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
sdb_conn_t *
sdb_connection_accept(int fd, sdb_conn_setup_cb setup, void *user_data);

/*
 * sdb_connection_close:
 * Close an open connection. Any subsequent reads from the connection will
 * fail. Use sdb_object_deref to free the memory used by the object.
 */
void
sdb_connection_close(sdb_conn_t *conn);

/*
 * sdb_connection_handle:
 * Read from an open connection until reading would block and handle all
 * incoming commands.
 *
 * Returns:
 *  - the number of bytes read (0 on EOF)
 *  - a negative value on error
 */
ssize_t
sdb_connection_handle(sdb_conn_t *conn);

/*
 * sdb_connection_send:
 * Send to an open connection.
 *
 * Returns:
 *  - the number of bytes written
 *  - a negative value on error
 */
ssize_t
sdb_connection_send(sdb_conn_t *conn, uint32_t code,
		uint32_t msg_len, const char *msg);

/*
 * sdb_connection_ping:
 * Send back a backend status indicator to the connected client.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_connection_ping(sdb_conn_t *conn);

/*
 * sdb_connection_server_version:
 * Send back the backend server version to the connected client.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_connection_server_version(sdb_conn_t *conn);

/*
 * session handling
 */

/*
 * sdb_conn_session_start:
 * Start a new user session on the specified connection.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_conn_session_start(sdb_conn_t *conn);

/*
 * store access
 */

/*
 * sdb_conn_query, sdb_conn_fetch, sdb_conn_list, sdb_conn_lookup,
 * sdb_conn_store:
 * Handle the SDB_CONNECTION_QUERY, SDB_CONNECTION_FETCH, SDB_CONNECTION_LIST,
 * SDB_CONNECTION_LOOKUP, and SDB_CONNECTION_STORE commands respectively. It
 * is expected that the current command has been initialized already.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_conn_query(sdb_conn_t *conn);
int
sdb_conn_fetch(sdb_conn_t *conn);
int
sdb_conn_list(sdb_conn_t *conn);
int
sdb_conn_lookup(sdb_conn_t *conn);
int
sdb_conn_store(sdb_conn_t *conn);

/*
 * sdb_conn_store_host, sdb_conn_store_service, sdb_conn_store_metric,
 * sdb_conn_store_attribute:
 * Execute the 'STORE' command for the respective object type.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_conn_store_host(sdb_conn_t *conn, const sdb_proto_host_t *host);
int
sdb_conn_store_service(sdb_conn_t *conn, const sdb_proto_service_t *svc);
int
sdb_conn_store_metric(sdb_conn_t *conn, const sdb_proto_metric_t *metric);
int
sdb_conn_store_attribute(sdb_conn_t *conn, const sdb_proto_attribute_t *attr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_FRONTEND_CONNECTION_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

