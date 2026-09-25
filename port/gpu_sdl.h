/*
 * gpu_sdl.h — the port's rendering surface.
 *
 * IMPORTANT: this header does NOT include the game's dw/types.h.
 *
 * SDL pulls in the system <stdint.h>, and the decomp defines its own 32-bit
 * int8_t / intptr_t for MIPS. Including both in one translation unit is a hard
 * conflict. So the backend speaks plain C types and stays on its own side of
 * the fence; the glue layer converts.
 *
 * That separation is not a workaround, it is the right shape for a port: the
 * renderer should not know anything about the game's type universe.
 */
#ifndef PORT_GPU_SDL_H
#define PORT_GPU_SDL_H

/* PS1 NTSC resolution. */
#define PORT_SCREEN_W 320
#define PORT_SCREEN_H 240

/* A primitive flattened into backend terms: screen coords, UVs, flat colour.
   The glue layer fills this from a POLY_FT3/POLY_FT4. */
typedef struct {
    int x[4], y[4];          /* up to 4 vertices */
    int u[4], v[4];
    int nverts;              /* 3 = triangle, 4 = quad */
    unsigned char r, g, b;   /* flat modulation colour */
    int texBaseX, texBaseY;  /* texture origin in emulated VRAM */
} PortPrim;

int  portGpuInit(const char *title);
void portGpuShutdown(void);
void portGpuClear(unsigned char r, unsigned char g, unsigned char b);
void portGpuLoadTexture(int x, int y, int w, int h, const unsigned short *pixels);
void portGpuDrawPrim(const PortPrim *p);
void portGpuPresent(void);
int  portGpuSaveBMP(const char *path);
const unsigned int *portGpuFramebuffer(void);

#endif
