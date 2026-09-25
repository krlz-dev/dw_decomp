/*
 * run_triangle.c — the cut that decides whether the port is realistic.
 *
 * Builds POLY_FT3 and POLY_FT4 primitives using the SAME PSY-Q macros the
 * game's own code uses (setXYWH / setUVWH — exactly what src/main/utils.c
 * calls), then rasterises them through the SDL2 backend.
 *
 * If this works, the rendering path a port needs is proven at its narrowest
 * point. If it does not, better to know now than after months of work.
 */
extern int printf(const char *, ...);

#include <dw/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "prim_glue.h"

#define SCREEN_W 320
#define SCREEN_H 240

#define TEX_X 512      /* where the texture lives in emulated VRAM */
#define TEX_Y 0
#define TEX_W 64
#define TEX_H 64

static uint16_t checker[TEX_W * TEX_H];

/* PS1 colour: 1-bit mask + 5:5:5 BGR. */
static uint16_t rgb15(int r, int g, int b) {
    return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000);
}

/* A checkerboard makes mapping errors obvious: any skew, flip or wrong UV
   shows up immediately as a broken pattern. */
static void buildTexture(void) {
    for (int y = 0; y < TEX_H; y++)
        for (int x = 0; x < TEX_W; x++)
            checker[y * TEX_W + x] = ((x / 8) + (y / 8)) & 1
                ? rgb15(240, 80, 40)     /* orange */
                : rgb15(30, 120, 220);   /* blue   */

    /* A white stripe along the top proves V orientation is right. */
    for (int x = 0; x < TEX_W; x++)
        for (int y = 0; y < 4; y++)
            checker[y * TEX_W + x] = rgb15(255, 255, 255);
}

static int failures = 0;
static void check(const char *what, int ok) {
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static int countDrawn(unsigned int bg) {
    const unsigned int *fb = portGpuFramebuffer();
    int n = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (fb[i] != bg) n++;
    return n;
}

static unsigned int pixelAt(int x, int y) {
    return portGpuFramebuffer()[y * SCREEN_W + x];
}

int main(void) {
    printf("\nPS1 primitives rasterised through SDL2\n");
    printf("======================================\n\n");

    printf("Primitive layouts (must match PS1 exactly):\n");
    printf("  POLY_FT4 = %2d bytes\n", (int)sizeof(POLY_FT4));
    printf("  POLY_FT3 = %2d bytes\n", (int)sizeof(POLY_FT3));
    printf("  POLY_F4  = %2d bytes\n\n", (int)sizeof(POLY_F4));

    if (portGpuInit("Digimon World — port spike") != 0) {
        printf("  FAIL  could not initialise the renderer\n");
        return 1;
    }

    buildTexture();
    portGpuLoadTexture(TEX_X, TEX_Y, TEX_W, TEX_H, checker);

    const unsigned int BG = 0xFF101018u;
    portGpuClear(0x10, 0x10, 0x18);
    check("framebuffer cleared", countDrawn(BG) == 0);

    /* ---- the textured triangle ---- */
    POLY_FT3 tri;
    SetPolyFT3(&tri);
    setRGB0(&tri, 0x80, 0x80, 0x80);      /* 0x80 = no modulation */
    tri.x0 =  60; tri.y0 =  40; tri.u0 =  0; tri.v0 =  0;
    tri.x1 = 150; tri.y1 =  40; tri.u1 = 63; tri.v1 =  0;
    tri.x2 = 105; tri.y2 = 130; tri.u2 = 31; tri.v2 = 63;
    portSubmitFT3(&tri, TEX_X, TEX_Y);

    int afterTri = countDrawn(BG);
    printf("\nTextured triangle:\n");
    printf("  pixels drawn: %d\n", afterTri);
    check("the triangle rasterised", afterTri > 2000);
    check("its centroid is textured", pixelAt(105, 70) != BG);
    check("outside the triangle stays clear", pixelAt(10, 10) == BG);

    /* ---- the textured quad, built with the GAME'S OWN macros ---- */
    POLY_FT4 quad;
    SetPolyFT4(&quad);
    setRGB0(&quad, 0x80, 0x80, 0x80);
    setXYWH(&quad, 180, 60, 100, 100);   /* exactly what utils.c calls */
    setUVWH(&quad,   0,  0,  63,  63);
    portSubmitFT4(&quad, TEX_X, TEX_Y);

    int afterQuad = countDrawn(BG) - afterTri;
    printf("\nTextured quad (setXYWH/setUVWH — the game's own macros):\n");
    printf("  pixels drawn: %d\n", afterQuad);
    check("the quad rasterised", afterQuad > 8000);
    check("its centre is textured", pixelAt(230, 110) != BG);

    /* The white stripe sits at v=0..3, so the quad's TOP edge must be white
       and its bottom must not. Catches a flipped V coordinate. */
    unsigned int top = pixelAt(230, 63), bottom = pixelAt(230, 155);
    printf("  top pixel    0x%08X\n", top);
    printf("  bottom pixel 0x%08X\n", bottom);
    check("V orientation correct (top stripe is white)",
          (top & 0xFFFFFF) > 0xE0E0E0 && (bottom & 0xFFFFFF) < 0xE0E0E0);

    /* ---- colour modulation, how the game tints sprites ---- */
    POLY_FT4 tinted;
    SetPolyFT4(&tinted);
    setRGB0(&tinted, 0x40, 0x40, 0x40);   /* half brightness */
    setXYWH(&tinted, 180, 170, 100, 60);
    setUVWH(&tinted,   0,   0,  63, 63);
    portSubmitFT4(&tinted, TEX_X, TEX_Y);
    printf("\nColour modulation:\n");
    check("a tinted quad is darker than an untinted one",
          (pixelAt(230, 200) & 0xFF) < (pixelAt(230, 110) & 0xFF));

    portGpuPresent();
    if (portGpuSaveBMP("/tmp/port_triangle.bmp") == 0)
        printf("\n  frame saved to /tmp/port_triangle.bmp\n");

    printf("\n%s\n\n", failures == 0
        ? "PASSED — PS1 primitives rasterise correctly through SDL2."
        : "FAILED");

    portGpuShutdown();
    return failures == 0 ? 0 : 1;
}
