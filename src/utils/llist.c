/*
 * syscollector - src/utils/llist.c
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

#include "utils/llist.h"

#include <assert.h>
#include <stdlib.h>

#include <pthread.h>

/*
 * private data types
 */

struct sc_llist_elem;
typedef struct sc_llist_elem sc_llist_elem_t;

struct sc_llist_elem {
	sc_object_t *obj;

	sc_llist_elem_t *next;
	sc_llist_elem_t *prev;
};

struct sc_llist {
	pthread_rwlock_t lock;

	sc_llist_elem_t *head;
	sc_llist_elem_t *tail;

	size_t length;
};

struct sc_llist_iter {
	sc_llist_t *list;
	sc_llist_elem_t *elem;
};

/*
 * private helper functions
 */

/* Insert a new element after 'elem'. If 'elem' is NULL, insert at the head of
 * the list. */
static int
sc_llist_insert_after(sc_llist_t *list, sc_llist_elem_t *elem,
		sc_object_t *obj)
{
	sc_llist_elem_t *new;

	assert(list);

	new = malloc(sizeof(*new));
	if (! new)
		return -1;

	new->obj  = obj;
	if (elem)
		new->next = elem->next;
	else if (list->head)
		new->next = list->head;
	else
		new->next = NULL;
	new->prev = elem;

	if (elem) {
		if (elem->next)
			elem->next->prev = new;
		else
			list->tail = new;
		elem->next = new;
	}
	else {
		/* new entry will be new head */
		if (list->head)
			list->head->prev = new;

		list->head = new;
		if (! list->tail)
			list->tail = new;
	}

	assert(list->head && list->tail);
	if (! list->length) {
		assert(list->head == list->tail);
	}

	sc_object_ref(obj);
	++list->length;
	return 0;
} /* sc_llist_insert_after */

static sc_object_t *
sc_llist_remove_elem(sc_llist_t *list, sc_llist_elem_t *elem)
{
	sc_object_t *obj;

	assert(list && elem);

	obj = elem->obj;

	if (elem->prev)
		elem->prev->next = elem->next;
	else {
		assert(elem == list->head);
		list->head = elem->next;
	}

	if (elem->next)
		elem->next->prev = elem->prev;
	else {
		assert(elem == list->tail);
		list->tail = elem->prev;
	}

	elem->prev = elem->next = NULL;
	free(elem);

	--list->length;
	return obj;
} /* sc_llist_remove_elem */

/*
 * public API
 */

sc_llist_t *
sc_llist_create(void)
{
	sc_llist_t *list;

	list = malloc(sizeof(*list));
	if (! list)
		return NULL;

	pthread_rwlock_init(&list->lock, /* attr = */ NULL);

	list->head = list->tail = NULL;
	list->length = 0;
	return list;
} /* sc_llist_create */

sc_llist_t *
sc_llist_clone(sc_llist_t *list)
{
	sc_llist_t *clone;
	sc_llist_elem_t *elem;

	if (! list)
		return NULL;

	clone = sc_llist_create();
	if (! clone)
		return NULL;

	if (! list->length) {
		assert((! list->head) && (! list->tail));
		return clone;
	}

	for (elem = list->head; elem; elem = elem->next) {
		if (sc_llist_append(clone, elem->obj)) {
			sc_llist_destroy(clone);
			return NULL;
		}
	}
	return clone;
} /* sc_llist_clone */

void
sc_llist_destroy(sc_llist_t *list)
{
	sc_llist_elem_t *elem;

	if (! list)
		return;

	pthread_rwlock_wrlock(&list->lock);

	elem = list->head;
	while (elem) {
		sc_llist_elem_t *tmp = elem->next;

		sc_object_deref(elem->obj);
		free(elem);

		elem = tmp;
	}

	list->head = list->tail = NULL;
	list->length = 0;

	pthread_rwlock_unlock(&list->lock);
	pthread_rwlock_destroy(&list->lock);
	free(list);
} /* sc_llist_destroy */

int
sc_llist_append(sc_llist_t *list, sc_object_t *obj)
{
	int status;

	if ((! list) || (! obj))
		return -1;

	pthread_rwlock_wrlock(&list->lock);
	status = sc_llist_insert_after(list, list->tail, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sc_llist_append */

int
sc_llist_insert(sc_llist_t *list, sc_object_t *obj, size_t index)
{
	sc_llist_elem_t *prev;
	sc_llist_elem_t *next;

	int status;

	size_t i;

	if ((! list) || (! obj) || (index > list->length))
		return -1;

	pthread_rwlock_wrlock(&list->lock);

	prev = NULL;
	next = list->head;

	for (i = 0; i < index; ++i) {
		prev = next;
		next = next->next;
	}
	status = sc_llist_insert_after(list, prev, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sc_llist_insert */

int
sc_llist_insert_sorted(sc_llist_t *list, sc_object_t *obj,
		int (*compare)(const sc_object_t *, const sc_object_t *))
{
	sc_llist_elem_t *prev;
	sc_llist_elem_t *next;

	int status;

	if ((! list) || (! obj) || (! compare))
		return -1;

	pthread_rwlock_wrlock(&list->lock);

	prev = NULL;
	next = list->head;

	while (next) {
		if (compare(obj, next->obj) < 0)
			break;

		prev = next;
		next = next->next;
	}
	status = sc_llist_insert_after(list, prev, obj);
	pthread_rwlock_unlock(&list->lock);
	return status;
} /* sc_llist_insert_sorted */

sc_object_t *
sc_llist_search(sc_llist_t *list, const sc_object_t *key,
		int (*compare)(const sc_object_t *, const sc_object_t *))
{
	sc_llist_elem_t *elem;

	if ((! list) || (! compare))
		return NULL;

	pthread_rwlock_rdlock(&list->lock);

	for (elem = list->head; elem; elem = elem->next)
		if (! compare(elem->obj, key))
			break;

	pthread_rwlock_unlock(&list->lock);

	if (elem)
		return elem->obj;
	return NULL;
} /* sc_llist_search */

sc_object_t *
sc_llist_shift(sc_llist_t *list)
{
	sc_object_t *obj;

	if ((! list) || (! list->head))
		return NULL;

	pthread_rwlock_wrlock(&list->lock);
	obj = sc_llist_remove_elem(list, list->head);
	pthread_rwlock_unlock(&list->lock);
	return obj;
} /* sc_llist_shift */

sc_llist_iter_t *
sc_llist_get_iter(sc_llist_t *list)
{
	sc_llist_iter_t *iter;

	if (! list)
		return NULL;

	iter = malloc(sizeof(*iter));
	if (! iter)
		return NULL;

	pthread_rwlock_rdlock(&list->lock);

	iter->list = list;
	iter->elem = list->head;

	/* XXX: keep lock until destroying the iterator? */
	pthread_rwlock_unlock(&list->lock);
	return iter;
} /* sc_llist_get_iter */

void
sc_llist_iter_destroy(sc_llist_iter_t *iter)
{
	if (! iter)
		return;

	iter->list = NULL;
	iter->elem = NULL;
	free(iter);
} /* sc_llist_iter_destroy */

_Bool
sc_llist_iter_has_next(sc_llist_iter_t *iter)
{
	if (! iter)
		return 0;
	return iter->elem != NULL;
} /* sc_llist_iter_has_next */

sc_object_t *
sc_llist_iter_get_next(sc_llist_iter_t *iter)
{
	sc_object_t *obj;

	if ((! iter) || (! iter->elem))
		return NULL;

	pthread_rwlock_rdlock(&iter->list->lock);

	obj = iter->elem->obj;
	iter->elem = iter->elem->next;

	pthread_rwlock_unlock(&iter->list->lock);
	return obj;
} /* sc_llist_iter_get_next */

/* vim: set tw=78 sw=4 ts=4 noexpandtab : */
