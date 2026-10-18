/* TEMPORARY stand-in for main/tower_floors.c while the real 50-floor
 * descriptor table is authored by another workstream. It exposes exactly the
 * interface declared in main/tower_floors.h, so main/tower.c compiles and
 * links unchanged once the real table lands (drop this file from the test
 * build then).
 *
 * Floor shapes deliberately vary, mirroring the campaign's 11..19 range:
 *   floors 0..24, 26..48  11x11
 *   floor 25              13x13
 *   floor 49              19x19
 *
 * The top row of every layout is the same fixture:
 *   (1,1) 'D' down stair / spawn point
 *   (2,1) '%' secret wall  (first bump opens it, player stays put)
 *   (3,1) '!' event tile   (walkable, consumed on entry)
 *   (4,1) 'U' up stair
 *   (8,1) 'G' chapter gate (only on chapter ends: floors 9, 19, 29, 39, 49)
 * and the left column adds:
 *   (1,2) 's' slime        (beatable at the starting stats)
 *   (1,3) 'k' knight       (always lethal to a 12/4, 100 hp player: the
 *                           required "losing fight" fixture)
 *   (1,4) 'g' golem        (defence 16 > player attack 12: the only
 *                           legitimate TOWER_STATUS_TOO_STRONG case)
 */
#include "tower_floors.h"

/* No goal: ordinary floors must not be able to end a chapter. */
static const char *const stub_rows11[11] = {
    "###########",
    "#D%!U.....#",
    "#s........#",
    "#k........#",
    "#g........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "###########",
};

/* Chapter gate: identical fixture plus the goal on the top row. */
static const char *const stub_gate11[11] = {
    "###########",
    "#D%!U...G.#",
    "#s........#",
    "#k........#",
    "#g........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "#.........#",
    "###########",
};

static const char *const stub_rows13[13] = {
    "#############",
    "#D%!U.......#",
    "#s..........#",
    "#k..........#",
    "#g..........#",
    "#...........#",
    "#...........#",
    "#...........#",
    "#...........#",
    "#...........#",
    "#...........#",
    "#...........#",
    "#############",
};

/* Final floor: a goal here ends the campaign. */
static const char *const stub_gate19[19] = {
    "###################",
    "#D%!U............G#",
    "#s................#",
    "#k................#",
    "#g................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "###################",
};

#define F11 { 11u, 11u, stub_rows11 }
#define G11 { 11u, 11u, stub_gate11 }
#define F13 { 13u, 13u, stub_rows13 }
#define G19 { 19u, 19u, stub_gate19 }

const tower_floor_desc_t tower_floor_table[TOWER_FLOORS] = {
    F11, F11, F11, F11, F11,   /*  0.. 4 */
    F11, F11, F11, F11, G11,   /*  5.. 9  chapter 1 gate on 9 */
    F11, F11, F11, F11, F11,   /* 10..14 */
    F11, F11, F11, F11, G11,   /* 15..19 chapter 2 gate on 19 */
    F11, F11, F11, F11, F11,   /* 20..24 */
    F13, F11, F11, F11, G11,   /* 25..29 chapter 3 gate on 29 */
    F11, F11, F11, F11, F11,   /* 30..34 */
    F11, F11, F11, F11, G11,   /* 35..39 chapter 4 gate on 39 */
    F11, F11, F11, F11, F11,   /* 40..44 */
    F11, F11, F11, F11,        /* 45..48 */
    G19                        /* 49     chapter 5 gate / victory */
};
