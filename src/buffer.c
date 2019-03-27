#include <stdlib.h>
#include <string.h>
#include "buffer.h"

buffer_t* B_NewBuffer (unsigned long size) {
	buffer_t* b;

	if ((b = (buffer_t*) malloc (sizeof (buffer_t))) == NULL) {
		return GFALSE;
	}

	if ((b->data = malloc (size)) == NULL) {
		goto clean;
	}

	memset (b->data, 0, size);

	b->allocated = size;
	b->size = 0;
	b->index = 0;

	return b;

clean:
	free (b);

	return NULL;
}

void B_FreeBuffer (buffer_t* buffer) {
	if (buffer) {
		if (buffer->data) {
			free (buffer->data);
		}

		free (buffer);
	}
}

GBOOL B_WriteBuffer (buffer_t* buffer, MS read, unsigned long count) {
	if (buffer->size + count > buffer->allocated) {
		return GFALSE;
	}

	if (!read (buffer->data, count)) {
		return GFALSE;
	}

	buffer->size += count;

	return GTRUE;
}

void B_CopyBuffer (buffer_t* dest, buffer_t* src) {
	unsigned long i, count;

	// Avoid buffer overrun.
	if (src->size > dest->allocated) {
		count = dest->allocated;
	} else {
		count = src->size;
	}

	for (i = 0; i < count; i++) {
		((GBYTE*) dest->data)[i] = ((GBYTE*) src->data)[i];
	}

	dest->size = count;
}

GBOOL B_CopyStreamToBuffer (buffer_t* buffer, GBYTE* stream, unsigned long count) {
	unsigned long i;

	if (buffer->size + count > buffer->allocated) {
		return GFALSE;
	}

	for (i = 0; i < count; i++, buffer->index++) {
		((GBYTE*) buffer->data)[buffer->index] = stream[i];
	}

	if (buffer->size < buffer->index) {
		buffer->size = buffer->index;
	}

	return GTRUE;
}

void B_CopyBufferToStream (buffer_t* buffer, GBYTE* stream) {
	unsigned long i;

	for (i = 0; i < buffer->size; i++) {
		stream[i] = ((GBYTE*) buffer->data)[i];
	}
}

GBOOL B_AppendBuffer (buffer_t* dest, buffer_t* src) {
	unsigned long i;

	if (dest->index + src->index > dest->allocated) {
		return GFALSE;
	}

	for (i = 0; i < src->index; i++, dest->index++) {
		((GBYTE*) dest->data)[dest->index] = ((GBYTE*) src->data)[i];
	}

	if (dest->size < dest->index) {
		dest->size = dest->index;
	}

	return GTRUE;
}

void B_ClearBuffer (buffer_t* buffer) {
	memset (buffer->data, 0, buffer->allocated);
	buffer->size = 0;
	buffer->index = 0;
}

