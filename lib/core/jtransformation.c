/*
 * JULEA - Flexible storage framework
 * Copyright (C) 2010-2018 Michael Kuhn
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

#include <jtransformation.h>

#include <glib.h>

/* #ifdef HAVE_LZ4 */
#include <lz4.h>
/* #endif */

/**
 * \defgroup JTransformation Transformation
 * @{
 **/

/**
 * XOR with 1 for each bit
 */
static void
j_transformation_apply_xor(gpointer input, gpointer* output,
			   guint64* length)
{
	guint8* in;
	guint8* out;

	in = input;
	out = g_slice_alloc(*length);

	for (guint i = 0; i < *length; i++)
	{
		out[i] = in[i] ^ 255;
	}

	*output = out;
}

static void
j_transformation_apply_xor_inverse(gpointer input, gpointer* output,
				   guint64* length)
{
	j_transformation_apply_xor(input, output, length);
}

/**
 * Simple run length encoding
 */
static void
j_transformation_apply_rle(gpointer input, gpointer* output,
			   guint64* length)
{
	guint8* in;
	guint8* out;
	guint8 value, copies;
	guint64 outpos;

	in = input;

	// dry run to get the size of output buffer
	// alternative:
	// temp allocate 2*length and memcpy into final buffer with exact size
	outpos = 0;
	if (*length > 0)
	{
		copies = 0;
		value = in[0];

		for (guint64 i = 1; i < *length; i++)
		{
			if (in[i] == value && copies < 255)
			{
				copies++;
			}
			else
			{
				outpos += 2;
			}
		}
		outpos += 2;
	}

	// allocate buffer that fits to transformed data
	out = g_slice_alloc(outpos);

	// run again and store the transform
	outpos = 0;
	if (*length > 0)
	{
		copies = 0; // this means count = 1, storing a 0 makes no sense
		value = in[0];

		for (guint64 i = 1; i < *length; i++)
		{
			if (in[i] == value && copies < 255)
			{
				copies++;
			}
			else
			{
				// found a new value, store the last one
				out[outpos] = copies;
				out[outpos + 1] = value;
				outpos += 2;

				copies = 0;
				value = in[i];
			}
		}
		// write last sequence
		out[outpos] = copies;
		out[outpos + 1] = value;
		outpos += 2;
	}

	*output = out;
	*length = outpos;
}

static void
j_transformation_apply_rle_inverse(gpointer input, gpointer* output,
				   guint64* length)
{
	guint8* in;
	guint8* out;
	guint8 value;
	guint16 count;
	guint64 outpos;

	in = input;

	// dry run to get the size of output buffer
	outpos = 0;
	for (guint64 i = 1; i < *length; i += 2)
	{
		count = (guint16)in[i - 1] + 1;
		outpos += count;
	}

	// allocate buffer that fits to transformed data
	out = g_slice_alloc(outpos);

	// run again and store the transform
	outpos = 0;
	for (guint64 i = 1; i < *length; i += 2)
	{
		count = (guint16)in[i - 1] + 1; // count = copies + 1
		value = in[i];
		memset(out + outpos, value, count);
		outpos += count;
	}

	*output = out;
	*length = outpos;
}

/**
 * Use LZ4 compression with "lz4" library, https://github.com/lz4/lz4
 */
static void
j_transformation_apply_lz4(gpointer input, gpointer* output,
			   guint64* length)
{
	/* #ifdef HAVE_LZ4 */
	char* out;
	guint64 max_out_len;
	gint64 lz4_compression_result;

	// LZ4 has a function to estimate the output (upper bound)
	max_out_len = LZ4_compressBound(*length);
	out = g_slice_alloc(max_out_len);

	// Compression
	lz4_compression_result = LZ4_compress_default(input, out, *length, max_out_len);
	g_assert(lz4_compression_result > 0);

	// Copy the used part only
	*output = g_slice_alloc(lz4_compression_result);
	*length = lz4_compression_result;
	memcpy(*output, out, lz4_compression_result);

	g_slice_free1(max_out_len, out);
	/* #endif */
}

static void
j_transformation_apply_lz4_inverse(gpointer input, gpointer* output,
				   guint64* inlength, guint64* outlength)
{
	/* #ifdef HAVE_LZ4 */
	char* out;
	guint64 max_out_len;
	gint64 lz4_decompression_result;

	// TODO Estimatedmaximum size of output as good as possible
	max_out_len = *outlength;
	out = g_slice_alloc(max_out_len);

	// Decompression
	lz4_decompression_result = LZ4_decompress_safe(input, out, *inlength, max_out_len);
	g_assert(lz4_decompression_result > 0);

	// Copy the used part only
	*output = g_slice_alloc(lz4_decompression_result);
	*outlength = lz4_decompression_result;
	memcpy(*output, out, *outlength);

	g_slice_free1(max_out_len, out);
	/* #endif */
}

static gboolean
j_transformation_here(JTransformation* trafo,
		      JTransformationCaller caller)
{
	// Early exit if nothing to do
	if (trafo->type == J_TRANSFORMATION_TYPE_NONE)
		return FALSE;

	/* #ifndef HAVE_LZ4 */
	/*     // User wants lz4 but it was not found in dependencies */
	/*     if (trafo->type == J_TRANSFORMATION_TYPE_LZ4) */
	/*     { */
	/*         g_warning("Request to J_TRANSFORMATION_TYPE_LZ4 but JULEA built without lz4, no transformation applied"); */
	/*         return FALSE; */
	/*     } */
	/* #endif */

	// Decide who needs to do transform in given mode
	switch (trafo->mode)
	{
		default:
		case J_TRANSFORMATION_MODE_CLIENT:
			return caller == J_TRANSFORMATION_CALLER_CLIENT_READ || caller == J_TRANSFORMATION_CALLER_CLIENT_WRITE;
		case J_TRANSFORMATION_MODE_TRANSPORT:
			return TRUE;
		case J_TRANSFORMATION_MODE_SERVER:
			return caller == J_TRANSFORMATION_CALLER_SERVER_READ || caller == J_TRANSFORMATION_CALLER_SERVER_WRITE;
	}
	return FALSE;
}

static gboolean
j_transformation_inverse(JTransformation* trafo,
			 JTransformationCaller caller)
{
	// Decide who needs to do transform and who inverse transform
	switch (trafo->mode)
	{
		default:
		case J_TRANSFORMATION_MODE_CLIENT:
			return caller == J_TRANSFORMATION_CALLER_CLIENT_READ;
		case J_TRANSFORMATION_MODE_TRANSPORT:
			return caller == J_TRANSFORMATION_CALLER_CLIENT_READ || caller == J_TRANSFORMATION_CALLER_SERVER_WRITE;
		case J_TRANSFORMATION_MODE_SERVER:
			return caller == J_TRANSFORMATION_CALLER_SERVER_READ;
	}
	return FALSE;
}

/**
 * Get a JTransformation object from type 
 **/
JTransformation*
j_transformation_new(JTransformationType type,
		     JTransformationMode mode)
{
	JTransformation* trafo;

	trafo = g_slice_new(JTransformation);

	trafo->type = type;
	trafo->mode = mode;
	trafo->ref_count = 1;

	switch (type)
	{
		default:
		case J_TRANSFORMATION_TYPE_NONE:
			trafo->partial_access = TRUE;
			break;
		case J_TRANSFORMATION_TYPE_XOR:
			trafo->partial_access = TRUE;
			break;
		case J_TRANSFORMATION_TYPE_RLE:
			trafo->partial_access = FALSE;
			break;
		case J_TRANSFORMATION_TYPE_LZ4:
			trafo->partial_access = FALSE;
			break;
	}

	return trafo;
}

JTransformation*
j_transformation_ref(JTransformation* item)
{
	g_return_val_if_fail(item != NULL, NULL);

	g_atomic_int_inc(&(item->ref_count));

	return item;
}

void
j_transformation_unref(JTransformation* item)
{
	g_return_if_fail(item != NULL);

	if (g_atomic_int_dec_and_test(&(item->ref_count)))
	{
		g_slice_free(JTransformation, item);
	}
}

/**
 * Applies a transformation (inverse) on the data with length and offset.
 * This is done inplace (with an internal copy if necessary).
 * Does support trafo == NULL
 **/
void
j_transformation_apply(JTransformation* trafo, gpointer input,
		       guint64 inlength, guint64 inoffset, gpointer* output,
		       guint64* outlength, guint64* outoffset, JTransformationCaller caller)
{
	// Buffer for output of transformation, needs to be allocated by every method
	// because only there the size is known / estimated
	gpointer buffer;
	guint64 length;
	guint64 offset;
	gboolean inverse;

	length = inlength;
	offset = inoffset;

	// not g_return_if_fail(trafo != NULL);
	g_return_if_fail(input != NULL);
	g_return_if_fail(output != NULL);
	g_return_if_fail(outlength != NULL);
	g_return_if_fail(outoffset != NULL);

	if (trafo == NULL || !j_transformation_here(trafo, caller))
	{
		// nothing to do here, but make sure output is usable
		*output = input;
		*outlength = inlength;
		*outoffset = inoffset;
		return;
	}

	inverse = j_transformation_inverse(trafo, caller);

	switch (trafo->type)
	{
		case J_TRANSFORMATION_TYPE_NONE:
			return;
		case J_TRANSFORMATION_TYPE_XOR:
			if (inverse)
				j_transformation_apply_xor_inverse(input, &buffer, &length);
			else
				j_transformation_apply_xor(input, &buffer, &length);
			break;
		case J_TRANSFORMATION_TYPE_RLE:
			if (inverse)
				j_transformation_apply_rle_inverse(input, &buffer, &length);
			else
				j_transformation_apply_rle(input, &buffer, &length);
			break;
		case J_TRANSFORMATION_TYPE_LZ4:
			if (inverse)
				j_transformation_apply_lz4_inverse(input, &buffer, &inlength, outlength);
			else
				j_transformation_apply_lz4(input, &buffer, &length);
			break;
		default:
			return;
	}

	// when !trafo->partial_access both input and output need to be the whole
	// object, which is realized by the caller
	if (!trafo->partial_access)
	{
		offset = 0;
	}

	// output buffer is always created by the method, but for client read we have
	// user app memory as output given, we need to copy the requested part
	// and free the output buffer (cleanup does free the input buffer)
	if (caller == J_TRANSFORMATION_CALLER_CLIENT_READ && *output != NULL)
	{
		g_return_if_fail(buffer != NULL);
		// buffer can now be the whole tranformed object while output
		// only wanted a small part of it
		g_return_if_fail(length - offset + *outoffset >= *outlength);
		memcpy(*output, (gchar*)buffer - offset + *outoffset, *outlength);
		g_slice_free1(length, buffer);
	}
	else
	{
		g_return_if_fail(buffer != NULL);
		*output = buffer;
		*outlength = length;
		*outoffset = offset;
	}
}

/**
 * Cleans up after j_transformation_apply is done, frees temp buffer
 *
 * For write operations this needs to be called in write_free() with the data
 * stored in the operation struct, after the data is transfered.
 * For read operations this can be called directly after the transformation was
 * applied and the parameters must be the temp buffer prepared by
 * prep_read_buffer()
 * Does support trafo == NULL
 **/
void
j_transformation_cleanup(JTransformation* trafo, gpointer data,
			 guint64 length, guint64 offset, JTransformationCaller caller)
{
	(void)offset; // unused

	g_return_if_fail(data != NULL);

	if (trafo == NULL || !j_transformation_here(trafo, caller))
		return;

	// client read only needs a buffer if transformation can't be done inplace
	// write always needs a temp buffer to not interfer with user app memory
	// but with !partial_access aka. need_whole_object the caller is responsible
	// to cleanup once the memory block for the wohle object, this method
	// gets called per operation
	if (caller == J_TRANSFORMATION_CALLER_CLIENT_READ || J_TRANSFORMATION_CALLER_CLIENT_WRITE)
	{
		if (!trafo->partial_access)
			g_slice_free1(length, data);
	}
	//
	else if (trafo->partial_access)
	{
		g_slice_free1(length, data);
	}
}

JTransformationMode
j_transformation_get_mode(JTransformation* trafo)
{
	if (trafo == NULL)
		return J_TRANSFORMATION_MODE_CLIENT;
	else
		return trafo->mode;
}

JTransformationType
j_transformation_get_type(JTransformation* trafo)
{
	if (trafo == NULL)
		return J_TRANSFORMATION_TYPE_NONE;
	else
		return trafo->type;
}

gboolean
j_transformation_need_whole_object(JTransformation* trafo,
				   JTransformationCaller caller)
{
	if (trafo == NULL || !j_transformation_here(trafo, caller))
		return FALSE;
	else
		return !trafo->partial_access;
}

/**
 * @}
 **/
