/*
 * libgte.h — portable stand-in for Sony's PSY-Q GTE header.
 *
 * The GTE is the PS1's fixed-point geometry coprocessor. This file provides
 * ONLY the data types, which is all that game headers embed. No GTE maths is
 * implemented here: the transform functions are a separate, much harder job
 * (see docs) and nothing in the game-logic layer calls them.
 *
 * Layout matters. These structs are memcpy'd out of game data files, so field
 * order and sizes must match the original exactly or every table read is
 * garbage. Verified with static_assert at the bottom.
 */
#ifndef PORT_LIBGTE_H
#define PORT_LIBGTE_H

#include <dw/types.h>

/* 16-bit vector — the workhorse for positions and rotations. 8 bytes. */
typedef struct {
    int16_t vx, vy;
    int16_t vz, pad;
} SVECTOR;

/* 32-bit vector. 16 bytes. */
typedef struct {
    int32_t vx, vy;
    int32_t vz, pad;
} VECTOR;

/* Unsigned colour vector. 4 bytes. */
typedef struct {
    uint8_t r, g, b, cd;
} CVECTOR;

/* 3x3 fixed-point rotation + 32-bit translation. 32 bytes. */
typedef struct {
    int16_t m[3][3];
    int32_t t[3];
} MATRIX;

#ifdef __cplusplus
#define PORT_STATIC_ASSERT(c, m) static_assert(c, m)
#else
#define PORT_STATIC_ASSERT(c, m) _Static_assert(c, m)
#endif

PORT_STATIC_ASSERT(sizeof(SVECTOR) == 8,  "SVECTOR must stay 8 bytes");
PORT_STATIC_ASSERT(sizeof(VECTOR)  == 16, "VECTOR must stay 16 bytes");
PORT_STATIC_ASSERT(sizeof(CVECTOR) == 4,  "CVECTOR must stay 4 bytes");
PORT_STATIC_ASSERT(sizeof(MATRIX)  == 32, "MATRIX must stay 32 bytes");

#endif /* PORT_LIBGTE_H */
