/*
 * gpu_sdl.c — an SDL2 backend for the PS1 primitives the game actually emits.
 *
 * This is the piece that decides whether a port is realistic: the game builds
 * POLY_FT4 / POLY_FT3 structs and hands them to AddPrim, expecting Sony's GPU
 * to rasterise them. Here we rasterise them ourselves.
 *
 * A survey of the decomp shows what matters:
 *     POLY_FT4  386 uses    <- textured quad, the workhorse
 *     POLY_F4    30
 *     POLY_GT4   17
 *     POLY_FT3    6         <- textured triangle
 * so textured tris and quads are 392 of ~460 primitive uses: the
 * representative case, not a toy.
 *
 * This file deliberately does NOT include the game's headers — see gpu_sdl.h.
 *
 * Not implemented: perspective correction (the PS1 had none either; its affine
 * warping is part of the look), semi-transparency modes, Gouraud shading, and
 * the GTE transform that produces these screen coordinates upstream.
 */
#include <SDL2/SDL.h>
#include "gpu_sdl.h"

#define VRAM_W 1024
#define VRAM_H 512

static SDL_Window   *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture  *g_screen;

/* The PS1 keeps textures in VRAM; a flat array emulates it so texture
   coordinates behave the way the game expects. */
static unsigned short g_vram[VRAM_W * VRAM_H];
static unsigned int   g_fb[PORT_SCREEN_W * PORT_SCREEN_H];

int portGpuInit(const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return -1;
    g_window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        PORT_SCREEN_W * 2, PORT_SCREEN_H * 2, SDL_WINDOW_SHOWN);
    if (!g_window) return -1;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) return -1;
    g_screen = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, PORT_SCREEN_W, PORT_SCREEN_H);
    return g_screen ? 0 : -1;
}

void portGpuShutdown(void) {
    if (g_screen)   SDL_DestroyTexture(g_screen);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window)   SDL_DestroyWindow(g_window);
    SDL_Quit();
}

void portGpuClear(unsigned char r, unsigned char g, unsigned char b) {
    unsigned int c = 0xFF000000u | ((unsigned int)r << 16) | ((unsigned int)g << 8) | b;
    for (int i = 0; i < PORT_SCREEN_W * PORT_SCREEN_H; i++) g_fb[i] = c;
}

/* Stands in for LoadImage(): upload pixels into emulated VRAM. */
void portGpuLoadTexture(int x, int y, int w, int h, const unsigned short *pixels) {
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VRAM_H) continue;
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= VRAM_W) continue;
            g_vram[dy * VRAM_W + dx] = pixels[row * w + col];
        }
    }
}

/* PS1 colour: 1-bit mask + 5:5:5 BGR. Expand to 8:8:8. */
static unsigned int vramToARGB(unsigned short p) {
    unsigned int r = (p & 0x1F) << 3;
    unsigned int g = ((p >> 5) & 0x1F) << 3;
    unsigned int b = ((p >> 10) & 0x1F) << 3;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void putPixel(int x, int y, unsigned int argb) {
    if (x < 0 || x >= PORT_SCREEN_W || y < 0 || y >= PORT_SCREEN_H) return;
    g_fb[y * PORT_SCREEN_W + x] = argb;
}

/* Edge function: sign tells which side of edge a->b the point lies on. */
static int edge(int ax, int ay, int bx, int by, int px, int py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/*
 * Rasterise one textured triangle, interpolating UVs barycentrically.
 *
 * Affine, not perspective-correct — matching the PS1, whose lack of
 * perspective correction causes its characteristic texture warping.
 * "Fixing" that here would make the port look wrong, not better.
 */
static void rasterTexTri(
    int x0, int y0, int u0, int v0,
    int x1, int y1, int u1, int v1,
    int x2, int y2, int u2, int v2,
    int texBaseX, int texBaseY,
    unsigned char mr, unsigned char mg, unsigned char mb)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= PORT_SCREEN_W) maxx = PORT_SCREEN_W - 1;
    if (maxy >= PORT_SCREEN_H) maxy = PORT_SCREEN_H - 1;

    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;                     /* degenerate */
    int sign = area > 0 ? 1 : -1;              /* accept either winding */
    int absArea = area * sign;

    for (int py = miny; py <= maxy; py++) {
        for (int px = minx; px <= maxx; px++) {
            int w0 = edge(x1, y1, x2, y2, px, py) * sign;
            int w1 = edge(x2, y2, x0, y0, px, py) * sign;
            int w2 = edge(x0, y0, x1, y1, px, py) * sign;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;

            int u = (w0 * u0 + w1 * u1 + w2 * u2) / absArea;
            int v = (w0 * v0 + w1 * v1 + w2 * v2) / absArea;

            int tx = texBaseX + u, ty = texBaseY + v;
            if (tx < 0 || tx >= VRAM_W || ty < 0 || ty >= VRAM_H) continue;

            unsigned short texel = g_vram[ty * VRAM_W + tx];
            if (texel == 0) continue;          /* PS1: 0x0000 is transparent */

            unsigned int c = vramToARGB(texel);
            /* Flat modulation, PS1 style: 0x80 means "unchanged". */
            unsigned int r = (((c >> 16) & 0xFF) * mr) >> 7;
            unsigned int g = (((c >>  8) & 0xFF) * mg) >> 7;
            unsigned int b = (( c        & 0xFF) * mb) >> 7;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            putPixel(px, py, 0xFF000000u | (r << 16) | (g << 8) | b);
        }
    }
}

/*
 * A PS1 quad is two triangles sharing an edge, vertices in Z order:
 *   0---1
 *   | \ |
 *   2---3
 */
void portGpuDrawPrim(const PortPrim *p) {
    rasterTexTri(p->x[0], p->y[0], p->u[0], p->v[0],
                 p->x[1], p->y[1], p->u[1], p->v[1],
                 p->x[2], p->y[2], p->u[2], p->v[2],
                 p->texBaseX, p->texBaseY, p->r, p->g, p->b);
    if (p->nverts == 4) {
        rasterTexTri(p->x[1], p->y[1], p->u[1], p->v[1],
                     p->x[3], p->y[3], p->u[3], p->v[3],
                     p->x[2], p->y[2], p->u[2], p->v[2],
                     p->texBaseX, p->texBaseY, p->r, p->g, p->b);
    }
}

void portGpuPresent(void) {
    SDL_UpdateTexture(g_screen, NULL, g_fb, PORT_SCREEN_W * sizeof(unsigned int));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_screen, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

/* Save the framebuffer so the result can be inspected headless. */
int portGpuSaveBMP(const char *path) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
        g_fb, PORT_SCREEN_W, PORT_SCREEN_H, 32,
        PORT_SCREEN_W * sizeof(unsigned int), SDL_PIXELFORMAT_ARGB8888);
    if (!s) return -1;
    int rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc;
}

const unsigned int *portGpuFramebuffer(void) { return g_fb; }
