/*
 * run_evolution.c — executes the REAL decompiled evolution code on x86.
 *
 * This links against src/main/evolution.c unmodified. Every number printed
 * comes out of the actual game function, reading the actual game tables.
 */
extern int printf(const char *, ...);

#include <dw/types.h>
#include <dw/entity.h>
#include <dw/evolution.h>
#include <dw/evl.h>
#include <dw/partner.h>

extern PartnerEntity PARTNER_ENTITY;
extern EvoRequirements EVO_REQ_DATA[63];
extern EvolutionPath EVO_PATHS_DATA[62];
extern DigimonPara DIGIMON_DATA[180];
extern PartnerPara PARTNER_PARA;
int32_t getNumMasteredMoves(void);

static int failures = 0;

static void check(const char *what, int got, int want) {
    if (got == want) {
        printf("  ok    %-46s %d\n", what, got);
    } else {
        printf("  FAIL  %-46s got %d want %d\n", what, got, want);
        failures++;
    }
}

/* Load stats into the global the evolution code reads. */
static void setStats(int16_t hp, int16_t mp, int16_t off, int16_t def,
                     int16_t speed, int16_t brain) {
    BaseStats *b = &PARTNER_ENTITY.digimonEntity.stats.base;
    b->hp = hp; b->mp = mp; b->off = off;
    b->def = def; b->speed = speed; b->brain = brain;
}

int main(void) {
    printf("\n");
    printf("Digimon World — REAL evolution.c running on x86-64\n");
    printf("==================================================\n\n");

    printf("Data tables linked from the decomp:\n");
    printf("  EVO_REQ_DATA   %3d entries\n", (int)(sizeof(EVO_REQ_DATA) / sizeof(EVO_REQ_DATA[0])));
    printf("  EVO_PATHS_DATA %3d entries\n", (int)(sizeof(EVO_PATHS_DATA) / sizeof(EVO_PATHS_DATA[0])));
    printf("  EvoRequirements = %d bytes (PS1: 28)\n\n", (int)sizeof(EvoRequirements));

    /* --- the tables are real game data, not zeros --- */
    printf("Sanity: the tables carry real values\n");
    check("EVO_REQ_DATA[5].offense is 100", EVO_REQ_DATA[5].offense, 100);
    check("EVO_REQ_DATA[5].care is 1",      EVO_REQ_DATA[5].care, 1);
    check("EVO_REQ_DATA[12].hp is 400",     EVO_REQ_DATA[12].hp, 400);

    /* --- calling the REAL functions --- */
    printf("\nCalling getRookieEvolutionTarget() — the real function\n");

    /* calculateRequirementScore() also reads PARTNER_PARA (care mistakes and
     * weight) and DIGIMON_DATA[target].level — all of which the game loads
     * from disc. With those zeroed, weight fails the "within ±5 of the
     * requirement" test for every candidate, so the function correctly returns
     * -1. Feeding realistic values exercises the real decision path. */
    DIGIMON_DATA[0x1a].level = 3;
    DIGIMON_DATA[0x36].level = 3;
    PARTNER_PARA.careMistakes = 0;
    PARTNER_PARA.weight = EVO_REQ_DATA[0x1a].weight;  /* on target */

    printf("  (target 0x1a wants weight %d, care <= %d)\n",
           EVO_REQ_DATA[0x1a].weight, EVO_REQ_DATA[0x1a].care);

    setStats(500, 400, 200, 200, 200, 200);
    int16_t strong = getRookieEvolutionTarget(8);
    printf("  strong partner (id 8)  -> evolves into %d\n", strong);

    setStats(20, 20, 5, 5, 5, 5);
    int16_t weak = getRookieEvolutionTarget(8);
    printf("  weak partner   (id 8)  -> evolves into %d\n", weak);

    check("the function is deterministic",
          getRookieEvolutionTarget(8) == weak, 1);

    printf("\nCalling getFreshEvolutionTarget() and getInTrainingEvolutionTarget()\n");
    setStats(300, 300, 150, 150, 150, 150);
    printf("  fresh(1)       -> %d\n", getFreshEvolutionTarget(1));
    printf("  in-training(3) -> %d\n", getInTrainingEvolutionTarget(3));

    printf("\nCalling getNumMasteredMoves()\n");
    int32_t moves = getNumMasteredMoves();
    printf("  mastered moves -> %d\n", moves);
    check("returns a sane count", moves >= 0 && moves <= 64, 1);

    printf("\n%s\n\n", failures == 0
        ? "PASSED — decompiled game logic executes unmodified off the PS1."
        : "FAILED");
    return failures == 0 ? 0 : 1;
}
