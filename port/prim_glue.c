/*
 * prim_glue.c — where the game's world meets the renderer's world.
 *
 * This is the actual boundary of a PS1 port, and the reason it gets its own
 * translation unit: this file includes the GAME's headers (32-bit MIPS types,
 * PSY-Q primitive structs) and speaks to the backend only through plain C
 * types. SDL is never visible here, and dw/types.h is never visible to SDL.
 *
 * Keeping that fence explicit is what makes the rest of the port tractable.
 */
extern int printf(const char *, ...);

#include <dw/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "gpu_sdl.h"
#include "prim_glue.h"

void portSubmitFT3(const POLY_FT3 *p, int texBaseX, int texBaseY) {
    PortPrim out;
    out.nverts = 3;
    out.x[0] = p->x0; out.y[0] = p->y0; out.u[0] = p->u0; out.v[0] = p->v0;
    out.x[1] = p->x1; out.y[1] = p->y1; out.u[1] = p->u1; out.v[1] = p->v1;
    out.x[2] = p->x2; out.y[2] = p->y2; out.u[2] = p->u2; out.v[2] = p->v2;
    out.x[3] = out.y[3] = out.u[3] = out.v[3] = 0;
    out.r = p->r0; out.g = p->g0; out.b = p->b0;
    out.texBaseX = texBaseX; out.texBaseY = texBaseY;
    portGpuDrawPrim(&out);
}

void portSubmitFT4(const POLY_FT4 *p, int texBaseX, int texBaseY) {
    PortPrim out;
    out.nverts = 4;
    out.x[0] = p->x0; out.y[0] = p->y0; out.u[0] = p->u0; out.v[0] = p->v0;
    out.x[1] = p->x1; out.y[1] = p->y1; out.u[1] = p->u1; out.v[1] = p->v1;
    out.x[2] = p->x2; out.y[2] = p->y2; out.u[2] = p->u2; out.v[2] = p->v2;
    out.x[3] = p->x3; out.y[3] = p->y3; out.u[3] = p->u3; out.v[3] = p->v3;
    out.r = p->r0; out.g = p->g0; out.b = p->b0;
    out.texBaseX = texBaseX; out.texBaseY = texBaseY;
    portGpuDrawPrim(&out);
}
