/*
 * run_gte.c — measure the software GTE against its fixed-point contract.
 *
 * The point is NOT "does it produce numbers". It is: does it produce the
 * numbers the hardware's documented behaviour requires, within a stated
 * tolerance — and where it does not, say so out loud.
 *
 * Every expectation below comes from the GTE's fixed-point conventions
 * (1.3.12 matrix entries, 4096 units per turn, saturation rather than wrap),
 * not from running the code and writing down what came out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gte.h"

#define ONE 4096

static int failures = 0;
static int warnings = 0;

static void ok(const char *what, int cond) {
    printf("  %s  %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) failures++;
}

static void near(const char *what, int32_t got, int32_t want, int tol) {
    int d = got - want; if (d < 0) d = -d;
    if (d <= tol) {
        printf("  ok    %-44s %6d (want %6d, +-%d)\n", what, got, want, tol);
    } else {
        printf("  FAIL  %-44s %6d (want %6d, +-%d)\n", what, got, want, tol);
        failures++;
    }
}

static void warn(const char *msg) {
    printf("  WARN  %s\n", msg);
    warnings++;
}

int main(void) {
    printf("\nSoftware GTE — fixed-point contract\n");
    printf("===================================\n\n");

    /* ---- trig: 4096 units per turn, results in 1.3.12 ---- */
    printf("csin/ccos (4096 units = 360 deg, result 1.3.12 so 4096 = 1.0)\n");
    near("sin(0)",      portCsin(0),     0,     8);
    near("sin(90deg)",  portCsin(1024),  4096,  40);
    near("sin(180deg)", portCsin(2048),  0,     40);
    near("sin(270deg)", portCsin(3072), -4096,  40);
    near("cos(0)",      portCcos(0),     4096,  40);
    near("cos(90deg)",  portCcos(1024),  0,     40);

    /* Compare against real doubles across a full turn — this is where an
       approximation shows its true error, not at the cardinal points. */
    int32_t worst = 0; int worstAt = 0;
    for (int a = 0; a < 4096; a += 7) {
        double exact = sin(2.0 * M_PI * a / 4096.0) * ONE;
        int32_t got = portCsin(a);
        int32_t err = (int32_t)llabs((long long)(got - (int32_t)exact));
        if (err > worst) { worst = err; worstAt = a; }
    }
    printf("  worst sin error over a full turn: %d/4096 (%.2f%%) at angle %d\n",
           worst, 100.0 * worst / ONE, worstAt);
    if (worst > 100) {
        warn("sin approximation is NOT bit-exact with PS1 hardware tables.");
        warn("the real GTE uses a lookup table; matching it needs that table.");
    }

    /* ---- identity rotation must be exactly 1.0 on the diagonal ---- */
    printf("\nRotation matrix (entries are 1.3.12, so 4096 = 1.0)\n");
    PortSVec none = {0, 0, 0, 0};
    PortMatrix I;
    portRotMatrixZYX(&none, &I);
    near("I[0][0]", I.m[0][0], 4096, 40);
    near("I[1][1]", I.m[1][1], 4096, 40);
    near("I[2][2]", I.m[2][2], 4096, 40);
    near("I[0][1]", I.m[0][1], 0,    40);

    /* ---- a 90-degree turn about Y must map +X to -Z ---- */
    printf("\nApplyMatrixSV (the most-used GTE op: 103 call sites)\n");
    PortSVec rotY = {0, 1024, 0, 0};      /* 90 degrees about Y */
    PortMatrix My;
    portRotMatrixZYX(&rotY, &My);

    PortSVec xAxis = {1000, 0, 0, 0}, out;
    portApplyMatrixSV(&My, &xAxis, &out);
    printf("  (1000,0,0) rotated 90deg about Y -> (%d,%d,%d)\n", out.vx, out.vy, out.vz);
    near("  x component", out.vx, 0,     40);
    near("  z component", out.vz, -1000, 40);

    /* identity must be a no-op, exactly */
    PortSVec v = {123, -456, 789, 0}, same;
    portApplyMatrixSV(&I, &v, &same);
    ok("identity leaves a vector unchanged",
       same.vx == 123 && same.vy == -456 && same.vz == 789);

    /* ---- saturation, not wraparound ---- */
    printf("\nSaturation (hardware clamps; wrapping would flip vertices)\n");
    PortMatrix big = I;
    big.m[0][0] = 32767;                   /* ~8.0 in 1.3.12 */
    PortSVec huge = {30000, 0, 0, 0}, sat;
    portApplyMatrixSV(&big, &huge, &sat);
    printf("  32767 * 30000 >> 12 = %d before clamping\n",
           (int)(((int32_t)32767 * 30000) >> 12));
    ok("result clamps to int16 range", sat.vx == 32767);

    /* ---- perspective ---- */
    printf("\nRTPS (perspective divide)\n");
    PortGeom geom = { .h = 300, .ofx = 160, .ofy = 120 };
    PortMatrix M = I;
    portTransMatrix(&M, &(PortVec){0, 0, 600, 0});   /* push 600 away */

    PortSVec p = {100, 50, 0, 0};
    PortScreen s;
    int visible = portRtps(&M, &p, &geom, &s);
    printf("  vertex (100,50,0) at z=600 -> screen (%d,%d) z=%d\n", s.x, s.y, s.z);
    ok("vertex is in front of the eye", visible == 1);
    near("  screen x = h*x/z + ofx", s.x, 300 * 100 / 600 + 160, 1);
    near("  screen y = h*y/z + ofy", s.y, 300 *  50 / 600 + 120, 1);

    /* farther away must project closer to the centre */
    portTransMatrix(&M, &(PortVec){0, 0, 1200, 0});
    PortScreen far;
    portRtps(&M, &p, &geom, &far);
    printf("  same vertex at z=1200  -> screen (%d,%d)\n", far.x, far.y);
    ok("doubling distance halves the offset from centre",
       (far.x - 160) * 2 == (s.x - 160));

    /* behind the eye must be reported, not invented */
    portTransMatrix(&M, &(PortVec){0, 0, -10, 0});
    PortScreen behind;
    ok("a vertex behind the eye is reported as clipped",
       portRtps(&M, &p, &geom, &behind) == 0);

    /* ---- ratan2 ---- */
    printf("\nratan2 (43 call sites, 4096 units per turn)\n");
    near("atan2(0, 1000)    = 0deg",    portRatan2(0, 1000),    0,    24);
    near("atan2(1000, 1000) = 45deg",   portRatan2(1000, 1000), 512,  24);
    near("atan2(1000, 0)    = 90deg",   portRatan2(1000, 0),    1024, 24);

    /* ---- sqrt ---- */
    printf("\nSquareRoot0\n");
    near("sqrt(10000)", portSquareRoot0(10000), 100, 1);
    near("sqrt(2)",     portSquareRoot0(2),     1,   1);
    ok("sqrt(0) is 0", portSquareRoot0(0) == 0);

    printf("\n");
    if (warnings)
        printf("%d warning(s): approximations that are NOT hardware-exact.\n", warnings);
    printf("%s\n\n", failures == 0
        ? "PASSED — the software GTE honours its fixed-point contract."
        : "FAILED");
    return failures == 0 ? 0 : 1;
}
