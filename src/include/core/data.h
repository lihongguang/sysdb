/*
 * SysDB - src/include/core/data.h
 * Copyright (C) 2012-2014 Sebastian 'tokkee' Harl <sh@tokkee.org>
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

#ifndef SDB_CORE_DATA_H
#define SDB_CORE_DATA_H 1

#include "core/time.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include <sys/types.h>
#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SDB_TYPE_NULL = 0,
	SDB_TYPE_BOOLEAN,
	SDB_TYPE_INTEGER,
	SDB_TYPE_DECIMAL,
	SDB_TYPE_STRING,
	SDB_TYPE_DATETIME,
	SDB_TYPE_BINARY,
	SDB_TYPE_REGEX, /* extended, case-insensitive POSIX regex */

	/* flags: */
	SDB_TYPE_ARRAY = 1 << 8,
};

#define SDB_TYPE_TO_STRING(t) \
	(((t) == SDB_TYPE_NULL) ? "NULL" \
		: ((t) == SDB_TYPE_BOOLEAN) ? "BOOLEAN" \
		: ((t) == SDB_TYPE_INTEGER) ? "INTEGER" \
		: ((t) == SDB_TYPE_DECIMAL) ? "DECIMAL" \
		: ((t) == SDB_TYPE_STRING) ? "STRING" \
		: ((t) == SDB_TYPE_DATETIME) ? "DATETIME" \
		: ((t) == SDB_TYPE_BINARY) ? "BINARY" \
		: ((t) == SDB_TYPE_REGEX) ? "REGEX" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_BOOLEAN)) ? "[]BOOLEAN" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_INTEGER)) ? "[]INTEGER" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_DECIMAL)) ? "[]DECIMAL" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_STRING)) ? "[]STRING" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_DATETIME)) ? "[]DATETIME" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_BINARY)) ? "[]BINARY" \
		: ((t) == (SDB_TYPE_ARRAY | SDB_TYPE_REGEX)) ? "[]REGEX" \
		: "UNKNOWN")

union sdb_datum;
typedef union sdb_datum sdb_datum_t;

union sdb_datum {
	bool        boolean;  /* SDB_TYPE_BOOLEAN */
	int64_t     integer;  /* SDB_TYPE_INTEGER */
	double      decimal;  /* SDB_TYPE_DECIMAL */
	char       *string;   /* SDB_TYPE_STRING  */
	sdb_time_t  datetime; /* SDB_TYPE_DATETIME */
	struct {
		size_t length;
		unsigned char *datum;
	} binary;             /* SDB_TYPE_BINARY */
	struct {
		char *raw;
		regex_t regex;
	} re;                 /* SDB_TYPE_REGEX */

	struct {
		size_t length;
		void *values;
	} array;
};

/*
 * sdb_data_t:
 * An arbitrary value of a specified type.
 */
typedef struct {
	int type;  /* type of the datum */
	sdb_datum_t data;
} sdb_data_t;
#define SDB_DATA_INIT { SDB_TYPE_NULL, { .integer = 0 } }

extern const sdb_data_t SDB_DATA_NULL;

/*
 * sdb_data_copy:
 * Copy the datum stored in 'src' to the memory location pointed to by 'dst'.
 * Any dynamic data (strings, binary data) is copied to newly allocated
 * memory. Use, for example, sdb_data_free_datum() to free any dynamic memory
 * stored in a datum. On error, 'dst' is unchanged. Else, any dynamic memory
 * in 'dst' will be freed.
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_copy(sdb_data_t *dst, const sdb_data_t *src);

/*
 * sdb_data_free_datum:
 * Free any dynamic memory referenced by the specified datum. Does not free
 * the memory allocated by the sdb_data_t object itself. This function must
 * not be used if any static or stack memory is referenced from the data
 * object.
 */
void
sdb_data_free_datum(sdb_data_t *datum);

/*
 * sdb_data_cmp:
 * Compare two data points. A NULL datum is considered less than any non-NULL
 * datum. On data-type mismatch, the function always returns a non-zero value.
 *
 * Returns:
 *  - a value less than zero if d1 compares less than d2
 *  - zero if d1 compares equal to d2
 *  - a value greater than zero if d1 compares greater than d2
 */
int
sdb_data_cmp(const sdb_data_t *d1, const sdb_data_t *d2);

/*
 * sdb_data_strcmp:
 * Compare the string values of two data points. A NULL datum is considered
 * less than any non-NULL. This function works for arbitrary combination of
 * data-types.
 *
 * Returns:
 *  - a value less than zero if d1 compares less than d2
 *  - zero if d1 compares equal to d2
 *  - a value greater than zero if d1 compares greater than d2
 */
int
sdb_data_strcmp(const sdb_data_t *d1, const sdb_data_t *d2);

/*
 * sdb_data_isnull:
 * Determine whether a datum is NULL. A datum is considered to be NULL if
 * either datum is NULL or if the type is SDB_TYPE_NULL or if the string or
 * binary datum is NULL.
 */
bool
sdb_data_isnull(const sdb_data_t *datum);

/*
 * sdb_data_inarray:
 * Determine whether a datum is included in an array based on the usual
 * comparison function of the value's type. The element type of the array has
 * to match the type of the value. The value may be another array. In that
 * case, the element types have to match and the function returns true if all
 * elements of the first array are included in the second where order does not
 * matter.
 */
bool
sdb_data_inarray(const sdb_data_t *value, const sdb_data_t *array);

/*
 * sdb_data_array_get:
 * Get the i-th value stored in the specified array and store an alias in
 * 'value'. Storing an alias means that the value points to the actual array
 * element. Do *not* free the value after using it (i.e., don't use
 * sdb_data_free_datum).
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_array_get(const sdb_data_t *array, size_t i, sdb_data_t *value);

/*
 * Operators supported by sdb_data_eval_expr.
 */
enum {
	SDB_DATA_ADD = 1, /* addition */
	SDB_DATA_SUB,     /* substraction */
	SDB_DATA_MUL,     /* multiplication */
	SDB_DATA_DIV,     /* division */
	SDB_DATA_MOD,     /* modulo */
	SDB_DATA_CONCAT,  /* string / binary data concatenation */
};

#define SDB_DATA_OP_TO_STRING(op) \
	(((op) == SDB_DATA_ADD) ? "+" \
		: ((op) == SDB_DATA_SUB) ? "-" \
		: ((op) == SDB_DATA_MUL) ? "*" \
		: ((op) == SDB_DATA_DIV) ? "/" \
		: ((op) == SDB_DATA_MOD) ? "%" \
		: ((op) == SDB_DATA_CONCAT) ? "||" : "UNKNOWN")

/*
 * sdb_data_parse_op:
 * Parse the string representation of an operator supported by
 * sdb_data_expr_eval.
 *
 * Returns:
 *  - the ID of the operator
 *  - a negative value in case the operator does not exist
 */
int
sdb_data_parse_op(const char *op);

/*
 * sdb_data_expr_eval:
 * Evaluate a simple arithmetic expression on two data points. String and
 * binary data only support concatenation and all other data types only
 * support the other operators. The result may be allocated dynamically and
 * has to be freed by the caller (using sdb_data_free_datum).
 *
 * If any of the data points is a NULL value, the result is also NULL.
 *
 * The data-types of d1 and d2 have to be the same, except for the following
 * cases:
 *  - <integer> or <decimal> <mul> <datetime>
 *  - <datetime> <mul> or <div> or <mod> <integer> or <decimal>
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_expr_eval(int op, const sdb_data_t *d1, const sdb_data_t *d2,
		sdb_data_t *res);

/*
 * sdb_data_expr_type:
 * Determine the type of the expression when applying the specified operator
 * to the specified types. Note that if an actual value is a typed NULL value
 * (e.g. a NULL string value), the return value of this function does not
 * match the return type of sdb_data_expr_eval.
 *
 * See the documentation of sdb_data_expr_eval() for a description of which
 * operations are supported.
 *
 * Returns:
 *  - the type id on success
 *  - a negative value else
 */
int
sdb_data_expr_type(int op, int type1, int type2);

/*
 * sdb_data_strlen:
 * Returns a (worst-case) estimate for the number of bytes required to format
 * the datum as a string. Does not take the terminating null byte into
 * account.
 */
size_t
sdb_data_strlen(const sdb_data_t *datum);

enum {
	SDB_UNQUOTED = 0,
	SDB_SINGLE_QUOTED,
	SDB_DOUBLE_QUOTED,
};

/*
 * sdb_data_format:
 * Output the specified datum to the specified string using a default format.
 * The value of 'quoted' determines whether and how non-integer and
 * non-decimal values are quoted. If the buffer size is less than the return
 * value of sdb_data_strlen, the datum may be truncated. The buffer will
 * always be nul-terminated after calling this function.
 *
 * Returns:
 *  - the number of characters written to the buffer (excluding the terminated
 *    null byte) or the number of characters which would have been written in
 *    case the output was truncated
 */
size_t
sdb_data_format(const sdb_data_t *datum, char *buf, size_t buflen, int quoted);

/*
 * sdb_data_parse:
 * Parse the specified string into a datum using the specified type. The
 * string value is expected to be a raw value of the specified type. Integer
 * and decimal numbers may be signed or unsigned octal (base 8, if the first
 * character of the string is "0"), sedecimal (base 16, if the string includes
 * the "0x" prefix), or decimal. Decimal numbers may also be "infinity" or
 * "NaN" or may use a decimal exponent. Date-time values are expected to be
 * specified as (floating point) number of seconds since the epoch. New memory
 * will be allocated as necessary and will have to be free'd using
 * sdb_data_free_datum().
 *
 * Returns:
 *  - 0 on success
 *  - a negative value else
 */
int
sdb_data_parse(const char *str, int type, sdb_data_t *data);

/*
 * sdb_data_sizeof:
 * Return the size of the data-type identified by the specified type.
 *
 * Returns:
 *  - the size of the data-type on success
 *  - 0 else
 */
size_t
sdb_data_sizeof(int type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ! SDB_CORE_DATA_H */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */

