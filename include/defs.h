#ifndef DEFS_H
#define DEFS_H

#define GFALSE                         0x00
#define GTRUE                          0xFF

typedef unsigned char                  GBYTE;
typedef unsigned short                 UNSIGNED;
typedef unsigned char                  GBOOL;

typedef GBOOL                          (*MS)(void*, unsigned long);
typedef GBOOL                          (*MSP)(long);

#endif

