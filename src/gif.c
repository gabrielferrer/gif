#include <string.h>
#include <stdlib.h>
#include "gif.h"

#define CODETABLESIZE                  4096
#define MAXBLOCKSIZE                   256
#define MAXCODEBITS                    12
#define MAXCODE                        (2 << MAXCODEBITS - 1)
#define BUFFERSIZE                     1024
#define EXTENSIONBLOCK                 0x21
#define IMAGESEPARATOR                 0x2C
#define PLAINTEXTLABEL                 0x01
#define GRAPHICCONTROLLABEL            0xF9
#define COMMENTLABEL                   0xFE
#define APPLICATIONEXTENSIONLABEL      0xFF
#define TRAILER                        0x3B

// Header.

typedef struct header_s {
	char                               signature[3];
	char                               version[3];
} header_t;

// Logical screen descriptor.

typedef struct lsd_s {
	UNSIGNED                           width;
	UNSIGNED                           height;
	GBYTE                              pkdfields;
	GBYTE                              bkidx;
	GBYTE                              par;
} lsd_t;

// GIF image descriptor. One per image in stream.

typedef struct imagedescriptor_s {
	UNSIGNED                           left;
	UNSIGNED                           top;
	UNSIGNED                           width;
	UNSIGNED                           height;
	GBYTE                              pkdfields;
} imagedescriptor_t;

// Graphic control extension.

typedef struct gce_s {
	GBYTE                              blocksize;
	GBYTE                              pkdfields;
	UNSIGNED                           delaytime;
	GBYTE                              tcidx;
	GBYTE                              blockterm;
} gce_t;

typedef struct appext_s {
	GBYTE                              blocksize;
	GBYTE                              appid[8];
	GBYTE                              authcode[3];
} appext_t;

// Encapsulates a code table.

typedef struct codetable_s {
	unsigned long                      length;
	GBYTE*                             indexes;
} codetable_t;

typedef struct read_s {
	UNSIGNED                           code;               // Current code.
	GBYTE                              bitptr;             // Bit pointer into GBYTE pointed by "byteptr".
	GBYTE                              codesize;           // Current code size.
	unsigned long                      totalcodes;         // Total codes of a given code size.
	unsigned long                      codecount;          // Codes read of a given code size.
	GBOOL                              blockterm;          // Indicate that the read_code() function read
	                                                       // the "block terminator" byte.
} read_t;

// All the information respective to the decoding process.

typedef struct decoder_s {
	read_t                             readinfo;           // Info for read_code() function.
	codetable_t*                       codetable;          // Code table.
	UNSIGNED                           clearcode;          // Clear code (CC).
	UNSIGNED                           eoicode;            // End of information (EOI) code.
	GBYTE                              mincodesize;        // Minimum code size.
	UNSIGNED                           nextcode;           // Next code to be put in the code table.
	UNSIGNED                           oldcode;            // Previous code.
	buffer_t*                          block;              // A block of encoded data (size: 0-255).
	GBYTE                              index;              // Current color index.
	buffer_t*                          indexes;            // Set of color indexes.
} decoder_t;

// Read and move pointer functions over the GIF data stream.

MS G_Read;
MSP G_Move;

void GIF_FreeImages (image_t* image) {
	if (image) {
		if (image->lct) {
			free (image->lct);
		}

		if (image->indexes) {
			B_FreeBuffer (image->indexes);
		}

		GIF_FreeImages (image->next);
		free (image);
	}
}

void GIF_FreeGif (gif_t* gif) {
	if (gif->gct) {
		free (gif->gct);
	}

	if (gif->images) {
		GIF_FreeImages (gif->images);
	}
}

GBYTE GIF_Min (GBYTE a, GBYTE b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

long GIF_Max (long a, long b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

void GIF_FlushTable (codetable_t* codetable, UNSIGNED fixedcodes) {
	UNSIGNED i;

	for (i = fixedcodes; i < CODETABLESIZE; i++) {
		if (codetable[i].indexes) {
			free (codetable[i].indexes);
			codetable[i].length = 0;
			codetable[i].indexes = NULL;
		} else {
			break;
		}
	}
}

// IMPORTANT: Not all the data contained in "data" must to be initialized.
// Some members MUST NOT to be initialized and MUST conserve his values.

void GIF_Init (decoder_t* data) {
	data->readinfo.code       = 0;
	data->nextcode            = data->eoicode + 1;
	data->oldcode             = 0;
	data->readinfo.codesize   = data->mincodesize + 1;
	data->readinfo.totalcodes = 1 << data->mincodesize;
	data->readinfo.codecount  = 1;
	data->index               = 0;

	// Fixed codes are: 2^mincodesize+2.
	// 2^mincodesize color indexes, CC and EOI.

	GIF_FlushTable (data->codetable, (1 << data->mincodesize) + 2);
}

GBOOL GIF_InitFixedCodes (codetable_t* codetable, UNSIGNED fixedcodes) {
	UNSIGNED i;

	for (i = 0; i < fixedcodes; i++) {
		if ((codetable[i].indexes = (GBYTE*) malloc (sizeof (GBYTE))) == NULL) {
			return GFALSE;
		}

		codetable[i].length = sizeof(GBYTE);
		codetable[i].indexes[0] = i;
	}

	return GTRUE;
}

buffer_t* GIF_CheckSize (buffer_t* indexes, unsigned long size) {
	buffer_t* b;

	if (indexes) {
		if (indexes->allocated <= size) {
			if ((b = B_NewBuffer (indexes->allocated + BUFFERSIZE)) == NULL) {
				return NULL;
			}

			B_CopyBuffer (b, indexes);
			B_FreeBuffer (indexes);

			return b;
		}

		return indexes;
	} else {
		return B_NewBuffer (size + BUFFERSIZE);
	}
}

/*
  Translates a code to a series of indexes and put them into a buffer updating
  buffer pointer.
*/

GBOOL GIF_Translate (UNSIGNED code, codetable_t* codetable, buffer_t* indexes) {

	// 0xFFF is the maximum code.

	if (code > MAXCODE) {
		return GFALSE;
	}

	// Check there is valid code.

	if (!codetable[code].indexes) {
		return GFALSE;
	}

	// Copy all indexes for that code.

	return B_CopyStreamToBuffer(indexes, codetable[code].indexes, codetable[code].length);
}

GBOOL GIF_TranslateSingle (UNSIGNED code, codetable_t* codetable, GBYTE* index) {
	if (code > MAXCODE) {
		return GFALSE;
	}

	if (!codetable[code].indexes || codetable[code].length > 1) {
		return GFALSE;
	}

	*index = codetable[code].indexes[0];

	return GTRUE;
}

GBOOL GIF_ReadCode (read_t* readinfo, buffer_t* block) {
	UNSIGNED c;
	GBYTE    cbp;
	GBYTE    w;
	GBYTE    bl;
	GBYTE    size;

	// Check if there is more codes to read with the current code size.

	if (readinfo->totalcodes == readinfo->codecount) {
		if (readinfo->codesize < MAXCODEBITS) {
			readinfo->totalcodes = 1 << readinfo->codesize;
			readinfo->codesize++;
			readinfo->codecount = 0;
		}
	}

	readinfo->code = 0;
	cbp = 0;
	bl = readinfo->codesize;

	while (cbp < readinfo->codesize) {
		// In some GBYTE we have to read the left bits from "bitptr" if this bit amount
		// is less than "codesize". If there is enough bits to read a code, we read
		// "codesize" bits.
		w = GIF_Min (8 - readinfo->bitptr, bl);
		bl = bl - w;
		// Move "bitptr" bits to the right and mask the first "w" bits from the right.
		c = ((GBYTE*) block->data)[block->index]>>readinfo->bitptr & (1 << w) - 1;
		// The bits in "c" are shifted "cbp" bits to the left. This is for
		// conserving the bits in "code". Then the bits in "code" and "c" are mixed,
		// it is the bits in "c" are appended to the bits in "code". Every chunck of
		// bits added to "code" are more significant than the bits already in "code".
		readinfo->code ^= c << cbp;
		// Update pointers.
		readinfo->bitptr += w;
		cbp += w;
		// Time to read a new GBYTE?
		if (readinfo->bitptr == 8) {
			block->index++;
			// Time to read a new block?
			if (block->index == block->size) {
				if (!G_Read(&size, sizeof (GBYTE))) {
					return GFALSE;
				}

				// If size=0 then we read the "block terminator" byte.
				if (size == 0) {
					readinfo->blockterm = GTRUE;
					return GTRUE;
				}

				block->size = 0;
				block->index = 0;

				if (!B_WriteBuffer (block, G_Read, size)) {
					return GFALSE;
				}
			}

			readinfo->bitptr = 0;
		}
	}

	// Increment code count.

	if (readinfo->codecount < readinfo->totalcodes) {
		readinfo->codecount++;
	}

	return GTRUE;
}

GBOOL GIF_AddNewCode (codetable_t* codetable, buffer_t* indexes, GBYTE index, UNSIGNED newcode) {
	if ((codetable[newcode].indexes = (GBYTE*) malloc (indexes->size + sizeof (GBYTE))) == NULL) {
		return GFALSE;
	}

	codetable[newcode].length = indexes->size + sizeof (GBYTE);
	B_CopyBufferToStream (indexes, codetable[newcode].indexes);
	codetable[newcode].indexes[codetable[newcode].length - 1] = index;

	return GTRUE;
}

GBOOL GIF_DecompressData (buffer_t* indexes) {
	decoder_t d;
	GBOOL     done;
	GBYTE     size;
	long      m;

	memset (&d, 0, sizeof (decoder_t));

	if ((d.codetable = (codetable_t*) malloc (CODETABLESIZE * sizeof (codetable_t))) == NULL) {
		return GFALSE;
	}

	memset (d.codetable, 0, CODETABLESIZE * sizeof (codetable_t));

	if ((d.block = B_NewBuffer (MAXBLOCKSIZE)) == NULL) {
		goto clean3;
	}

	if ((d.indexes = B_NewBuffer (BUFFERSIZE)) == NULL) {
		goto clean2;
	}

	// Read only once.

	if (!G_Read (&d.mincodesize, sizeof (GBYTE))) {
		goto clean;
	}

	done                  = GFALSE;

	// Initialize

	d.clearcode           = 1 << d.mincodesize;
	d.eoicode             = d.clearcode + 1;
	d.readinfo.codesize   = d.mincodesize + 1;
	d.readinfo.totalcodes = 1 << d.mincodesize;
	d.readinfo.codecount  = 0;

	// Initialize 2^mincodesize fixed codes, CC and EOI.

	if (!GIF_InitFixedCodes (d.codetable, 1 << d.mincodesize)) {
		goto clean;
	}

	// "block.size" is the size of the allocated memory.
	// "block.index" is the GBYTE where must to be stored the next GBYTE.

	if (!G_Read (&size, sizeof (GBYTE))) {
		goto clean;
	}

	if (!B_WriteBuffer (d.block, G_Read, size)) {
		goto clean;
	}

	// Read block from first GBYTE.

	d.block->index = 0;

	if (!GIF_ReadCode (&d.readinfo, d.block)) {
		goto clean;
	}

	// First code read MUST to be the clear code (CC).

	if (d.readinfo.code != d.clearcode) {
		goto clean;
	}

	GIF_Init (&d);

	if (!GIF_ReadCode (&d.readinfo, d.block)) {
		goto clean;
	}

	d.oldcode = d.readinfo.code;

	if (!GIF_Translate (d.oldcode, d.codetable, indexes)) {
		goto clean;
	}

	if (!GIF_TranslateSingle (d.oldcode, d.codetable, &d.index)) {
		goto clean;
	}

	while (!done) {
		if (!GIF_ReadCode (&d.readinfo, d.block)) {
			goto clean;
		}

		// If CC (Clear Code) is founded. Flush table and init all variables.

		if (d.readinfo.code == d.clearcode) {
			GIF_Init (&d);

			if (!GIF_ReadCode (&d.readinfo, d.block)) {
				goto clean;
			}

			d.oldcode = d.readinfo.code;

			if (!GIF_Translate (d.oldcode, d.codetable, indexes)) {
				goto clean;
			}

			if (!GIF_TranslateSingle (d.oldcode, d.codetable, &d.index)) {
				goto clean;
			}
		} else if (d.readinfo.code==d.eoicode) {

			// EOI (END OF INFORMATION) code founded. Decoding process done.

			done=GTRUE;
			break;
		} else {

			// Check if a new bigger memory block must to be allocated.

			m = GIF_Max (d.codetable[d.readinfo.code].length, d.codetable[d.oldcode].length + sizeof (GBYTE));

			if ((d.indexes = GIF_CheckSize (d.indexes, m)) == NULL) {
				goto clean;
			}

			// Overwrite existing data.

			d.indexes->size  = 0;
			d.indexes->index = 0;

			// Exists the code read in the code table?

			if (d.codetable[d.readinfo.code].indexes) {
				if (!GIF_Translate (d.readinfo.code, d.codetable, d.indexes)) {
					goto clean;
				}
			} else {
				if (!GIF_Translate (d.oldcode, d.codetable, d.indexes)) {
					goto clean;
				}

				B_CopyStreamToBuffer (d.indexes, &d.index, sizeof (GBYTE));
			}

			// Output decoded indexes.

			if (!B_AppendBuffer (indexes, d.indexes)) {
				goto clean;
			}

			d.index          = ((GBYTE*) d.indexes->data)[0];
			d.indexes->size  = 0;
			d.indexes->index = 0;

			if (!GIF_Translate (d.oldcode, d.codetable, d.indexes)) {
				goto clean;
			}

			// Add a new code to the code table.

			if (!GIF_AddNewCode (d.codetable, d.indexes, d.index, d.nextcode)) {
				goto clean;
			}

			// We added a new code, so increment the "nextcode".

			if (d.nextcode < CODETABLESIZE) {
				d.nextcode++;
			}

			// Update "oldcode".

			d.oldcode = d.readinfo.code;
		}
	}

	// Read block terminator if not read by GIF_ReadCode() function.

	if (!d.readinfo.blockterm) {
		if (!G_Read (&d.index, sizeof (GBYTE))) {
			goto clean;
		}

		// Block terminator must be zero.

		if (d.index != 0) {
			goto clean;
		}
	}

	GIF_FlushTable (d.codetable, 0);
	free (d.codetable);
	B_FreeBuffer (d.block);
	B_FreeBuffer (d.indexes);

	return GTRUE;

clean:
	B_FreeBuffer (d.indexes);
clean2:
	B_FreeBuffer (d.block);
clean3:
	GIF_FlushTable (d.codetable, 0);
	free (d.codetable);

	return GFALSE;
}

GBOOL GIF_ReadGraphicControlBlock (GBOOL* gceread, gce_t* gce) {
	if (*gceread) {
		return GFALSE;
	}

	// Only one graphic control block per graphic rendering block.

	if (!G_Read(gce, sizeof (gce_t))) {
		return GFALSE;
	}

	// Block size must to be four and block terminator must to be zero.

	if (!(gce->blocksize == 4 && gce->blockterm == 0)) {
		return GFALSE;
	}

	// Graphic control extension read.

	*gceread = GTRUE;

	return GTRUE;
}

GBOOL GIF_ReadCommentBlock (comment_t** comments) {
	unsigned long i, commentsize;
	long          totalbytes;
	GBYTE         c;
	comment_t*    comment;

	if (!G_Read (&c, sizeof (GBYTE))) {
		return GFALSE;
	}

	commentsize = 0;
	totalbytes = 0;

	while (c != 0) {
		commentsize += c;
		totalbytes  += c + sizeof (GBYTE);

		// Move stream pointer to the next comment block.

		if (!G_Move (c)) {
			return GFALSE;
		}

		// Read block size.

		if (!G_Read (&c, sizeof (GBYTE))) {
			return GFALSE;
		}
	}

	totalbytes++;

	if (commentsize > 0) {
		if ((comment = (comment_t*) malloc (sizeof (comment_t))) == NULL) {
			return GFALSE;
		}

		comment->next = NULL;

		if ((comment->comment = (char*) malloc (commentsize + 1)) == NULL) {
			goto clean2;
		}

		// Move the stream pointer to the starting comment block.

		if (!G_Move (-totalbytes)) {
			goto clean;
		}

		if (!G_Read (&c, sizeof (GBYTE))) {
			goto clean;
		}

		// Read the comment blocks and copy it to the new comment.

		i = 0;

		while (c != 0) {
			if (!G_Read (comment->comment + i, c)) {
				goto clean;
			}

			i += c;

			if (!G_Read (&c, sizeof (GBYTE))) {
				goto clean;
			}
		}

		// End of comment.

		*(comment->comment + i) = '\0';

		if (*comments) {
			comment->next = *comments;
		}

		*comments = comment;
	}

	return GTRUE;

clean:
	free (comment->comment);
clean2:
	free (comment);

	return GFALSE;
}

GBOOL GIF_ReadApplicationBlock (app_t** apps) {
	appext_t aext;
	long     totalbytes;
	GBYTE    c, n;
	GBOOL    done;
	app_t*   app;

	if(!G_Read (&aext, sizeof (appext_t))) {
		return GFALSE;
	}

	// Block size must to be 11.

	if (aext.blocksize != APPLICATIONIDSIZE + APPLICATIONAUTHCODESIZE) {
		return GFALSE;
	}

	totalbytes = 0;
	done       = GFALSE;

	while (!done) {
		if (!G_Read (&c, sizeof (GBYTE))) {
			return GFALSE;
		}

		if (c == 0) {
			if (!G_Read (&c, sizeof (GBYTE))) {
				return GFALSE;
			}

			if (c == TRAILER || c == EXTENSIONBLOCK) {

				// Go back "totalbytes" + two last bytes read.

				if (!G_Move (-totalbytes - 2)) {
					return GFALSE;
				}

				done = GTRUE;
			} else {

				// There was no trailer or extension block, so go back one byte.

				if (!G_Move (-1)) {
					return GFALSE;
				}

				// Count the last zero read as a application byte.

				totalbytes++;
			}
		} else {
			totalbytes++;
		}
	}

	if (totalbytes > 0) {
		if ((app = (app_t*) malloc (sizeof (app_t))) == NULL) {
			return GFALSE;
		}

		app->next = NULL;

		if ((app->data = (GBYTE*) malloc (totalbytes)) == NULL) {
			goto clean2;
		}

		strncpy (app->appid, aext.appid, APPLICATIONIDSIZE);
		strncpy (app->authcode, aext.authcode, APPLICATIONAUTHCODESIZE);

		if (!G_Read (app->data, totalbytes)) {
			goto clean;
		}

		// Skip block terminator.

		if (!G_Move (sizeof (GBYTE))) {
			goto clean;
		}

		if (*apps) {
			app->next = *apps;
		}

		*apps = app;
	}

	return GTRUE;

clean:
	free (app->data);
clean2:
	free (app);

	return GFALSE;
}

GBOOL GIF_ProcessStream (gif_t** gif, MS r, MSP mp) {
	header_t          header;
	lsd_t             lsd;
	imagedescriptor_t id;
	gce_t             gce;
	size_t            items;
	gif_t*            agif;
	image_t*          i;
	image_t*          p;
	GBYTE             c;
	GBOOL             done;
	GBOOL             gceread;

	if (r == NULL || mp == NULL) {
		goto clean;
	}

	G_Read = r;
	G_Move = mp;

	if ((agif = (gif_t*) malloc (sizeof (gif_t))) == NULL) {
		goto clean;
	}

	memset (agif, 0, sizeof (gif_t));

	if (!G_Read (&header, sizeof (header_t))) {
		goto clean;
	}

	if (strncmp (header.signature, "GIF", 3) != 0) {
		goto clean;
	}

	if (strncmp (header.version, "87a", 3) && strncmp (header.version, "89a", 3)) {
		goto clean;
	}

	if (!G_Read (&lsd, sizeof (lsd_t))) {
		goto clean;
	}

	agif->screenwidth  = lsd.width;
	agif->screenheight = lsd.height;
	agif->background   = (lsd.pkdfields & 0x80)? GTRUE : GFALSE;
	agif->bkgindex     = lsd.bkidx;

	// TODO: calculate the aspect ratio if "lsd.par!=0".

	agif->aspectratio  = lsd.par;

	// Check Global Color Table existence.

	if (lsd.pkdfields & 0x80) {
		items = 2 << (lsd.pkdfields & 0x07);

		if ((agif->gct = (rgb_t*) malloc (sizeof (rgb_t) * items)) == NULL) {
			goto clean;
		}

		if (!G_Read (agif->gct, sizeof (rgb_t) * items)) {
			goto clean;
		}
	}

	done    = GFALSE;
	gceread = GFALSE;

	while (!done) {
		if (!G_Read (&c, sizeof (GBYTE))) {
			goto clean;
		}

		switch (c) {

			// Extension block.

			case EXTENSIONBLOCK:
				if (!G_Read (&c, sizeof (GBYTE))) {
					goto clean;
				}

				switch (c) {

					// Plain text label.

					case PLAINTEXTLABEL:
						break;

					// Graphic control label.

					case GRAPHICCONTROLLABEL:
						if (!GIF_ReadGraphicControlBlock (&gceread, &gce)) {
							goto clean;
						}

						break;

					// Comment label.

					case COMMENTLABEL:
						if (!GIF_ReadCommentBlock (&agif->comments)) {
							goto clean;
						}

						break;

					// Application extension label.

					case APPLICATIONEXTENSIONLABEL:
						if (!GIF_ReadApplicationBlock (&agif->apps)) {
							goto clean;
						}

						break;

					default:
						goto clean;
				};

				break;

			// Image separator.

			case IMAGESEPARATOR:
				if (!G_Read (&id, sizeof (imagedescriptor_t))) {
					goto clean;
				}

				if ((i = (image_t*) malloc (sizeof (image_t))) == NULL) {
					goto clean;
				}

				memset (i, 0, sizeof (image_t));

				i->left   = id.left;
				i->top    = id.top;
				i->width  = id.width;
				i->height = id.height;

				if (gceread) {
					i->delaytime = gce.delaytime;

					if (gce.pkdfields & 0x01) {
						i->transparent = GTRUE;
						i->trnspindex = gce.tcidx;
					}

					// Mark as processed.

					gceread = GFALSE;
				}

				i->interlaced = (id.pkdfields & 0x40) ? GTRUE : GFALSE;
				i->sorted     = (id.pkdfields & 0x20) ? GTRUE : GFALSE;

				// Check Local Color Table existence.

				if (id.pkdfields & 0x80) {
					items = 2 << (id.pkdfields & 0x07);

					if ((i->lct = (rgb_t*) malloc (sizeof (rgb_t) * items)) == NULL) {
						goto clean;
					}

					if (!G_Read (i->lct, sizeof (rgb_t) * items)) {
						goto clean;
					}
				}

				i->next = NULL;

				// This will be filled after decompression.

				if ((i->indexes = B_NewBuffer (i->width * i->height)) == NULL) {
					goto clean;
				}

				// Decompress RGB information.

				if (!GIF_DecompressData (i->indexes)) {
					goto clean;
				}

				// Add image to linked list.

				if (agif->images) {
					p->next = i;
				} else {
					agif->images = i;
				}

				p = i;

				break;

			// Trailer.

			case TRAILER:
				done = GTRUE;

				break;

			// Any other code is an error.

			default:
				goto clean;
		}
	}

	*gif = agif;

	return GTRUE;

clean:
	GIF_FreeGif (agif);

	return GFALSE;
}

