/*
 * ot.c — the PS1 ordering table, in software.
 *
 * Measured scope, not guessed. A survey of src/ counts:
 *
 *     GsGetWorkBase  177   AddPrim        173   GsSetWorkBase  168
 *     DrawSync       134   GsSortSprite    45   GsSortObject4   42
 *     GsClearOt       32   GsSortBoxFill   23   GsDrawOt        10
 *
 * and the declarations say GsOT NAME[2] everywhere — the game is
 * double-buffered, with ACTIVE_FRAMEBUFFER selecting the bank.
 *
 * WHAT AN OT ACTUALLY IS
 *
 * Not a sorting algorithm: a bucket array. Each slot is one depth value, and
 * every slot holds a singly-linked list of primitives. AddPrim(ot->org + n, p)
 * pushes p onto bucket n. GsDrawOt then walks buckets from the far end to the
 * near end, drawing each list.
 *
 * That gives O(1) insertion and no comparisons at all — which is how a 33MHz
 * console depth-sorted a scene every frame. Rebuilding it with qsort would be
 * both slower and WRONG: primitives at equal depth must keep their submission
 * order, and a comparison sort would not guarantee it.
 *
 * The one subtlety that matters: AddPrim pushes onto the FRONT of each bucket,
 * so within one depth the last primitive submitted is drawn FIRST. Getting
 * that backwards silently inverts overlapping UI elements — the kind of bug
 * that looks like "a sprite is missing" rather than "the list is reversed".
 *
 * Measured depths in use: GsClearOt is called with 0, 2, 4, 5, 0xa, 0xffe and
 * 0xfff, so the table must handle up to 4096 entries.
 */
#include <stdlib.h>
#include <string.h>
#include "ot.h"

void portOtInit(PortOT *ot, int depth) {
    if (depth < 1) depth = 1;
    if (depth > PORT_OT_MAX) depth = PORT_OT_MAX;
    ot->depth = depth;
    ot->count = 0;
    memset(ot->bucket, 0, sizeof(ot->bucket[0]) * depth);
}

/* GsClearOt: empty every bucket. Cheap — the primitives themselves live in the
   caller's scratch buffer, which the game resets each frame via
   GsSetWorkBase. Nothing is freed here because nothing was allocated. */
void portOtClear(PortOT *ot) {
    memset(ot->bucket, 0, sizeof(ot->bucket[0]) * ot->depth);
    ot->count = 0;
}

/*
 * AddPrim — push a primitive onto bucket `slot`.
 *
 * Out-of-range slots are clamped rather than ignored: the hardware would
 * happily scribble past the table, and silently dropping geometry is a worse
 * failure mode for a port than drawing it slightly wrong.
 */
void portOtAdd(PortOT *ot, int slot, void *prim) {
    if (slot < 0) slot = 0;
    if (slot >= ot->depth) slot = ot->depth - 1;
    if (ot->count >= PORT_OT_PRIMS) return;      /* scratch exhausted */

    PortOtNode *n = &ot->pool[ot->count++];
    n->prim = prim;
    n->next = ot->bucket[slot];                  /* push front, like AddPrim */
    ot->bucket[slot] = n;
}

/*
 * GsDrawOt — walk the table from the FAR end (high index) to the NEAR end.
 *
 * On the PS1 a larger OT index means farther away, so drawing high-to-low is
 * painter's algorithm: distant geometry first, near geometry over the top.
 * Reversing this draws the world in front of the characters.
 */
void portOtDraw(const PortOT *ot, PortOtDrawFn fn, void *user) {
    for (int slot = ot->depth - 1; slot >= 0; slot--)
        for (const PortOtNode *n = ot->bucket[slot]; n; n = n->next)
            fn(n->prim, slot, user);
}

int portOtCount(const PortOT *ot) { return ot->count; }

int portOtBucketCount(const PortOT *ot, int slot) {
    if (slot < 0 || slot >= ot->depth) return 0;
    int n = 0;
    for (const PortOtNode *p = ot->bucket[slot]; p; p = p->next) n++;
    return n;
}
