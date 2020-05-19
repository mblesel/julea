/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2010-2020 Michael Kuhn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 **/

#ifndef JULEA_LIST_ITERATOR_H
#define JULEA_LIST_ITERATOR_H

#if !defined(JULEA_H) && !defined(JULEA_COMPILATION)
#error "Only <julea.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

struct JListIterator;

typedef struct JListIterator JListIterator;

G_END_DECLS

#include <core/jlist.h>

G_BEGIN_DECLS

JListIterator* j_list_iterator_new(JList*);
void j_list_iterator_free(JListIterator*);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(JListIterator, j_list_iterator_free)

gboolean j_list_iterator_next(JListIterator*);
gpointer j_list_iterator_get(JListIterator*);

G_END_DECLS

#endif
