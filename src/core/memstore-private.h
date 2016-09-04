/*
 * SysDB - src/core/memstore-private.h
 * Copyright (C) 2012-2013 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

/*
 * private data structures used by the memstore module
 */

#ifndef SDB_CORE_MEMSTORE_PRIVATE_H
#define SDB_CORE_MEMSTORE_PRIVATE_H 1

#include "core/memstore.h"
#include "core/store.h"
#include "utils/avltree.h"

#include <sys/types.h>
#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * core types
 */

struct sdb_memstore_obj {
	sdb_object_t super;
#define _name super.name

	/* object type */
	int type;

	/* common meta information */
	sdb_time_t last_update;
	sdb_time_t interval; /* moving average */
	char **backends;
	size_t backends_num;
	sdb_memstore_obj_t *parent;
};
#define STORE_OBJ(obj) ((sdb_memstore_obj_t *)(obj))
#define STORE_CONST_OBJ(obj) ((const sdb_memstore_obj_t *)(obj))

typedef struct {
	sdb_memstore_obj_t super;

	sdb_data_t value;
} attr_t;
#define ATTR(obj) ((attr_t *)(obj))
#define CONST_ATTR(obj) ((const attr_t *)(obj))

typedef struct {
	sdb_memstore_obj_t super;

	sdb_avltree_t *attributes;
} service_t;
#define SVC(obj) ((service_t *)(obj))
#define CONST_SVC(obj) ((const service_t *)(obj))

typedef struct {
	sdb_memstore_obj_t super;

	sdb_avltree_t *attributes;
	struct {
		char *type;
		char *id;
		sdb_time_t last_update;
	} store;
} metric_t;
#define METRIC(obj) ((metric_t *)(obj))

typedef struct {
	sdb_memstore_obj_t super;

	sdb_avltree_t *services;
	sdb_avltree_t *metrics;
	sdb_avltree_t *attributes;
} host_t;
#define HOST(obj) ((host_t *)(obj))
#define CONST_HOST(obj) ((const host_t *)(obj))

/* shortcuts for accessing service/host attributes */
#define _last_update super.last_update
#define _interval super.interval

/*
 * querying
 */

struct sdb_memstore_query {
	sdb_object_t super;
	sdb_ast_node_t *ast;
	sdb_memstore_matcher_t *matcher;
	sdb_memstore_matcher_t *filter;
};
#define QUERY(m) ((sdb_memstore_query_t *)(m))

/*
 * expressions
 */

enum {
	TYPED_EXPR  = -3, /* obj type stored in data.data.integer */
	ATTR_VALUE  = -2, /* attr name stored in data.data.string */
	FIELD_VALUE = -1, /* field type stored in data.data.integer */
	/*  0: const value (stored in data) */
	/* >0: operator id */
};

struct sdb_memstore_expr {
	sdb_object_t super;

	int type; /* see above */
	int data_type;

	sdb_memstore_expr_t *left;
	sdb_memstore_expr_t *right;

	sdb_data_t data;
};
#define CONST_EXPR(v) { SDB_OBJECT_INIT, 0, (v).type, NULL, NULL, (v) }
#define EXPR_TO_STRING(e) \
	(((e)->type == TYPED_EXPR) ? "<typed>" \
		: ((e)->type == ATTR_VALUE) ? "attribute" \
		: ((e)->type == FIELD_VALUE) ? SDB_FIELD_TO_NAME((e)->data.data.integer) \
		: ((e)->type == 0) ? "<constant>" \
		: ((e)->type > 0) ? SDB_DATA_OP_TO_STRING((e)->type) \
		: "<unknown>")

/*
 * matchers
 */

/* when adding to this, also update 'MATCHER_SYM' below and 'matchers' in
 * memstore_lookup.c */
enum {
	MATCHER_OR,
	MATCHER_AND,
	MATCHER_NOT,
	MATCHER_ANY,
	MATCHER_ALL,
	MATCHER_IN,

	/* unary operators */
	MATCHER_ISNULL,
	MATCHER_ISTRUE,
	MATCHER_ISFALSE,

	/* ary operators */
	MATCHER_LT,
	MATCHER_LE,
	MATCHER_EQ,
	MATCHER_NE,
	MATCHER_GE,
	MATCHER_GT,
	MATCHER_REGEX,
	MATCHER_NREGEX,

	/* a generic query */
	MATCHER_QUERY,
};

#define MATCHER_SYM(t) \
	(((t) == MATCHER_OR) ? "OR" \
		: ((t) == MATCHER_AND) ? "AND" \
		: ((t) == MATCHER_NOT) ? "NOT" \
		: ((t) == MATCHER_ANY) ? "ANY" \
		: ((t) == MATCHER_ALL) ? "ALL" \
		: ((t) == MATCHER_IN) ? "IN" \
		: ((t) == MATCHER_ISNULL) ? "IS NULL" \
		: ((t) == MATCHER_ISTRUE) ? "IS TRUE" \
		: ((t) == MATCHER_ISFALSE) ? "IS FALSE" \
		: ((t) == MATCHER_LT) ? "<" \
		: ((t) == MATCHER_LE) ? "<=" \
		: ((t) == MATCHER_EQ) ? "=" \
		: ((t) == MATCHER_NE) ? "!=" \
		: ((t) == MATCHER_GE) ? ">=" \
		: ((t) == MATCHER_GT) ? ">" \
		: ((t) == MATCHER_REGEX) ? "=~" \
		: ((t) == MATCHER_NREGEX) ? "!~" \
		: ((t) == MATCHER_QUERY) ? "QUERY" \
		: "UNKNOWN")

/* matcher base type */
struct sdb_memstore_matcher {
	sdb_object_t super;
	/* type of the matcher */
	int type;
};
#define M(m) ((sdb_memstore_matcher_t *)(m))

/* infix operator matcher */
typedef struct {
	sdb_memstore_matcher_t super;

	/* left and right hand operands */
	sdb_memstore_matcher_t *left;
	sdb_memstore_matcher_t *right;
} op_matcher_t;
#define OP_M(m) ((op_matcher_t *)(m))

/* unary operator matcher */
typedef struct {
	sdb_memstore_matcher_t super;

	/* operand */
	sdb_memstore_matcher_t *op;
} uop_matcher_t;
#define UOP_M(m) ((uop_matcher_t *)(m))

/* iter matcher */
typedef struct {
	sdb_memstore_matcher_t super;
	sdb_memstore_expr_t *iter;
	sdb_memstore_matcher_t *m;
} iter_matcher_t;
#define ITER_M(m) ((iter_matcher_t *)(m))

/* compare operator matcher */
typedef struct {
	sdb_memstore_matcher_t super;

	/* left and right hand expressions */
	sdb_memstore_expr_t *left;
	sdb_memstore_expr_t *right;
} cmp_matcher_t;
#define CMP_M(m) ((cmp_matcher_t *)(m))

typedef struct {
	sdb_memstore_matcher_t super;
	sdb_memstore_expr_t *expr;
} unary_matcher_t;
#define UNARY_M(m) ((unary_matcher_t *)(m))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_MEMSTORE_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

