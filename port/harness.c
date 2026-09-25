/*
 * harness.c — the minimum needed to LINK and RUN the real evolution.c on x86.
 *
 * src/main/evolution.c compiles clean against the portable headers in
 * port/include, but still references 18 external symbols: the game's global
 * state plus a few functions that live in other translation units.
 *
 * This file provides them so the ACTUAL decompiled evolution code executes off
 * the PlayStation. Nothing in evolution.c is modified.
 *
 * Every signature here was read from the decomp (include/dw/*.h and the
 * declarations at the top of evolution.c) — none were guessed.
 *
 * Stub policy: anything not needed to exercise evolution logic aborts loudly
 * if called, instead of returning a plausible value. A silent wrong answer
 * would make the whole experiment worthless.
 */
/* Deliberately NOT including <stdio.h>/<stdlib.h>.
 *
 * The decomp targets 32-bit freestanding MIPS: dw/types.h defines its own
 * int8_t, and dw/math.h declares random(int32_t) — which collides with glibc's
 * random(void). Rather than fight the system headers with macro tricks, the
 * harness declares the three libc functions it needs and lets the game's own
 * definitions win everywhere else. This is exactly the kind of friction a real
 * port has to settle, so it is worth settling cleanly. */
extern int printf(const char *, ...);
extern int fprintf(void *, const char *, ...);
extern void abort(void);
extern void *stderr;

#include <dw/types.h>
#include <dw/entity.h>
#include <dw/evolution.h>
#include <dw/params.h>
#include <dw/partner.h>
#include <dw/tamer.h>

/* ---- globals, with the exact types the decomp declares ---- */

PartnerEntity PARTNER_ENTITY;
PartnerPara   PARTNER_PARA;
DigimonPara   DIGIMON_DATA[180];

/* Array of POINTERS, matching include/dw/entity.h:133 exactly. */
Entity *ENTITY_TABLE[ENTITY_MAX];

uint8_t  CURRENT_SCREEN;
int32_t  NANIMON_TRIGGER;
int16_t  EVOLUTION_TARGET;
Stats    DEATH_STATS;

/* ---- tracked stubs ---- */

static int8_t raised_flags[256];
static int8_t tamer_state;
static int8_t partner_state;

int32_t hasDigimonRaised(int32_t digimonId) {
    return (digimonId >= 0 && digimonId < 256) ? raised_flags[digimonId] : 0;
}

void setDigimonRaised(int32_t type) {
    if (type >= 0 && type < 256) raised_flags[type] = 1;
}

int32_t getTamerState(void)        { return tamer_state; }
void    setTamerState(int8_t s)    { tamer_state = s; }
void    setPartnerState(int8_t s)  { partner_state = s; }

/* Deterministic PRNG. The original is the PS1's; a fixed sequence keeps the
   test reproducible. Signature matches dw/math.h: random(limit). */
static uint32_t rng_state = 1;
int32_t random(int32_t limit) {
    rng_state = rng_state * 1103515245u + 12345u;
    int32_t r = (int32_t)((rng_state >> 16) & 0x7fff);
    return limit > 0 ? r % limit : 0;
}

/* Not exercised by the paths under test. Abort rather than lie. */
static void unimplemented(const char *name) {
    fprintf(stderr, "\n[harness] %s() called but not implemented.\n", name);
    fprintf(stderr, "[harness] refusing to fake it — the result would be meaningless.\n");
    abort();
}

void initializeReincarnatedPartner(int32_t type, int32_t posX, int32_t posY,
                                   int32_t posZ, int32_t rotX, int32_t rotY,
                                   int32_t rotZ) {
    (void)type; (void)posX; (void)posY; (void)posZ;
    (void)rotX; (void)rotY; (void)rotZ;
    unimplemented("initializeReincarnatedPartner");
}

void removeEntity(int32_t objectId, int32_t entityId) {
    (void)objectId; (void)entityId;
    unimplemented("removeEntity");
}

void thunkUnloadModel(int32_t digiType, int32_t modelType) {
    (void)digiType; (void)modelType;
    unimplemented("thunkUnloadModel");
}
