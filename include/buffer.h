#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"

typedef struct buffer_s {
	void*                              data;
	unsigned long                      allocated;
	unsigned long                      size;
	unsigned long                      index;
} buffer_t;

	buffer_t*                          B_NewBuffer (unsigned long size);
	void                               B_FreeBuffer (buffer_t* buffer);
	GBOOL                              B_WriteBuffer (buffer_t* buffer, MS read, unsigned long count);
	void                               B_CopyBuffer (buffer_t* dest, buffer_t* src);
	GBOOL                              B_CopyStreamToBuffer (buffer_t* buffer, GBYTE* stream, unsigned long count);
	void                               B_CopyBufferToStream (buffer_t* buffer, GBYTE* stream);
	GBOOL                              B_AppendBuffer (buffer_t* dest, buffer_t* src);
	void                               B_ClearBuffer (buffer_t* buffer);

#endif

