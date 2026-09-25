/*
 * run_gte_error.c — does the approximation error actually matter?
 *
 * "0.17% trig error" is not a useful number. The useful number is: how many
 * PIXELS does a vertex land away from where the hardware would have put it?
 *
 * This projects a cloud of vertices through the software GTE and through
 * double-precision maths, and reports the screen-space difference. A port
 * fails not when the maths is imperfect but when the imperfection is visible.
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "gte.h"

#define ONE 4096
#define SCREEN_W 320
#define SCREEN_H 240

/* Reference projection in doubles: what the geometry SHOULD be. */
static void refProject(double rx, double ry, double rz,
                       double x, double y, double z,
                       double tz, double h, double ofx, double ofy,
                       double *sx, double *sy)
{
    double cx = cos(rx), sxr = sin(rx);
    double cy = cos(ry), syr = sin(ry);
    double cz = cos(rz), szr = sin(rz);

    /* ZYX composition, same order as portRotMatrixZYX */
    double m00 = cz*cy;
    double m01 = cz*syr*sxr - szr*cx;
    double m02 = cz*syr*cx + szr*sxr;
    double m10 = szr*cy;
    double m11 = szr*syr*sxr + cz*cx;
    double m12 = szr*syr*cx - cz*sxr;
    double m20 = -syr;
    double m21 = cy*sxr;
    double m22 = cy*cx;

    double px = m00*x + m01*y + m02*z;
    double py = m10*x + m11*y + m12*z;
    double pz = m20*x + m21*y + m22*z + tz;

    *sx = h * px / pz + ofx;
    *sy = h * py / pz + ofy;
}

int main(void) {
    printf("\nGTE accuracy in SCREEN PIXELS\n");
    printf("=============================\n\n");
    printf("A 0.17%% trig error is meaningless on its own. What matters is how\n");
    printf("far a projected vertex lands from where it should.\n\n");

    PortGeom geom = { .h = 300, .ofx = SCREEN_W/2, .ofy = SCREEN_H/2 };

    double worst = 0, sum = 0;
    int samples = 0, offByOne = 0, offByTwo = 0, worstAngle = 0;

    /* sweep rotations and a spread of model-space vertices */
    for (int a = 0; a < 4096; a += 13) {
        PortSVec rot = { (int16_t)(a/3), (int16_t)a, (int16_t)(a/5), 0 };
        PortMatrix M;
        portRotMatrixZYX(&rot, &M);
        portTransMatrix(&M, &(PortVec){0, 0, 800, 0});

        static const int pts[][3] = {
            {100,0,0}, {0,100,0}, {0,0,100}, {80,80,80},
            {-120,40,60}, {200,-90,30}, {-50,-50,-50}, {300,10,-20},
        };

        for (unsigned i = 0; i < sizeof(pts)/sizeof(pts[0]); i++) {
            PortSVec v = { (int16_t)pts[i][0], (int16_t)pts[i][1], (int16_t)pts[i][2], 0 };
            PortScreen s;
            if (!portRtps(&M, &v, &geom, &s)) continue;

            double rx = 2*M_PI*(a/3)/4096.0;
            double ry = 2*M_PI*a/4096.0;
            double rz = 2*M_PI*(a/5)/4096.0;
            double ex, ey;
            refProject(rx, ry, rz, pts[i][0], pts[i][1], pts[i][2],
                       800, geom.h, geom.ofx, geom.ofy, &ex, &ey);

            double dx = s.x - ex, dy = s.y - ey;
            double d = sqrt(dx*dx + dy*dy);

            sum += d; samples++;
            if (d > worst) { worst = d; worstAngle = a; }
            if (d > 1.0) offByOne++;
            if (d > 2.0) offByTwo++;
        }
    }

    printf("samples projected:        %d\n", samples);
    printf("mean error:               %.3f px\n", sum / samples);
    printf("worst error:              %.3f px (at angle %d)\n", worst, worstAngle);
    printf("vertices off by > 1 px:   %d  (%.2f%%)\n", offByOne, 100.0*offByOne/samples);
    printf("vertices off by > 2 px:   %d  (%.2f%%)\n\n", offByTwo, 100.0*offByTwo/samples);

    /* The honest verdict, stated as a threshold rather than a feeling. */
    if (worst < 1.0) {
        printf("VERDICT: sub-pixel across the whole sweep. Visually indistinguishable.\n");
    } else if (worst < 2.0) {
        printf("VERDICT: worst case is ~1 px. Acceptable for a port; NOT bit-exact,\n");
        printf("         so it cannot be used to validate a matching decomp.\n");
    } else {
        printf("VERDICT: errors exceed 2 px. Visible wobble on animated geometry —\n");
        printf("         a lookup-table implementation is required.\n");
    }
    printf("\nEither way this is NOT hardware-exact: the real GTE uses lookup\n");
    printf("tables, and matching it bit-for-bit needs those tables.\n\n");
    return 0;
}
