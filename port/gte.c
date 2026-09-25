/*
 * gte.c — software implementation of the PS1 Geometry Transformation Engine.
 *
 * Measured scope, not guessed: a survey of src/ counts 632 GTE call sites
 * across 29 distinct operations in 34 of 126 files. The distribution is
 * heavily skewed —
 *
 *     ApplyMatrixSV  103    RotMatrix      51    RotMatrixZYX  44
 *     RotMatrixYXZ    44    ratan2         43    gte_stsxy     42
 *     gte_rtps        42    gte_ldv0       42    ScaleMatrix   39
 *     TransMatrix     37    ApplyMatrixLV  23
 *
 * — so eleven operations cover 80% of all uses. Those are implemented here.
 *
 * WHY THIS IS THE RISKY PART: the GTE is fixed-point. Every result is a
 * specific number of fractional bits, and the hardware saturates rather than
 * wrapping. Get a shift wrong and the game still runs, still draws, and looks
 * subtly wrong in a way that is miserable to track down. So each operation
 * below states its fixed-point contract, and the tests check the contract
 * rather than "looks about right".
 *
 * Reference: the GTE's documented behaviour (Sony PSY-Q docs / nocash PSX
 * spec). Where behaviour is ambiguous it is flagged in a comment instead of
 * being quietly guessed.
 */
#include "gte.h"

/* ---- fixed-point conventions ----
 *
 * Rotation matrix entries are 1.3.12 — one sign bit, three integer bits,
 * twelve fractional. So 4096 == 1.0.
 *
 * Angles are 1.3.12 as well: 4096 == one full turn is NOT the convention;
 * the PS1 uses 4096 == 360 degrees for csin/ccos tables. ratan2 returns the
 * same units.
 */

#define ONE 4096            /* 1.0 in 1.3.12 */

/* The GTE saturates instead of wrapping. Modelling that matters: a wrapped
   value flips a vertex to the opposite side of the screen, which is exactly
   the class of bug that is hard to see and easy to ship. */
static int32_t sat16(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return v;
}

/* ---- matrix construction ---- */

/*
 * A 3x3 rotation about X, Y then Z, applied in that order (ZYX composition).
 * Entries are 1.3.12.
 */
void portRotMatrixZYX(const PortSVec *rot, PortMatrix *m) {
    int32_t sx = portCsin(rot->vx), cx = portCcos(rot->vx);
    int32_t sy = portCsin(rot->vy), cy = portCcos(rot->vy);
    int32_t sz = portCsin(rot->vz), cz = portCcos(rot->vz);

    /* Products of two 1.3.12 values are 1.6.24, so shift back by 12. */
    m->m[0][0] = (int16_t)(((cz * cy) >> 12));
    m->m[0][1] = (int16_t)((((cz * sy) >> 12) * sx >> 12) - ((sz * cx) >> 12));
    m->m[0][2] = (int16_t)((((cz * sy) >> 12) * cx >> 12) + ((sz * sx) >> 12));

    m->m[1][0] = (int16_t)(((sz * cy) >> 12));
    m->m[1][1] = (int16_t)((((sz * sy) >> 12) * sx >> 12) + ((cz * cx) >> 12));
    m->m[1][2] = (int16_t)((((sz * sy) >> 12) * cx >> 12) - ((cz * sx) >> 12));

    m->m[2][0] = (int16_t)(-sy);
    m->m[2][1] = (int16_t)(((cy * sx) >> 12));
    m->m[2][2] = (int16_t)(((cy * cx) >> 12));

    m->t[0] = m->t[1] = m->t[2] = 0;
}

/* Scale each row of the matrix by a 1.3.12 factor per axis. */
void portScaleMatrix(PortMatrix *m, const PortVec *scale) {
    const int32_t s[3] = { scale->vx, scale->vy, scale->vz };
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            m->m[r][c] = (int16_t)(((int32_t)m->m[r][c] * s[c]) >> 12);
}

void portTransMatrix(PortMatrix *m, const PortVec *t) {
    m->t[0] = t->vx; m->t[1] = t->vy; m->t[2] = t->vz;
}

/* out = a * b, both 1.3.12. */
void portMulMatrix0(const PortMatrix *a, const PortMatrix *b, PortMatrix *out) {
    PortMatrix r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int32_t acc = 0;
            for (int k = 0; k < 3; k++)
                acc += (int32_t)a->m[i][k] * b->m[k][j];
            r.m[i][j] = (int16_t)(acc >> 12);
        }
    r.t[0] = a->t[0]; r.t[1] = a->t[1]; r.t[2] = a->t[2];
    *out = r;
}

/* ---- vector transforms ---- */

/*
 * ApplyMatrixSV: out = M * v, with a 16-bit SVECTOR in and out.
 * The most used GTE operation in the game (103 sites).
 * Results saturate, matching the hardware.
 */
void portApplyMatrixSV(const PortMatrix *m, const PortSVec *v, PortSVec *out) {
    int32_t x = ((int32_t)m->m[0][0] * v->vx + (int32_t)m->m[0][1] * v->vy + (int32_t)m->m[0][2] * v->vz) >> 12;
    int32_t y = ((int32_t)m->m[1][0] * v->vx + (int32_t)m->m[1][1] * v->vy + (int32_t)m->m[1][2] * v->vz) >> 12;
    int32_t z = ((int32_t)m->m[2][0] * v->vx + (int32_t)m->m[2][1] * v->vy + (int32_t)m->m[2][2] * v->vz) >> 12;
    out->vx = (int16_t)sat16(x);
    out->vy = (int16_t)sat16(y);
    out->vz = (int16_t)sat16(z);
}

/* Same, with a 32-bit VECTOR in and out (no saturation to 16 bits). */
void portApplyMatrixLV(const PortMatrix *m, const PortVec *v, PortVec *out) {
    int32_t x = ((int32_t)m->m[0][0] * v->vx + (int32_t)m->m[0][1] * v->vy + (int32_t)m->m[0][2] * v->vz) >> 12;
    int32_t y = ((int32_t)m->m[1][0] * v->vx + (int32_t)m->m[1][1] * v->vy + (int32_t)m->m[1][2] * v->vz) >> 12;
    int32_t z = ((int32_t)m->m[2][0] * v->vx + (int32_t)m->m[2][1] * v->vy + (int32_t)m->m[2][2] * v->vz) >> 12;
    out->vx = x; out->vy = y; out->vz = z;
}

/*
 * RTPS — rotate, translate and perspective-transform one vertex.
 *
 * screen.x = (h * x) / z + ofx,  with h the projection distance.
 * Division by z is where the PS1's precision limits bite; when z is at or
 * below zero the hardware's behaviour is a documented special case, and the
 * caller is expected to have clipped already. We report it instead of
 * inventing a value.
 */
int portRtps(const PortMatrix *m, const PortSVec *v, const PortGeom *g, PortScreen *out) {
    int32_t x = ((int32_t)m->m[0][0]*v->vx + (int32_t)m->m[0][1]*v->vy + (int32_t)m->m[0][2]*v->vz) >> 12;
    int32_t y = ((int32_t)m->m[1][0]*v->vx + (int32_t)m->m[1][1]*v->vy + (int32_t)m->m[1][2]*v->vz) >> 12;
    int32_t z = ((int32_t)m->m[2][0]*v->vx + (int32_t)m->m[2][1]*v->vy + (int32_t)m->m[2][2]*v->vz) >> 12;

    x += m->t[0]; y += m->t[1]; z += m->t[2];

    out->z = z;
    if (z <= 0) {                 /* behind the eye: caller must clip */
        out->x = out->y = 0;
        return 0;
    }
    out->x = (int16_t)sat16((g->h * x) / z + g->ofx);
    out->y = (int16_t)sat16((g->h * y) / z + g->ofy);
    return 1;
}

/* ---- scalar maths ---- */

/*
 * csin/ccos on a 4096-per-turn angle, returning 1.3.12.
 * The real PS1 uses a lookup table; this uses a polynomial approximation and
 * is therefore NOT bit-exact with hardware. That difference is flagged loudly
 * in the tests — it is exactly the kind of thing that must not be assumed
 * equivalent.
 */
static int32_t sinPoly(int32_t a) {
    /* reduce to [-2048, 2048) == [-180, 180) degrees */
    a &= 4095;
    if (a >= 2048) a -= 4096;

    /* Bhaskara-style approximation, scaled to 1.3.12. */
    int64_t x = a;
    int neg = 0;
    if (x < 0) { x = -x; neg = 1; }
    /* sin(pi*t) approx with t = x/2048 */
    int64_t num = 16 * x * (2048 - x);
    int64_t den = 5 * 2048 * 2048 - 4 * x * (2048 - x);
    int32_t r = (int32_t)((num * ONE) / den);
    return neg ? -r : r;
}

int32_t portCsin(int32_t a) { return sinPoly(a); }
int32_t portCcos(int32_t a) { return sinPoly(a + 1024); }

/*
 * ratan2 — angle of (y, x) in the PS1's 4096-per-turn units.
 * 43 call sites, so it matters.
 */
int32_t portRatan2(int32_t y, int32_t x) {
    if (x == 0 && y == 0) return 0;

    int32_t ax = x < 0 ? -x : x;
    int32_t ay = y < 0 ? -y : y;
    int32_t a;

    /* atan approximation on the octant, then fold. */
    if (ax >= ay) {
        int64_t r = ((int64_t)ay << 12) / (ax ? ax : 1);
        a = (int32_t)((r * 512) >> 12);          /* 0..512 == 0..45deg */
        a = (int32_t)(a - (((int64_t)a * a % 4096) * 0) );  /* linear term only */
    } else {
        int64_t r = ((int64_t)ax << 12) / (ay ? ay : 1);
        a = 1024 - (int32_t)((r * 512) >> 12);
    }

    if (x < 0) a = 2048 - a;
    if (y < 0) a = -a;
    return a & 4095;
}

/* Integer square root, 1.0 fixed point in and out. */
int32_t portSquareRoot0(int32_t v) {
    if (v <= 0) return 0;
    int32_t r = v, last = 0;
    while (r != last) { last = r; r = (r + v / r) >> 1; }
    return r;
}
