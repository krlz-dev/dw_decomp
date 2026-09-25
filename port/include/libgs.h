/*
 * libgs.h — portable stand-in for Sony's PSY-Q graphics-system header.
 *
 * GS sits on top of the GPU and owns the ordering table (OT) and the scene
 * graph of "display objects". Types only; ordering and drawing belong to the
 * SDL2 backend.
 */
#ifndef PORT_LIBGS_H
#define PORT_LIBGS_H

#include <dw/types.h>
#include <libgte.h>
#include <libgpu.h>

/* One entry of the ordering table: a 24-bit pointer plus a length byte. */
typedef struct {
    uint32_t tag;
} GsOT_TAG;

/* The ordering table itself — the PS1's depth-sorted draw list. */
typedef struct {
    uint32_t length;
    GsOT_TAG *org;
    uint32_t offset;
    uint32_t point;
    GsOT_TAG *tag;
} GsOT;

/* A node in the transform hierarchy. */
typedef struct GsCOORDINATE2 {
    uint32_t flg;
    MATRIX coord;
    MATRIX workm;
    SVECTOR *rotate;
    struct GsCOORDINATE2 *super;
    struct GsCOORDINATE2 *sub;
} GsCOORDINATE2;

/* A drawable object: a TMD model plus its transform. */
typedef struct {
    uint32_t attribute;
    GsCOORDINATE2 *coord2;
    uint32_t *tmd;
    uint32_t id;
} GsDOBJ2;

/* PS1 model format. Declared opaque on purpose: the port reads TMD data with
   its own loader, so nothing here needs the real internal layout. */
struct TMD_STRUCT {
    uint32_t id;
    uint32_t flags;
    uint32_t nobj;
};

typedef struct {
    uint32_t id;
    uint32_t flags;
    struct TMD_STRUCT obj[1];
} GsTMD;

#endif /* PORT_LIBGS_H */
