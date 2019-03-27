#ifndef GIF_H
#define GIF_H

#include "buffer.h"

#define APPLICATIONIDSIZE              8
#define APPLICATIONAUTHCODESIZE        3

typedef struct rgb_s {
	GBYTE                              red;
	GBYTE                              green;
	GBYTE                              blue;
} rgb_t;

typedef struct comment_s {
	char*                              comment;
	struct comment_s*                  next;
} comment_t;

typedef struct app_s {
	GBYTE                              appid[APPLICATIONIDSIZE];
	GBYTE                              authcode[APPLICATIONAUTHCODESIZE];
	GBYTE*                             data;
	struct app_s*                      next;
} app_t;

typedef struct image_s {
	rgb_t*                             lct;
	buffer_t*                          indexes;
	UNSIGNED                           left;
	UNSIGNED                           top;
	UNSIGNED                           width;
	UNSIGNED                           height;
	UNSIGNED                           delaytime;
	GBOOL                              transparent;
	GBYTE                              trnspindex;
	GBOOL                              interlaced;
	GBOOL                              sorted;
	struct image_s*                    next;
} image_t;

typedef struct gif_s {
	rgb_t*                             gct;
	UNSIGNED                           screenwidth;
	UNSIGNED                           screenheight;
	GBOOL                              background;
	GBYTE                              bkgindex;
	GBYTE                              aspectratio;
	image_t*                           images;
	comment_t*                         comments;
	app_t* apps;
} gif_t;

GBOOL                                  GIF_ProcessStream (gif_t** gif, MS r, MSP mp);
void                                   GIF_FreeGif (gif_t* gif);

#endif

