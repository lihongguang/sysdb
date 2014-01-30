/*
 * SysDB - t/core/data_test.c
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

#include "core/data.h"
#include "libsysdb_test.h"

#include <check.h>

START_TEST(test_data)
{
	sdb_data_t d1, d2;
	int check;

	d2.type = SDB_TYPE_INTEGER;
	d2.data.integer = 4711;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.integer == d2.data.integer,
			"sdb_data_copy() didn't copy integer data: got: %d; expected: %d",
			d1.data.integer, d2.data.integer);

	d2.type = SDB_TYPE_DECIMAL;
	d2.data.decimal = 47.11;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.decimal == d2.data.decimal,
			"sdb_data_copy() didn't copy decimal data: got: %f; expected: %f",
			d1.data.decimal, d2.data.decimal);

	d2.type = SDB_TYPE_STRING;
	d2.data.string = "some string";
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(!strcmp(d1.data.string, d2.data.string),
			"sdb_data_copy() didn't copy string data: got: %s; expected: %s",
			d1.data.string, d2.data.string);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.string == NULL,
			"sdb_data_free_datum() didn't free string data");

	d2.type = SDB_TYPE_DATETIME;
	d2.data.datetime = 4711;
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.datetime == d2.data.datetime,
			"sdb_data_copy() didn't copy datetime data: got: %d; expected: %d",
			d1.data.datetime, d2.data.datetime);

	d2.type = SDB_TYPE_BINARY;
	d2.data.binary.datum = (unsigned char *)"some string";
	d2.data.binary.length = strlen((const char *)d2.data.binary.datum);
	check = sdb_data_copy(&d1, &d2);
	fail_unless(!check, "sdb_data_copy() = %i; expected: 0", check);
	fail_unless(d1.type == d2.type,
			"sdb_data_copy() didn't copy type; got: %i; expected: %i",
			d1.type, d2.type);
	fail_unless(d1.data.binary.length == d2.data.binary.length,
			"sdb_data_copy() didn't copy length; got: %d; expected: 5d",
			d1.data.binary.length, d2.data.binary.length);
	fail_unless(!memcmp(d1.data.binary.datum, d2.data.binary.datum,
				d2.data.binary.length),
			"sdb_data_copy() didn't copy binary data: got: %s; expected: %s",
			d1.data.string, d2.data.string);

	sdb_data_free_datum(&d1);
	fail_unless(d1.data.binary.length == 0,
			"sdb_data_free_datum() didn't reset binary datum length");
	fail_unless(d1.data.binary.datum == NULL,
			"sdb_data_free_datum() didn't free binary datum");
}
END_TEST

Suite *
core_data_suite(void)
{
	Suite *s = suite_create("core::data");
	TCase *tc;

	tc = tcase_create("core");
	tcase_add_test(tc, test_data);
	suite_add_tcase(s, tc);

	return s;
} /* core_data_suite */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
