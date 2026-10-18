#ifndef MAGIC_TOWER_H
#define MAGIC_TOWER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOWER_WIDTH 11
#define TOWER_HEIGHT 11
#define TOWER_FLOORS 10
#define TOWER_SAVE_BYTES 2048u

/* Single-owner portable model. All mutable state belongs to the handle. */
typedef struct tower_t *tower_handle_t;
typedef struct { unsigned reserved; } tower_config_t;
typedef enum {
    TOWER_OK = 0, TOWER_ERR_ARGUMENT = -1, TOWER_ERR_MEMORY = -2,
    TOWER_ERR_SAVE = -3
} tower_result_t;
typedef enum { TOWER_PHASE_PLAY, TOWER_PHASE_COMBAT } tower_phase_t;
typedef enum {
    TOWER_STATUS_READY, TOWER_STATUS_MOVED, TOWER_STATUS_WALL,
    TOWER_STATUS_NEED_KEY, TOWER_STATUS_ITEM, TOWER_STATUS_DOOR,
    TOWER_STATUS_CONFIRM, TOWER_STATUS_TOO_STRONG, TOWER_STATUS_VICTORY,
    TOWER_STATUS_CANCELLED, TOWER_STATUS_STAIRS, TOWER_STATUS_CLEARED,
    TOWER_STATUS_SHOP, TOWER_STATUS_NEED_GOLD, TOWER_STATUS_TALK
} tower_status_t;
typedef enum {
    TOWER_TILE_FLOOR, TOWER_TILE_WALL,
    TOWER_TILE_KEY_YELLOW, TOWER_TILE_KEY_BLUE, TOWER_TILE_KEY_RED,
    TOWER_TILE_DOOR_YELLOW, TOWER_TILE_DOOR_BLUE, TOWER_TILE_DOOR_RED,
    TOWER_TILE_POTION, TOWER_TILE_BIG_POTION, TOWER_TILE_ATTACK,
    TOWER_TILE_DEFENCE, TOWER_TILE_SLIME, TOWER_TILE_BAT,
    TOWER_TILE_KNIGHT, TOWER_TILE_SENTINEL, TOWER_TILE_MAGE,
    TOWER_TILE_GOLEM, TOWER_TILE_WRAITH, TOWER_TILE_DEMON,
    TOWER_TILE_SHOP, TOWER_TILE_NPC, TOWER_TILE_UP, TOWER_TILE_DOWN,
    TOWER_TILE_GOAL, TOWER_TILE_COUNT
} tower_tile_t;
typedef struct {
    tower_tile_t tile;
    uint32_t hp, attack, defence, gold;
    uint32_t damage, rounds;
    bool winnable;
} tower_enemy_preview_t;
typedef struct {
    tower_phase_t phase;
    tower_status_t status;
    uint32_t floor, x, y, hp, attack, defence;
    uint32_t keys[3]; /* yellow, blue, red */
    uint32_t gold, turns;
    uint32_t shop_attack, shop_defence;
    bool cleared;
    uint32_t pending_x, pending_y;
    tower_enemy_preview_t pending;
} tower_snapshot_t;

tower_result_t tower_create(const tower_config_t *config, tower_handle_t *ret_handle);
void tower_delete(tower_handle_t handle);
void tower_new(tower_handle_t handle);
tower_result_t tower_read(tower_handle_t handle, tower_snapshot_t *snapshot);
/* Cardinal unit moves only. Combat requires a later tower_confirm call.
 * Refused actions update feedback status only; HP/map/position/turns stay intact. */
tower_status_t tower_move(tower_handle_t handle, int dx, int dy);
tower_status_t tower_confirm(tower_handle_t handle);
tower_status_t tower_cancel(tower_handle_t handle);
tower_tile_t tower_tile(tower_handle_t handle, unsigned floor, int x, int y);
/* Shared rising purchase price: 15, 25, 35, ... for both attributes. */
#define TOWER_SHOP_COST 15u
#define TOWER_SHOP_STEP 10u
#define TOWER_SHOP_GAIN 3u
uint32_t tower_shop_price(const tower_snapshot_t *state);
tower_status_t tower_buy(tower_handle_t handle, int kind);
tower_result_t tower_enemy_preview(tower_handle_t handle, tower_tile_t tile,
                                   tower_enemy_preview_t *preview);
/* Fixed little-endian wire format; corrupt imports leave all live state intact. */
tower_result_t tower_export(tower_handle_t handle, uint8_t *bytes, size_t size);
tower_result_t tower_import(tower_handle_t handle, const uint8_t *bytes, size_t size);
uint32_t tower_hash(tower_handle_t handle);
const char *tower_status_text(tower_status_t status);
const char *tower_enemy_name(tower_tile_t tile);

#ifdef __cplusplus
}
#endif
#endif
