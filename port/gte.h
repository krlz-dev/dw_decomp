/*
 * gte.h — software GTE for the port.
 *
 * Plain C types only, same fence as gpu_sdl.h: this never sees the game's
 * 32-bit MIPS typedefs, and the glue layer converts.
 */
#ifndef PORT_GTE_H
#define PORT_GTE_H

#include <stdint.h>

typedef struct { int16_t vx, vy, vz, pad; } PortSVec;
typedef struct { int32_t vx, vy, vz, pad; } PortVec;
typedef struct { int16_t m[3][3]; int32_t t[3]; } PortMatrix;

/* Projection parameters: h is the projection distance, ofx/ofy the screen
   offset — what SetGeomScreen/SetGeomOffset configure on hardware. */
typedef struct { int32_t h, ofx, ofy; } PortGeom;

typedef struct { int16_t x, y; int32_t z; } PortScreen;

void portRotMatrixZYX(const PortSVec *rot, PortMatrix *m);
void portScaleMatrix(PortMatrix *m, const PortVec *scale);
void portTransMatrix(PortMatrix *m, const PortVec *t);
void portMulMatrix0(const PortMatrix *a, const PortMatrix *b, PortMatrix *out);

void portApplyMatrixSV(const PortMatrix *m, const PortSVec *v, PortSVec *out);
void portApplyMatrixLV(const PortMatrix *m, const PortVec *v, PortVec *out);

/* Returns 0 when the vertex is at or behind the eye (caller must clip). */
int  portRtps(const PortMatrix *m, const PortSVec *v, const PortGeom *g, PortScreen *out);

int32_t portCsin(int32_t a);      /* 4096 units per turn, returns 1.3.12 */
int32_t portCcos(int32_t a);
int32_t portRatan2(int32_t y, int32_t x);
int32_t portSquareRoot0(int32_t v);

#endif
