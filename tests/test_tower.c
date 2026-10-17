#include "tower.h"
#include "win_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static tower_snapshot_t read_state(tower_handle_t t)
{
    tower_snapshot_t s;
    assert(tower_read(t, &s) == TOWER_OK);
    return s;
}

static uint32_t checksum(const uint8_t *b)
{
    uint32_t hash = 2166136261u;
    unsigned i;
    for (i = 0; i < TOWER_SAVE_BYTES; ++i)
        if (i < 8 || i > 11) hash = (hash ^ b[i]) * 16777619u;
    return hash;
}

static void fix_checksum(uint8_t *b)
{
    uint32_t h = checksum(b);
    unsigned i;
    for (i = 0; i < 4; ++i) b[8 + i] = (uint8_t)(h >> (8 * i));
}

int main(void)
{
    tower_handle_t t = NULL, other = NULL;
    tower_snapshot_t s;
    tower_enemy_preview_t p;
    uint8_t saved[TOWER_SAVE_BYTES], changed[TOWER_SAVE_BYTES];
    uint32_t initial, hash;
    unsigned i;

    assert(tower_create(NULL, &t) == TOWER_OK && t);
    assert(tower_create(NULL, &other) == TOWER_OK && other);
    initial = tower_hash(t);
    assert(tower_export(t, saved, sizeof saved) == TOWER_OK);
    assert(tower_import(other, saved, sizeof saved) == TOWER_OK);
    assert(tower_hash(other) == initial);

    s = read_state(t);
    assert(s.x == 5 && s.y == 9 && s.hp == 100 && s.attack == 12 && s.defence == 4);
    assert(s.floor == 0 && !s.cleared && !s.gold);
    assert(tower_tile(t, 0, 5, 1) == TOWER_TILE_UP);
    assert(tower_tile(t, 9, 5, 1) == TOWER_TILE_GOAL);
    assert(tower_tile(t, 2, 1, 4) == TOWER_TILE_KEY_RED);
    assert(tower_tile(t, 4, 5, 5) == TOWER_TILE_DOOR_RED);
    assert(tower_tile(t, 2, 4, 8) == TOWER_TILE_DOOR_BLUE);
    assert(tower_tile(t, 3, 8, 7) == TOWER_TILE_DOOR_BLUE);
    assert(tower_tile(t, 3, 3, 3) == TOWER_TILE_DOOR_RED);
    assert(tower_tile(t, 6, 9, 5) == TOWER_TILE_KEY_RED);
    assert(tower_tile(t, 7, 5, 3) == TOWER_TILE_DOOR_RED);
    assert(tower_tile(t, 9, 5, 3) == TOWER_TILE_DEMON);
    /* A guardian must be fought to collect its reward pocket. */
    assert(tower_tile(t, 5, 3, 5) == TOWER_TILE_MAGE);
    assert(tower_tile(t, 5, 2, 5) == TOWER_TILE_DEFENCE);
    assert(tower_tile(t, 5, 7, 5) == TOWER_TILE_GOLEM);
    assert(tower_tile(t, 5, 8, 5) == TOWER_TILE_ATTACK);
    assert(tower_tile(t, 8, 3, 5) == TOWER_TILE_GOLEM);
    assert(tower_tile(t, 8, 2, 5) == TOWER_TILE_DEFENCE);
    assert(tower_tile(t, 8, 7, 5) == TOWER_TILE_WRAITH);
    assert(tower_tile(t, 8, 8, 5) == TOWER_TILE_ATTACK);
    assert(tower_move(t, 0, 1) == TOWER_STATUS_WALL);
    assert(tower_move(t, 1, 1) == TOWER_STATUS_WALL);
    assert(read_state(t).turns == 0);

    assert(tower_enemy_preview(t, TOWER_TILE_WRAITH, &p) == TOWER_OK);
    assert(p.hp == 260 && p.attack == 36 && p.defence == 22 && p.gold == 34);
    assert(tower_enemy_preview(t, TOWER_TILE_DEMON, &p) == TOWER_OK);
    assert(p.hp == 700 && p.attack == 58 && p.defence == 46 && p.gold == 150);
    assert(tower_enemy_preview(t, TOWER_TILE_SENTINEL, &p) == TOWER_OK);
    assert(!p.winnable && p.damage == UINT32_MAX);

    /* First floor: potion, intro NPC, and a real fight. */
    assert(tower_move(t, 0, -1) == TOWER_STATUS_ITEM);
    assert(read_state(t).hp == 130);
    assert(tower_move(t, 0, 1) == TOWER_STATUS_MOVED);
    assert(tower_move(t, 1, 0) == TOWER_STATUS_TALK);
    assert(read_state(t).x == 6 && read_state(t).y == 9);
    assert(tower_move(t, -1, 0) == TOWER_STATUS_MOVED);
    assert(tower_move(t, 0, -1) == TOWER_STATUS_MOVED);

    /* Full ten-floor route generated from the shipped map data. */
    tower_new(t);
    for (i = 0; i < WIN_ROUTE_COUNT; ++i) {
        signed char a = WIN_ROUTE[i];
        if (a == 6) {
            if (read_state(t).phase == TOWER_PHASE_COMBAT) (void)tower_confirm(t);
            continue;
        }
        {
            const int dx = a == 0 ? -1 : a == 1 ? 1 : 0;
            const int dy = a == 2 ? -1 : a == 5 ? 1 : 0;
            tower_status_t r = tower_move(t, dx, dy);
            assert(r != TOWER_STATUS_WALL && r != TOWER_STATUS_NEED_KEY &&
                   r != TOWER_STATUS_TOO_STRONG);
            if (r == TOWER_STATUS_CONFIRM) (void)tower_confirm(t);
        }
    }
    s = read_state(t);
    assert(s.floor == WIN_EXPECT_FLOOR && s.x == WIN_EXPECT_X && s.y == WIN_EXPECT_Y);
    assert(s.hp == WIN_EXPECT_HP && s.attack == WIN_EXPECT_ATTACK);
    assert(s.defence == WIN_EXPECT_DEFENCE && s.gold == WIN_EXPECT_GOLD);
    assert(s.turns == WIN_EXPECT_TURNS && s.cleared);
    hash = tower_hash(t);
    assert(tower_export(t, saved, sizeof saved) == TOWER_OK);
    assert(tower_import(other, saved, sizeof saved) == TOWER_OK);
    assert(tower_hash(other) == hash);

    /* Corruption of any byte in the 2 KiB record is rejected transactionally. */
    for (i = 0; i < TOWER_SAVE_BYTES; ++i) {
        memcpy(changed, saved, sizeof saved);
        changed[i] ^= 0x80;
        assert(tower_import(t, changed, sizeof changed) == TOWER_ERR_SAVE);
        assert(tower_hash(t) == hash);
    }
    memcpy(changed, saved, sizeof saved);
    changed[116] = TOWER_TILE_FLOOR;
    fix_checksum(changed);
    assert(tower_import(t, changed, sizeof changed) == TOWER_ERR_SAVE);
    assert(tower_import(t, saved, sizeof saved - 1) == TOWER_ERR_SAVE);
    assert(tower_hash(t) == hash);

    /* Shop price increases jointly for attack and defence.
     * The store's spent-gold validator must accept this exact sequence. */
    assert(tower_import(other, saved, sizeof saved) == TOWER_OK);
    s = read_state(other);
    assert(tower_shop_price(&s) == 15);
    assert(tower_buy(other, 0) == TOWER_STATUS_ITEM);
    s = read_state(other);
    assert(s.attack == WIN_EXPECT_ATTACK + TOWER_SHOP_GAIN);
    assert(s.gold == WIN_EXPECT_GOLD - 15);
    assert(tower_shop_price(&s) == 25);
    assert(tower_buy(other, 1) == TOWER_STATUS_ITEM);
    s = read_state(other);
    assert(s.defence == WIN_EXPECT_DEFENCE + TOWER_SHOP_GAIN);
    assert(s.gold == WIN_EXPECT_GOLD - 40);
    assert(tower_shop_price(&s) == 35);
    assert(tower_export(other, changed, sizeof changed) == TOWER_OK);
    assert(tower_import(t, changed, sizeof changed) == TOWER_OK);
    assert(tower_hash(t) == tower_hash(other));

    tower_delete(other);
    tower_delete(t);
    tower_delete(NULL);
    puts("PASS: ten floors, cross-floor keys, NPCs, transactional 2 KiB saves");
    return 0;
}
