/*
 * libcd.h — portable stand-in for Sony's PSY-Q CD-ROM header.
 *
 * Types only. Real file loading is the port's asset layer, which reads from the
 * user's own disc image rather than PS1 CD hardware.
 */
#ifndef PORT_LIBCD_H
#define PORT_LIBCD_H

#include <dw/types.h>

/* BCD disc position: minute / second / sector / track. */
typedef struct {
    uint8_t minute, second, sector, track;
} CdlLOC;

typedef struct {
    CdlLOC pos;
    uint32_t size;
    char name[16];
    uint32_t attr;
} CdlFILE;

#endif /* PORT_LIBCD_H */
