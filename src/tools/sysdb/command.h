/*
 * SysDB - src/tools/sysdb/command.h
 * Copyright (C) 2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#include "tools/sysdb/input.h"

#ifndef SYSDB_COMMAND_H
#define SYSDB_COMMAND_H 1

/*
 * sdb_command_print_reply:
 * Read a reply from the server and print it to the standard output channel.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value if no reply could be read from the server
 *  - a positive value if the server returned an error
 */
int
sdb_command_print_reply(sdb_input_t *input);

/*
 * sdb_command_exec:
 * Execute the current command buffer and return the query as send to the
 * server. The query buffer points to dynamically allocated memory which has
 * to be free'd by the caller.
 *
 * The function waits for the server's reply and prints it to the standard
 * output channel.
 *
 * Returns:
 *  - the query (nul-terminated string) on success
 *  - NULL else
 */
char *
sdb_command_exec(sdb_input_t *input);

/*
 * sdb_command_print_server_version:
 * Query and print the server version.
 */
void
sdb_command_print_server_version(sdb_input_t *input);

#endif /* SYSDB_COMMAND_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

