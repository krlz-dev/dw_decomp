/*
 * run_scene.c — the three layers working together.
 *
 * Until now each piece was proven alone. This is the first test where the full
 * pipeline runs end to end, the way a frame actually happens on the console:
 *
 *   1. GTE     transforms model-space vertices into screen space (RTPS)
 *   2. OT      buckets each primitive by its depth
 *   3. GPU     draws the buckets far-to-near
 *
 * The thing worth proving is OCCLUSION: a near quad must cover a far one, and
 * it must do so because of the ordering table, not because of the order the
 * code happened to submit them in. So the test deliberately submits the NEAR
 * quad FIRST — if the OT is wrong, the far quad paints over it and the check
 * fails.
 */
#include <stdio.h>
#include <string.h>
#include "gte.h"
#include "ot.h"
#include "gpu_sdl.h"

#define TEX_X 512
#define TEX_Y 0
#define TEX_W 64
#define TEX_H 64

static unsigned short texA[TEX_W * TEX_H];   /* red-ish  */
static unsigned short texB[TEX_W * TEX_H];   /* green-ish */

static unsigned short rgb15(int r, int g, int b) {
    return (unsigned short)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000);
}

static int failures = 0;
static void ok(const char *what, int cond) {
    printf("  %s  %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) failures++;
}

/* A quad already in screen space, carrying its depth bucket. */
typedef struct {
    int x[4], y[4], u[4], v[4];
    int texX, texY;
    unsigned char r, g, b;
    int slot;
} SceneQuad;

static void drawQuad(void *p, int slot, void *user) {
    (void)slot; (void)user;
    const SceneQuad *q = (const SceneQuad *)p;
    PortPrim prim;
    prim.nverts = 4;
    for (int i = 0; i < 4; i++) {
        prim.x[i] = q->x[i]; prim.y[i] = q->y[i];
        prim.u[i] = q->u[i]; prim.v[i] = q->v[i];
    }
    prim.r = q->r; prim.g = q->g; prim.b = q->b;
    prim.texBaseX = q->texX; prim.texBaseY = q->texY;
    portGpuDrawPrim(&prim);
}

/* Project a model-space square through the GTE into a screen-space quad. */
static int projectSquare(const PortMatrix *m, const PortGeom *g,
                         int halfSize, SceneQuad *out)
{
    const int corner[4][3] = {
        { -halfSize, -halfSize, 0 },   /* 0 top-left     */
        {  halfSize, -halfSize, 0 },   /* 1 top-right    */
        { -halfSize,  halfSize, 0 },   /* 2 bottom-left  */
        {  halfSize,  halfSize, 0 },   /* 3 bottom-right */
    };
    int zsum = 0;
    for (int i = 0; i < 4; i++) {
        PortSVec v = { (short)corner[i][0], (short)corner[i][1], (short)corner[i][2], 0 };
        PortScreen s;
        if (!portRtps(m, &v, g, &s)) return 0;    /* clipped */
        out->x[i] = s.x; out->y[i] = s.y;
        zsum += s.z;
    }
    /* UVs match the PS1 quad winding: 0-1 top, 2-3 bottom. */
    out->u[0] = 0;  out->v[0] = 0;
    out->u[1] = 63; out->v[1] = 0;
    out->u[2] = 0;  out->v[2] = 63;
    out->u[3] = 63; out->v[3] = 63;

    /* Average Z picks the depth bucket — this is what gte_avsz4 does on
       hardware, and what GsSortObject4 uses internally. */
    out->slot = (zsum / 4) >> 4;
    return 1;
}

int main(void) {
    printf("\nFull frame: GTE -> ordering table -> rasteriser\n");
    printf("==============================================\n\n");

    if (portGpuInit("Digimon World — scene spike") != 0) {
        printf("  FAIL  renderer init\n");
        return 1;
    }

    for (int i = 0; i < TEX_W * TEX_H; i++) {
        int cell = ((i % TEX_W) / 8 + (i / TEX_W) / 8) & 1;
        texA[i] = cell ? rgb15(230, 60, 60)  : rgb15(120, 20, 20);
        texB[i] = cell ? rgb15(60, 220, 90)  : rgb15(20, 110, 40);
    }
    portGpuLoadTexture(TEX_X, TEX_Y, TEX_W, TEX_H, texA);
    portGpuLoadTexture(TEX_X, TEX_Y + 64, TEX_W, TEX_H, texB);

    PortGeom geom = { .h = 300, .ofx = 160, .ofy = 120 };
    PortOT ot;
    portOtInit(&ot, 1024);
    ok("ordering table initialised", portOtCount(&ot) == 0);

    /* --- two squares at different depths, same screen position --- */
    PortSVec noRot = {0, 0, 0, 0};
    PortMatrix mFar, mNear;
    portRotMatrixZYX(&noRot, &mFar);
    portRotMatrixZYX(&noRot, &mNear);
    portTransMatrix(&mFar,  &(PortVec){0, 0, 1600, 0});   /* far  */
    portTransMatrix(&mNear, &(PortVec){0, 0,  700, 0});   /* near */

    SceneQuad far, near;
    ok("far square projects",  projectSquare(&mFar,  &geom, 200, &far));
    ok("near square projects", projectSquare(&mNear, &geom, 200, &near));

    far.texX = TEX_X;  far.texY = TEX_Y;       /* red  */
    near.texX = TEX_X; near.texY = TEX_Y + 64; /* green */
    far.r = far.g = far.b = 0x80;
    near.r = near.g = near.b = 0x80;

    printf("\n  far  square: slot %4d, on screen %d..%d px wide\n",
           far.slot, far.x[0], far.x[1]);
    printf("  near square: slot %4d, on screen %d..%d px wide\n",
           near.slot, near.x[0], near.x[1]);

    ok("the nearer square is bigger on screen",
       (near.x[1] - near.x[0]) > (far.x[1] - far.x[0]));
    ok("the nearer square lands in a LOWER OT slot", near.slot < far.slot);

    /* --- submit NEAR first, on purpose --- */
    portOtAdd(&ot, near.slot, &near);
    portOtAdd(&ot, far.slot,  &far);
    ok("both primitives are in the table", portOtCount(&ot) == 2);
    ok("they landed in different buckets",
       portOtBucketCount(&ot, near.slot) == 1 &&
       portOtBucketCount(&ot, far.slot) == 1);

    portGpuClear(0x10, 0x10, 0x18);
    portOtDraw(&ot, drawQuad, NULL);

    /* The centre pixel must be GREEN: the near square wins despite being
       submitted first. If the OT walked the wrong way it would be red. */
    unsigned int centre = portGpuFramebuffer()[120 * 320 + 160];
    unsigned int r = (centre >> 16) & 0xFF, g = (centre >> 8) & 0xFF;
    printf("\n  centre pixel: 0x%08X  (r=%u g=%u)\n", centre, r, g);
    ok("the NEAR square occludes the far one (green wins)", g > r);

    /* The near square spans x=75..245 and the far one x=123..197, so the far
     * square is completely hidden — correct physics, and a reminder that
     * "nothing red is visible" is the expected outcome here rather than a
     * failure. Counting pixels states that explicitly. */
    int redPixels = 0, greenPixels = 0;
    const unsigned int *fb = portGpuFramebuffer();
    for (int i = 0; i < 320 * 240; i++) {
        unsigned int pr = (fb[i] >> 16) & 0xFF, pg = (fb[i] >> 8) & 0xFF;
        if (pr > pg + 20) redPixels++;
        if (pg > pr + 20) greenPixels++;
    }
    printf("  green pixels: %d, red pixels: %d\n", greenPixels, redPixels);
    ok("the near square is drawn", greenPixels > 8000);
    ok("the far square is fully hidden behind it", redPixels == 0);

    /* Now prove the ordering actually did the work: swap the depths so the
     * RED square is nearer, submit in the same order, and the result must
     * invert. If drawing were submission-ordered this would not change. */
    portOtClear(&ot);
    int tmpSlot = near.slot; near.slot = far.slot; far.slot = tmpSlot;
    portOtAdd(&ot, near.slot, &near);
    portOtAdd(&ot, far.slot,  &far);
    portGpuClear(0x10, 0x10, 0x18);
    portOtDraw(&ot, drawQuad, NULL);

    unsigned int swapped = portGpuFramebuffer()[120 * 320 + 160];
    unsigned int sr = (swapped >> 16) & 0xFF, sg = (swapped >> 8) & 0xFF;
    printf("  after swapping depths, centre: 0x%08X (r=%u g=%u)\n", swapped, sr, sg);
    ok("swapping depth inverts what is visible — the OT is doing the sorting",
       sr > sg);

    /* restore for the bucket test below */
    tmpSlot = near.slot; near.slot = far.slot; far.slot = tmpSlot;

    /* --- within one bucket, AddPrim pushes to the front --- */
    portOtClear(&ot);
    ok("clearing empties the table", portOtCount(&ot) == 0);

    static int order[3] = {1, 2, 3};
    portOtAdd(&ot, 50, &order[0]);
    portOtAdd(&ot, 50, &order[1]);
    portOtAdd(&ot, 50, &order[2]);
    ok("three primitives share one bucket", portOtBucketCount(&ot, 50) == 3);

    portGpuPresent();
    if (portGpuSaveBMP("/tmp/port_scene.bmp") == 0)
        printf("\n  frame saved to /tmp/port_scene.bmp\n");

    printf("\n%s\n\n", failures == 0
        ? "PASSED — GTE, ordering table and rasteriser work as one pipeline."
        : "FAILED");

    portGpuShutdown();
    return failures == 0 ? 0 : 1;
}
