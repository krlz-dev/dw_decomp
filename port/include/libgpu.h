/*
 * libgpu.h — portable stand-in for Sony's PSY-Q GPU header.
 *
 * Types only. The drawing primitives themselves are the job of the SDL2
 * backend; this header exists so game headers that embed a RECT or a DISPENV
 * can be parsed off the PlayStation.
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

/* Display environment — which part of VRAM is being shown. */
typedef struct {
    RECT disp;
    RECT screen;
    uint8_t isinter;
    uint8_t isrgb24;
    uint8_t pad0, pad1;
} DISPENV;

/* Draw environment — where primitives land, and how they are clipped. */
typedef struct {
    RECT clip;
    int16_t ofs[2];
    RECT tw;
    uint16_t tpage;
    uint8_t dtd, dfe;
    uint8_t isbg;
    uint8_t r0, g0, b0;
    uint8_t dr_env[92];
} DRAWENV;

_Static_assert(sizeof(RECT) == 8, "RECT must stay 8 bytes");

#endif /* PORT_LIBGPU_H */
