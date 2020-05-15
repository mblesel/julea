/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2017-2018 Michael Kuhn
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

#include <julea-config.h>

#include <glib.h>

#include <string.h>

#include <bson.h>

#include <object/jchunked-transformation-object.h>
#include <object/jtransformation-object.h>
#include <object/jobject.h>

#include <julea-kv.h>

#include <julea.h>

/**
 * \defgroup JChunkedTransformationObject Object
 *
 * Data structures and functions for managing transformation objects.
 *
 * @{
 **/

struct JChunkedTransformationObjectOperation
{
	union
	{
		struct
		{
			JChunkedTransformationObject* object;
			gint64* modification_time;
			guint64* original_size;
			guint64* transformed_size;
			JTransformationType* transformation_type;
            guint64* chunk_count;
            guint64* chunk_size;
		}
		status;

		struct
		{
			JChunkedTransformationObject* object;
			gpointer data;
			guint64 length;
			guint64 offset;
			guint64* bytes_read;
		}
		read;

		struct
		{
			JChunkedTransformationObject* object;
			gconstpointer data;
			guint64 length;
			guint64 offset;
			guint64* bytes_written;
		}
		write;
	};
};

typedef struct JChunkedTransformationObjectOperation JChunkedTransformationObjectOperation;

/**
 * A JChunkedTransformationObject.
 **/
struct JChunkedTransformationObject
{
	/**
	 * The data server index.
	 */
	guint32 index;

	/**
	 * The namespace.
	 **/
	gchar* namespace;

	/**
	 * The name.
	 **/

	gchar* name;
	/**
	 * The reference count.
	 **/

	gint ref_count;

    /**
     * The Transformation
     **/
    JTransformationType transformation_type;

    /**
     * The transformation mode
     **/
    JTransformationMode transformation_mode;

    /**
     * KV Object which stores transformation metadata
     **/
    JKV* metadata;

    /**
     * The size of the object in its detransformed state
     **/
    guint64 original_size;

    /**
     * The size of the object in its transformed state
     **/
    guint64 transformed_size;

    /**
     * Number of JTransformationObjects (chunks) that hold the data
     **/
    guint64 chunk_count;

    /**
     * Maximum original data size for each chunk
     **/
    guint64 chunk_size;
};

/**
 * TODO
 **/
struct JChunkedTransformationObjectMetadata
{
    //TODO
    gint32 transformation_type;
    gint32 transformation_mode;
    guint64 original_size;
    guint64 transformed_size;
    guint64 chunk_count;
    guint64 chunk_size;
};

typedef struct JChunkedTransformationObjectMetadata JChunkedTransformationObjectMetadata;

static
void
j_chunked_transformation_object_create_free (gpointer data)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObject* object = data;

	j_chunked_transformation_object_unref(object);
}

static
void
j_chunked_transformation_object_delete_free (gpointer data)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObject* object = data;

	j_chunked_transformation_object_unref(object);
}

static
void
j_chunked_transformation_object_status_free (gpointer data)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObjectOperation* operation = data;

	j_chunked_transformation_object_unref(operation->status.object);

	g_slice_free(JChunkedTransformationObjectOperation, operation);
}

static
void
j_chunked_transformation_object_read_free (gpointer data)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObjectOperation* operation = data;

	j_chunked_transformation_object_unref(operation->read.object);

	g_slice_free(JChunkedTransformationObjectOperation, operation);
}

static
void
j_chunked_transformation_object_write_free (gpointer data)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObjectOperation* operation = data;

	j_chunked_transformation_object_unref(operation->write.object);

	g_slice_free(JChunkedTransformationObjectOperation, operation);
}

static
gboolean
j_chunked_transformation_object_store_metadata(JChunkedTransformationObject* object, JSemantics* semantics)
{
    bool ret = false;
    // TODO mdata freigeben?
    g_autoptr(JBatch) kv_batch = NULL;

    kv_batch = j_batch_new(semantics);
    gpointer mdata = malloc(sizeof(JChunkedTransformationObjectMetadata));
    ((JChunkedTransformationObjectMetadata*)mdata)->transformation_type = object->transformation_type;
    ((JChunkedTransformationObjectMetadata*)mdata)->transformation_mode = object->transformation_mode; 
    ((JChunkedTransformationObjectMetadata*)mdata)->original_size = object->original_size;
    ((JChunkedTransformationObjectMetadata*)mdata)->transformed_size = object->transformed_size;
    ((JChunkedTransformationObjectMetadata*)mdata)->chunk_count = object->chunk_count;
    ((JChunkedTransformationObjectMetadata*)mdata)->chunk_size = object->chunk_size;
    j_kv_put(object->metadata, mdata, sizeof(JChunkedTransformationObjectMetadata), g_free, kv_batch);
    ret = j_batch_execute(kv_batch);

    return ret;
}

static
gboolean
j_chunked_transformation_object_load_metadata(JChunkedTransformationObject* object)
{
    bool ret = false;

    g_autoptr(JKVIterator) kv_iter = NULL;
    kv_iter = j_kv_iterator_new(object->namespace, object->name);
    while(j_kv_iterator_next(kv_iter))
    {
        gchar const* key;
        gconstpointer value;
        guint32 len;

        key = j_kv_iterator_get(kv_iter, &value, &len);

        if(g_strcmp0(key, object->name) == 0)
        {
            JChunkedTransformationObjectMetadata* mdata = (JChunkedTransformationObjectMetadata*)value;
            object->transformation_type = mdata->transformation_type;
            object->transformation_mode = mdata->transformation_mode;
            object->original_size = mdata->original_size;
            object->transformed_size = mdata->transformed_size;
            object->chunk_count = mdata->chunk_count;
            object->chunk_size = mdata->chunk_size;
            ret = true;
        }
    }
    return ret;
}

static
gboolean
j_chunked_transformation_object_create_exec (JList* operations, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	gboolean ret = TRUE;

	g_autoptr(JListIterator) it = NULL;

	g_return_val_if_fail(operations != NULL, FALSE);
	g_return_val_if_fail(semantics != NULL, FALSE);

	it = j_list_iterator_new(operations);

    // Create the first underlying JTransformationObject and set the metadata in the kv-store
	while (j_list_iterator_next(it))
	{
		JChunkedTransformationObject* object = j_list_iterator_get(it);
        g_autoptr(JBatch) batch = NULL;
        g_autoptr(JTransformationObject) transformation_object = NULL;
        g_autofree gchar* chunk_name = NULL;
        gboolean created = FALSE;

        batch = j_batch_new(semantics);

        chunk_name = g_strdup_printf("%s_%d",object->name, 0);
        transformation_object = j_transformation_object_new(object->namespace, chunk_name);
        
        j_transformation_object_create(transformation_object, batch, object->transformation_type,
                object->transformation_mode, NULL);

        created = j_batch_execute(batch);

        // TODO add additional metadata
        if(created)
        {
            g_autoptr(JBatch) kv_batch = NULL;

            object->chunk_count = 1;

            j_chunked_transformation_object_store_metadata(object, semantics);
    
        }
	}

    return ret;

}

static
gboolean
j_chunked_transformation_object_delete_exec (JList* operations, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	gboolean ret = TRUE;

	g_autoptr(JListIterator) it = NULL;

	g_return_val_if_fail(operations != NULL, FALSE);
	g_return_val_if_fail(semantics != NULL, FALSE);

	it = j_list_iterator_new(operations);

    // Create the first underlying JTransformationObject and set the metadata in the kv-store
	while (j_list_iterator_next(it))
	{
		JChunkedTransformationObject* object = j_list_iterator_get(it);
        g_autoptr(JBatch) batch = NULL;
        g_autoptr(JBatch) kv_batch = NULL;
        g_autoptr(JTransformationObject) transformation_object = NULL;

        batch = j_batch_new(semantics);

        // TODO delete chunks
        transformation_object = j_transformation_object_new(object->namespace, object->name);
        j_transformation_object_delete(transformation_object, batch);
        j_batch_execute(batch);

        kv_batch = j_batch_new(semantics);
        j_kv_delete(object->metadata, kv_batch);
        j_batch_execute(kv_batch);
	}

    return ret;
}


static
gboolean
j_chunked_transformation_object_read_exec (JList* operations, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	gboolean ret = TRUE;

	g_autoptr(JListIterator) it = NULL;

	g_return_val_if_fail(operations != NULL, FALSE);
	g_return_val_if_fail(semantics != NULL, FALSE);

	it = j_list_iterator_new(operations);

	while (j_list_iterator_next(it))
	{
        JChunkedTransformationObjectOperation* op = j_list_iterator_get(it);
        JChunkedTransformationObject* object = op->read.object;

        // TODO only load parts?
        j_chunked_transformation_object_load_metadata(object);
        

        // TODO actual chunk handling
        g_autofree gchar* chunk_name = g_strdup_printf("%s_%d", object->name, 0);
        g_autoptr(JTransformationObject) chunk_object = j_transformation_object_new(
                object->namespace, chunk_name);
        g_autoptr(JBatch) batch = j_batch_new(semantics);

        j_transformation_object_read(chunk_object, op->read.data, op->read.length,
                op->read.offset, op->read.bytes_read, batch);

        j_batch_execute(batch);
    }

    return ret;
}

static
gboolean
j_chunked_transformation_object_write_exec (JList* operations, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	gboolean ret = TRUE;

	g_autoptr(JListIterator) it = NULL;

	g_return_val_if_fail(operations != NULL, FALSE);
	g_return_val_if_fail(semantics != NULL, FALSE);

	it = j_list_iterator_new(operations);

	while (j_list_iterator_next(it))
	{
        JChunkedTransformationObjectOperation* op = j_list_iterator_get(it);
        JChunkedTransformationObject* object = op->write.object;

        // TODO only load parts?
        j_chunked_transformation_object_load_metadata(object);
        

        // TODO actual chunk handling
        g_autofree gchar* chunk_name = g_strdup_printf("%s_%d", object->name, 0);
        g_autoptr(JTransformationObject) chunk_object = j_transformation_object_new(
                object->namespace, chunk_name);
        g_autoptr(JBatch) batch = j_batch_new(semantics);

        j_transformation_object_write(chunk_object, op->write.data, op->write.length,
                op->write.offset, op->write.bytes_written, batch);

        j_batch_execute(batch);

        // TODO only store parts?
        j_chunked_transformation_object_store_metadata(object, semantics);
    }

    return ret;
}

static
gboolean
j_chunked_transformation_object_status_exec (JList* operations, JSemantics* semantics)
{
	J_TRACE_FUNCTION(NULL);

	gboolean ret = TRUE;
	gboolean status = FALSE;

	g_autoptr(JListIterator) it = NULL;

	g_return_val_if_fail(operations != NULL, FALSE);
	g_return_val_if_fail(semantics != NULL, FALSE);

	it = j_list_iterator_new(operations);

	while (j_list_iterator_next(it))
	{
        JChunkedTransformationObjectOperation* op = j_list_iterator_get(it);
        JChunkedTransformationObject* object = op->status.object;

        // TODO only load parts?
        j_chunked_transformation_object_load_metadata(object);

        // TODO actual chunk handling
        g_autofree gchar* chunk_name = g_strdup_printf("%s_%d", object->name, 0);
        g_autoptr(JTransformationObject) chunk_object = j_transformation_object_new(
                object->namespace, chunk_name);
        g_autoptr(JBatch) batch = j_batch_new(semantics);

        j_transformation_object_status_ext(chunk_object, op->status.modification_time,
                op->status.original_size, op->status.transformed_size, 
                op->status.transformation_type, batch);

        status = j_batch_execute(batch);
        if(status)
        {
            if(op->status.chunk_count != NULL)
            {
                *(op->status.chunk_count) = object->chunk_count;
            }

            if(op->status.chunk_size != NULL)
            {
                *(op->status.chunk_size) = object->chunk_size;
            }
        }
    }

    return ret;

}




/**
 * Creates a new item.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * JChunkedTransformationObject* i;
 *
 * i = j_chunked_transformation_object_new("JULEA");
 * \endcode
 *
 * \param name         An item name.
 * \param distribution A distribution.
 *
 * \return A new item. Should be freed with j_chunked_transformation_object_unref().
 **/
JChunkedTransformationObject*
j_chunked_transformation_object_new (gchar const* namespace, gchar const* name)
{
    J_TRACE_FUNCTION(NULL);

    JConfiguration* configuration = j_configuration();
    JChunkedTransformationObject* object;

	g_return_val_if_fail(namespace != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

    object = g_slice_new(JChunkedTransformationObject);
    
	object->index = j_helper_hash(name) % j_configuration_get_server_count(configuration, J_BACKEND_TYPE_OBJECT);
	object->namespace = g_strdup(namespace);
	object->name = g_strdup(name);
	object->ref_count = 1;

    // TODO add new metadata initializations
    object->metadata = j_kv_new(namespace, name);

    return object;
}

/**
 * Creates a new item.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * JChunkedTransformationObject* i;
 *
 * i = j_chunked_transformation_object_new("JULEA");
 * \endcode
 *
 * \param name         An item name.
 * \param distribution A distribution.
 *
 * \return A new item. Should be freed with j_chunked_transformation_object_unref().
 **/
JChunkedTransformationObject*
j_chunked_transformation_object_new_for_index (guint32 index, gchar const* namespace, gchar const* name)
{
	J_TRACE_FUNCTION(NULL);

	JConfiguration* configuration = j_configuration();
	JChunkedTransformationObject* object;

	g_return_val_if_fail(namespace != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(index < j_configuration_get_server_count(configuration, J_BACKEND_TYPE_OBJECT), NULL);

	object = g_slice_new(JChunkedTransformationObject);
	object->index = index;
	object->namespace = g_strdup(namespace);
	object->name = g_strdup(name);
	object->ref_count = 1;

    // TODO add new metadata initializations
    object->metadata = j_kv_new(namespace, name);
    object->original_size = 0;
    object->transformed_size = 0;

	return object;
}

/**
 * Increases an item's reference count.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * JChunkedTransformationObject* i;
 *
 * j_chunked_transformation_object_ref(i);
 * \endcode
 *
 * \param item An item.
 *
 * \return #item.
 **/
JChunkedTransformationObject*
j_chunked_transformation_object_ref (JChunkedTransformationObject* object)
{
	J_TRACE_FUNCTION(NULL);

	g_return_val_if_fail(object != NULL, NULL);

	g_atomic_int_inc(&(object->ref_count));

	return object;
}

/**
 * Decreases an item's reference count.
 * When the reference count reaches zero, frees the memory allocated for the item.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param item An item.
 **/
void
j_chunked_transformation_object_unref (JChunkedTransformationObject* object)
{
	J_TRACE_FUNCTION(NULL);

	g_return_if_fail(object != NULL);

	if (g_atomic_int_dec_and_test(&(object->ref_count)))
	{
		g_free(object->name);
		g_free(object->namespace);

		g_slice_free(JChunkedTransformationObject, object);
	}
}

/**
 * Creates an object.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param name         A name.
 * \param distribution A distribution.
 * \param batch        A batch.
 *
 * \return A new item. Should be freed with j_chunked_transformation_object_unref().
 **/
void
j_chunked_transformation_object_create (JChunkedTransformationObject* object, JBatch* batch, JTransformationType type, JTransformationMode mode, guint64 chunk_size, void* params)
{
	J_TRACE_FUNCTION(NULL);

	JOperation* operation;

	g_return_if_fail(object != NULL);

    object->transformation_type = type;
    object->transformation_mode = mode;
    object->chunk_size = chunk_size;
    object->original_size = 0;
    object->transformed_size = 0;

	operation = j_operation_new();
	// FIXME key = index + namespace
	operation->key = object;
	operation->data = j_chunked_transformation_object_ref(object);
	operation->exec_func = j_chunked_transformation_object_create_exec;
	operation->free_func = j_chunked_transformation_object_create_free;

	j_batch_add(batch, operation);
}

/**
 * Deletes an object.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param item       An item.
 * \param batch      A batch.
 **/
void
j_chunked_transformation_object_delete (JChunkedTransformationObject* object, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	JOperation* operation;

	g_return_if_fail(object != NULL);

	operation = j_operation_new();
	operation->key = object;
	operation->data = j_chunked_transformation_object_ref(object);
	operation->exec_func = j_chunked_transformation_object_delete_exec;
	operation->free_func = j_chunked_transformation_object_delete_free;

	j_batch_add(batch, operation);
}

/**
 * Reads an item.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param object     An object.
 * \param data       A buffer to hold the read data.
 * \param length     Number of bytes to read.
 * \param offset     An offset within #object.
 * \param bytes_read Number of bytes read.
 * \param batch      A batch.
 **/
void
j_chunked_transformation_object_read (JChunkedTransformationObject* object, gpointer data, guint64 length, guint64 offset, guint64* bytes_read, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObjectOperation* iop;
	JOperation* operation;

	g_return_if_fail(object != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(length > 0);
	g_return_if_fail(bytes_read != NULL);

    iop = g_slice_new(JChunkedTransformationObjectOperation);
    iop->read.object = j_chunked_transformation_object_ref(object);
    iop->read.data = data;
    iop->read.length = length;
    iop->read.offset = offset;
    iop->read.bytes_read = bytes_read;

    operation = j_operation_new();
    operation->key = object;
    operation->data = iop;
    operation->exec_func = j_chunked_transformation_object_read_exec;
    operation->free_func = j_chunked_transformation_object_read_free;

    j_batch_add(batch, operation);

	*bytes_read = 0;
}

/**
 * Writes an item.
 *
 * \note
 * j_chunked_transformation_object_write() modifies bytes_written even if j_batch_execute() is not called.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param item          An item.
 * \param data          A buffer holding the data to write.
 * \param length        Number of bytes to write.
 * \param offset        An offset within #item.
 * \param bytes_written Number of bytes written.
 * \param batch         A batch.
 **/
void
j_chunked_transformation_object_write (JChunkedTransformationObject* object, gconstpointer data, guint64 length, guint64 offset, guint64* bytes_written, JBatch* batch)
{
    J_TRACE_FUNCTION(NULL);

    JChunkedTransformationObjectOperation* iop;
    JOperation* operation;

	g_return_if_fail(object != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(length > 0);
	g_return_if_fail(bytes_written != NULL);

    iop = g_slice_new(JChunkedTransformationObjectOperation);
    iop->write.object = j_chunked_transformation_object_ref(object);
    iop->write.data = data;
    iop->write.length = length;
    iop->write.offset = offset;
    iop->write.bytes_written = bytes_written;

    operation = j_operation_new();
    operation->key = object;
    operation->data = iop;
    operation->exec_func = j_chunked_transformation_object_write_exec;
    operation->free_func = j_chunked_transformation_object_write_free;

    j_batch_add(batch, operation);
	*bytes_written = 0;
}


/**
 * Get the status of an item.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param item      An item.
 * \param batch     A batch.
 **/
void
j_chunked_transformation_object_status (JChunkedTransformationObject* object, gint64* modification_time,
	guint64* size, JBatch* batch)
{
     j_chunked_transformation_object_status_ext(object, modification_time, size, NULL,
             NULL, NULL, NULL, batch);
}

/**
 * Get the status of an item with transformation properties.
 *
 * \author Michael Blesel, Oliver Pola
 *
 * \code
 * \endcode
 *
 * \param item      An item.
 * \param batch     A batch.
 **/
void
j_chunked_transformation_object_status_ext (JChunkedTransformationObject* object, gint64* modification_time,
	guint64* original_size, guint64* transformed_size, JTransformationType* transformation_type,
    guint64* chunk_count, guint64* chunk_size, JBatch* batch)
{
	J_TRACE_FUNCTION(NULL);

	JChunkedTransformationObjectOperation* iop;
	JOperation* operation;

	g_return_if_fail(object != NULL);

	iop = g_slice_new(JChunkedTransformationObjectOperation);
	iop->status.object = j_chunked_transformation_object_ref(object);
	iop->status.modification_time = modification_time;
	iop->status.original_size = original_size;
    iop->status.transformed_size = transformed_size;
    iop->status.transformation_type = transformation_type;
    iop->status.chunk_count = chunk_count;
    iop->status.chunk_size = chunk_size;

	operation = j_operation_new();
	operation->key = object;
	operation->data = iop;
	operation->exec_func = j_chunked_transformation_object_status_exec;
	operation->free_func = j_chunked_transformation_object_status_free;

	j_batch_add(batch, operation);
}

/**
 * @}
 **/
