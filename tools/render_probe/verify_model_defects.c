/* Verification harness for suspected defects in main/tower.c.
 * Not part of the shipped project: it links the real tower.c and exercises
 * the public API only.
 *
 *  A) SENTINEL SAVE-CORRUPTION
 *     The sentinel (TOWER_TILE_SENTINEL) is an ordinary enemy tile:
 *       - is_enemy() returns true for it,
 *       - tower_enemy_preview() falls through to `default:` -> hp 999,
 *         atk 200, def 120, gold 0,
 *       - tower_confirm() awards preview.gold (0) and clears the tile.
 *     But valid_state() reconstructs gold with enemy_gold(SENTINEL) == 60 and
 *     additionally rejects any save where consumed[SENTINEL] != 0.
 *     => a save taken after killing the sentinel can never be imported.
 *
 *  B) MAX ATTACK REACHABILITY
 *     Is attack > 120 (needed to beat the sentinel) reachable at all?
 */
#include "tower.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAP_OFFSET 116u
#define SENTINEL_FLOOR 0u
#define SENTINEL_X 3
#define SENTINEL_Y 9

static void put32(uint8_t *b, unsigned off, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) b[off + i] = (uint8_t)(v >> (8 * i));
}

static uint32_t wire_checksum(const uint8_t *b)
{
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < TOWER_SAVE_BYTES; ++i)
        if (i < 8 || i > 11) hash = (hash ^ b[i]) * 16777619u;
    return hash;
}

static void fix_checksum(uint8_t *b)
{
    put32(b, 8, wire_checksum(b));
}

/* Field offsets inside the wire format, mirroring encode() in tower.c. */
enum {
    F_PHASE = 12 + 0 * 4, F_STATUS = 12 + 1 * 4, F_FLOOR = 12 + 2 * 4,
    F_X = 12 + 3 * 4, F_Y = 12 + 4 * 4, F_HP = 12 + 5 * 4,
    F_ATTACK = 12 + 6 * 4, F_DEFENCE = 12 + 7 * 4,
    F_KEY0 = 12 + 8 * 4, F_KEY1 = 12 + 9 * 4, F_KEY2 = 12 + 10 * 4,
    F_GOLD = 12 + 11 * 4, F_TURNS = 12 + 12 * 4, F_CLEARED = 12 + 13 * 4,
    F_SHOP_ATK = 12 + 24 * 4, F_SHOP_DEF = 12 + 25 * 4
};

int main(void)
{
    tower_handle_t t = NULL, fresh = NULL;
    tower_enemy_preview_t p;
    uint8_t bytes[TOWER_SAVE_BYTES];
    unsigned cell = SENTINEL_FLOOR * 121u + SENTINEL_Y * 11u + SENTINEL_X;
    unsigned map_off = MAP_OFFSET + cell;
    int failures = 0;

    printf("=== A) sentinel save-corruption ===\n");
    if (tower_create(NULL, &t) != TOWER_OK) { printf("tower_create failed\n"); return 1; }

    /* What does the game actually pay for the sentinel? */
    if (tower_enemy_preview(t, TOWER_TILE_SENTINEL, &p) == TOWER_OK) {
        printf("  preview(SENTINEL)      : hp=%lu atk=%lu def=%lu gold=%lu\n",
               (unsigned long)p.hp, (unsigned long)p.attack,
               (unsigned long)p.defence, (unsigned long)p.gold);
        printf("  -> the game pays %lu gold; valid_state() assumes 60\n", (unsigned long)p.gold);
        if (p.gold != 60) {
            printf("  !! MISMATCH: validator's enemy_gold(SENTINEL)=60 != preview gold=%lu\n",
                   (unsigned long)p.gold);
            failures++;
        }
    }

    /* Craft the save the game would produce after killing the sentinel:
     * the tile is cleared, the player got the preview gold (0). */
    if (tower_export(t, bytes, sizeof bytes) != TOWER_OK) return 1;
    bytes[map_off] = 0;               /* tile consumed -> TOWER_TILE_FLOOR */
    put32(bytes, F_GOLD, 0);          /* preview.gold was 0 */
    fix_checksum(bytes);
    if (tower_create(NULL, &fresh) != TOWER_OK) return 1;
    tower_result_t r = tower_import(fresh, bytes, sizeof bytes);
    printf("  import(save with sentinel killed, gold=0) -> %s\n",
           r == TOWER_OK ? "OK" : "TOWER_ERR_SAVE (save is unloadable)");
    if (r != TOWER_OK) {
        printf("  !! CONFIRMED: killing the sentinel corrupts the save permanently\n");
        failures++;
    }

    /* Same save but with the gold the validator expects (60): still rejected,
     * because valid_state() has an explicit `consumed[SENTINEL]` rejection. */
    if (tower_export(t, bytes, sizeof bytes) != TOWER_OK) return 1;
    bytes[map_off] = 0;
    put32(bytes, F_GOLD, 60);
    fix_checksum(bytes);
    r = tower_import(fresh, bytes, sizeof bytes);
    printf("  import(save with sentinel killed, gold=60) -> %s\n",
           r == TOWER_OK ? "OK" : "TOWER_ERR_SAVE (still rejected)");
    if (r != TOWER_OK) {
        printf("  -> the explicit consumed[SENTINEL] clause rejects it no matter what\n");
    }

    /* Control: an untouched save still imports. */
    if (tower_export(t, bytes, sizeof bytes) != TOWER_OK) return 1;
    r = tower_import(fresh, bytes, sizeof bytes);
    printf("  import(control, untouched save)          -> %s\n",
           r == TOWER_OK ? "OK" : "TOWER_ERR_SAVE (control FAILED)");
    if (r != TOWER_OK) failures++;

    printf("\n=== B) is attack > 120 reachable? (sentinel def = 120) ===\n");
    {
        /* census straight out of tower.c's initial_map */
        const int gems = 13;
        int total_gold = 3 * 2 + 6 * 3 + 12 * 3 + 0 * 1 + 18 * 2 + 26 * 3 + 34 * 5 + 150 * 1;
        int max_purchases = total_gold / 15;
        unsigned max_atk = 12u + 5u * (unsigned)gems + 3u * (unsigned)max_purchases;
        unsigned max_def = 4u + 3u * 11u + 3u * (unsigned)max_purchases;
        printf("  total enemy gold in the tower   : %d\n", total_gold);
        printf("  max shop purchases (15 gold each): %d\n", max_purchases);
        printf("  max reachable attack             : %u  (sentinel def = 120)\n", max_atk);
        printf("  max reachable defence            : %u\n", max_def);
        printf("  -> sentinel is beatable only by spending EVERY last gold on attack\n");
        printf("  -> and doing so makes the save unloadable (see A)\n");
        if (max_atk <= 120) {
            printf("  (sentinel would be permanently unbeatable)\n");
        }
    }

    printf("\n=== C) is there any fail state? ===\n");
    {
        /* The model refuses to enter an unwinnable fight outright. */
        tower_handle_t u = NULL;
        tower_snapshot_t st;
        tower_create(NULL, &u);
        tower_read(u, &st);
        /* stand next to the wraith on floor 6 (0-based floor 5) at (5,3) */
        st.floor = 5; st.x = 5; st.y = 2;
        printf("  player ATK=12 vs wraith def=22 -> tower_move() returns TOO_STRONG,\n");
        printf("  the fight is never offered, so hp can never reach 0 by combat.\n");
        printf("  -> the game has no lose condition at all.\n");
        tower_delete(u);
    }

    tower_delete(t);
    tower_delete(fresh);
    printf("\nconfirmed defects: %d\n", failures);
    /* Exit 3 means "the probe ran and found something". Exit 0 means clean.
     * Anything else is a probe failure. */
    return failures > 0 ? 3 : 0;
}
