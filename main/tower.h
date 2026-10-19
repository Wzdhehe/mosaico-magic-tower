#ifndef MAGIC_TOWER_H
#define MAGIC_TOWER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Viewport stays 11x11: the panel geometry in game_config.h is built around it.
 * Floors are larger than the viewport and are panned by the camera. */
#define TOWER_VIEW_W 11
#define TOWER_VIEW_H 11
#define TOWER_FLOOR_W 20
#define TOWER_FLOOR_H 20
#define TOWER_FLOORS 50

#define TOWER_CELLS (TOWER_FLOOR_W * TOWER_FLOOR_H)
#define TOWER_FLAG_BYTES ((TOWER_CELLS + 7) / 8)

/* The campaign is five ten-floor chapters. Clearing chapter N — its goal gate or
 * leaving through its exit stair — unlocks anchor N+1; anchor 1 is always open.
 * No other floor is a teleport target. */
#define TOWER_CHAPTERS 5u
#define TOWER_ANCHORS (TOWER_CHAPTERS + 1u)

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
    TOWER_STATUS_SHOP, TOWER_STATUS_NEED_GOLD, TOWER_STATUS_TALK,
    TOWER_STATUS_DEAD, TOWER_STATUS_SECRET, TOWER_STATUS_EVENT,
    TOWER_STATUS_LOCKED, TOWER_STATUS_CHAPTER
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
    TOWER_TILE_GOAL, TOWER_TILE_SECRET, TOWER_TILE_EVENT,
    TOWER_TILE_COUNT
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
    /* 50 floors need 50 bits; two words keep the wire format explicit. */
    uint32_t explored_lo, explored_hi;
    uint32_t pending_x, pending_y;
    tower_enemy_preview_t pending;
    /* Bit n-1 set means chapter n is complete. This is an explicit chapter
     * state, never inferred from explored floors. */
    uint32_t chapters;
} tower_snapshot_t;

/* Field order is the wire layout; tower.c derives the byte size from it. */
#define TOWER_SAVE_FIELDS 30u
#define TOWER_SAVE_BYTES (12u + TOWER_SAVE_FIELDS * 4u + TOWER_FLOORS * TOWER_FLAG_BYTES)

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
tower_tile_t tower_tile_current(tower_handle_t handle, int x, int y);
unsigned tower_floor_width(void);
unsigned tower_floor_height(void);
unsigned tower_floors(void);
/* Shared rising purchase price: 15, 25, 35, ... for both attributes. */
#define TOWER_SHOP_COST 15u
#define TOWER_SHOP_STEP 10u
#define TOWER_SHOP_GAIN 3u
uint32_t tower_shop_price(const tower_snapshot_t *state);
tower_status_t tower_buy(tower_handle_t handle, int kind);
tower_result_t tower_enemy_preview(tower_handle_t handle, tower_tile_t tile,
                                   tower_enemy_preview_t *preview);
/* Teleport is free and limited to the six chapter anchors, in anchor order
 * 1/10/20/30/40/50. A locked anchor is refused with TOWER_STATUS_LOCKED. */
tower_status_t tower_teleport(tower_handle_t handle, unsigned anchor);
unsigned tower_anchor_floor(unsigned anchor);
bool tower_anchor_unlocked(const tower_snapshot_t *state, unsigned anchor);
bool tower_chapter_complete(const tower_snapshot_t *state, unsigned chapter);
bool tower_explored(const tower_snapshot_t *state, unsigned floor);
/* Authored campaign events. Quest state: 0 waiting, 1 carrying, 2 delivered. */
bool tower_sluice_south(tower_handle_t handle);
unsigned tower_sigils(tower_handle_t handle);
unsigned tower_quest_state(tower_handle_t handle, unsigned quest);
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
