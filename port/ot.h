/*
 * ot.h — the PS1 ordering table.
 *
 * Plain C types only, same fence as gpu_sdl.h and gte.h: never sees the game's
 * 32-bit MIPS typedefs.
 */
#ifndef PORT_OT_H
#define PORT_OT_H

/* Measured: GsClearOt is called with depths up to 0xfff. */
#define PORT_OT_MAX   4096
/* Scratch capacity for one frame's primitives. The game reserves this with
   GsGetWorkBase/GsSetWorkBase (177/168 call sites). */
#define PORT_OT_PRIMS 8192

typedef struct PortOtNode {
    void *prim;
    struct PortOtNode *next;
} PortOtNode;

typedef struct {
    int depth;
    int count;
    PortOtNode *bucket[PORT_OT_MAX];
    PortOtNode  pool[PORT_OT_PRIMS];
} PortOT;

/* Called once per primitive, in draw order. `slot` is its depth bucket. */
typedef void (*PortOtDrawFn)(void *prim, int slot, void *user);

void portOtInit(PortOT *ot, int depth);
void portOtClear(PortOT *ot);
void portOtAdd(PortOT *ot, int slot, void *prim);
void portOtDraw(const PortOT *ot, PortOtDrawFn fn, void *user);
int  portOtCount(const PortOT *ot);
int  portOtBucketCount(const PortOT *ot, int slot);

#endif
