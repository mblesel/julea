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

#ifndef JULEA_ITEM_COLLECTION_H
#define JULEA_ITEM_COLLECTION_H

#if !defined(JULEA_ITEM_H) && !defined(JULEA_ITEM_COMPILATION)
#error "Only <julea-item.h> can be included directly."
#endif

#include <glib.h>

#include <julea.h>

G_BEGIN_DECLS

struct JCollection;

typedef struct JCollection JCollection;

G_END_DECLS

#include <item/jitem.h>

G_BEGIN_DECLS

JCollection* j_collection_ref(JCollection*);
void j_collection_unref(JCollection*);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(JCollection, j_collection_unref)

gchar const* j_collection_get_name(JCollection*);

JCollection* j_collection_create(gchar const*, JBatch*);
void j_collection_get(JCollection**, gchar const*, JBatch*);
void j_collection_delete(JCollection*, JBatch*);

G_END_DECLS

#endif
