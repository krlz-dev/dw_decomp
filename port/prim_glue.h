/*
 * prim_glue.h — submit PSY-Q primitives to the port's renderer.
 *
 * Only include this from translation units that already speak the game's
 * types; it references POLY_FT3/POLY_FT4 from libgpu.h.
 */
#ifndef PORT_PRIM_GLUE_H
#define PORT_PRIM_GLUE_H

struct POLY_FT3;
struct POLY_FT4;

void portSubmitFT3(const POLY_FT3 *p, int texBaseX, int texBaseY);
void portSubmitFT4(const POLY_FT4 *p, int texBaseX, int texBaseY);

/* Re-exported from the backend so game-side code never includes gpu_sdl.h. */
int  portGpuInit(const char *title);
void portGpuShutdown(void);
void portGpuClear(unsigned char r, unsigned char g, unsigned char b);
void portGpuLoadTexture(int x, int y, int w, int h, const unsigned short *pixels);
void portGpuPresent(void);
int  portGpuSaveBMP(const char *path);
const unsigned int *portGpuFramebuffer(void);

#endif
