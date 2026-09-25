/*
 * libgpu.h — portable stand-in for Sony's PSY-Q GPU header.
 *
 * Types AND the primitive structs the game actually builds. A survey of the
 * decomp shows what matters:
 *
 *     POLY_FT4   386 uses   <- textured quad, the workhorse
 *     POLY_F4     30
 *     POLY_GT4    17
 *     LINE_F2     17
 *     POLY_FT3     6        <- textured triangle
 *
 * so POLY_FT4/FT3 are what a renderer has to get right first.
 *
 * Layout is load-bearing: the game writes these structs field by field and
 * hands them to AddPrim, so field order and offsets must match the PS1
 * exactly. static_asserts below pin the sizes.
 */
#ifndef PORT_LIBGPU_H
#define PORT_LIBGPU_H

#include <dw/types.h>
#include <libgte.h>

/* Screen/VRAM rectangle. 8 bytes. */
typedef struct {
    int16_t x, y;
    int16_t w, h;
} RECT;

/* Every primitive starts with this 4-byte tag: 24-bit next pointer + length. */
#define PRIM_TAG uint32_t tag

/* Flat-shaded textured quad — 386 uses, the most important primitive. */
typedef struct {
    PRIM_TAG;
    uint8_t r0, g0, b0, code;
    int16_t x0, y0;  uint8_t u0, v0;  uint16_t clut;
    int16_t x1, y1;  uint8_t u1, v1;  uint16_t tpage;
    int16_t x2, y2;  uint8_t u2, v2;  uint16_t pad1;
    int16_t x3, y3;  uint8_t u3, v3;  uint16_t pad2;
} POLY_FT4;

/* Flat-shaded textured triangle. */
typedef struct {
    PRIM_TAG;
    uint8_t r0, g0, b0, code;
    int16_t x0, y0;  uint8_t u0, v0;  uint16_t clut;
    int16_t x1, y1;  uint8_t u1, v1;  uint16_t tpage;
    int16_t x2, y2;  uint8_t u2, v2;  uint16_t pad1;
} POLY_FT3;

/* Flat-shaded untextured quad. */
typedef struct {
    PRIM_TAG;
    uint8_t r0, g0, b0, code;
    int16_t x0, y0;
    int16_t x1, y1;
    int16_t x2, y2;
    int16_t x3, y3;
} POLY_F4;

/* Gouraud-shaded textured quad. */
typedef struct {
    PRIM_TAG;
    uint8_t r0, g0, b0, code;
    int16_t x0, y0;  uint8_t u0, v0;  uint16_t clut;
    uint8_t r1, g1, b1, p1;
    int16_t x1, y1;  uint8_t u1, v1;  uint16_t tpage;
    uint8_t r2, g2, b2, p2;
    int16_t x2, y2;  uint8_t u2, v2;  uint16_t pad1;
    uint8_t r3, g3, b3, p3;
    int16_t x3, y3;  uint8_t u3, v3;  uint16_t pad2;
} POLY_GT4;

/* Flat line. */
typedef struct {
    PRIM_TAG;
    uint8_t r0, g0, b0, code;
    int16_t x0, y0;
    int16_t x1, y1;
} LINE_F2;

/* Display / draw environments. */
typedef struct {
    RECT disp;
    RECT screen;
    uint8_t isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    RECT clip;
    int16_t ofs[2];
    RECT tw;
    uint16_t tpage;
    uint8_t dtd, dfe, isbg;
    uint8_t r0, g0, b0;
    uint8_t dr_env[92];
} DRAWENV;

/* ---- the PSY-Q primitive macros the game uses ---- */

#define setXY4(p, _x0,_y0, _x1,_y1, _x2,_y2, _x3,_y3) \
    ((p)->x0=(_x0), (p)->y0=(_y0), (p)->x1=(_x1), (p)->y1=(_y1), \
     (p)->x2=(_x2), (p)->y2=(_y2), (p)->x3=(_x3), (p)->y3=(_y3))

#define setXYWH(p, _x, _y, _w, _h) \
    setXY4(p, _x, _y, (_x)+(_w), _y, _x, (_y)+(_h), (_x)+(_w), (_y)+(_h))

#define setUV4(p, _u0,_v0, _u1,_v1, _u2,_v2, _u3,_v3) \
    ((p)->u0=(_u0), (p)->v0=(_v0), (p)->u1=(_u1), (p)->v1=(_v1), \
     (p)->u2=(_u2), (p)->v2=(_v2), (p)->u3=(_u3), (p)->v3=(_v3))

#define setUVWH(p, _u, _v, _w, _h) \
    setUV4(p, _u, _v, (_u)+(_w), _v, _u, (_v)+(_h), (_u)+(_w), (_v)+(_h))

#define setRGB0(p, _r, _g, _b) ((p)->r0=(_r), (p)->g0=(_g), (p)->b0=(_b))

/* Primitive codes, as the PS1 GPU defines them. */
#define POLY_FT4_CODE 0x2C
#define POLY_FT3_CODE 0x24
#define POLY_F4_CODE  0x28
#define POLY_GT4_CODE 0x3C

#define setlen(p, _len)  ((p)->tag = ((p)->tag & 0x00ffffff) | ((uint32_t)(_len) << 24))
#define setcode(p, _c)   ((p)->code = (_c))

#define SetPolyFT4(p) (setlen(p, 9),  setcode(p, POLY_FT4_CODE))
#define SetPolyFT3(p) (setlen(p, 7),  setcode(p, POLY_FT3_CODE))
#define SetPolyF4(p)  (setlen(p, 5),  setcode(p, POLY_F4_CODE))

_Static_assert(sizeof(RECT)     ==  8, "RECT must stay 8 bytes");
_Static_assert(sizeof(POLY_FT4) == 40, "POLY_FT4 must stay 40 bytes (PS1 layout)");
_Static_assert(sizeof(POLY_FT3) == 32, "POLY_FT3 must stay 32 bytes (PS1 layout)");
_Static_assert(sizeof(POLY_F4)  == 24, "POLY_F4 must stay 24 bytes (PS1 layout)");

#endif /* PORT_LIBGPU_H */
