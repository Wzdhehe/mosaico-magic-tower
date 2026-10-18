#include "tower.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAP_CELLS (TOWER_WIDTH * TOWER_HEIGHT)
#define MAP_OFFSET 116u
#define SAVE_VERSION 4u
#define MAX_TURNS 1000000000u

struct tower_t {
    tower_snapshot_t state;
    uint8_t map[TOWER_FLOORS][MAP_CELLS];
    uint8_t wire[TOWER_SAVE_BYTES];
};

/* Hand-authored 11x11 floors: combat costs, optional supply rooms and
 * key-gated loops. F4 grants the blue key for F3's sealed red key; F5
 * demands that red key. F7's wraith guards F8's mandatory red seal.
 * Static layouts use no random generation or per-frame pathfinding. */
static const char initial_map[TOWER_FLOORS][TOWER_HEIGHT][TOWER_WIDTH + 1] = {
    {"###########",
     "#####U#####",
     "#####.#####",
     "#####sap###",
     "#######k###",
     "###b.v.B###",
     "###adY###P#",
     "###s#.###.#",
     "###y.p.r.R#",
     "###X#.n####",
     "###########"},
    {"###########",
     "#####U#####",
     "#####k#####",
     "#####B#####",
     "#S###v#####",
     "#b.......P#",
     "#####Y#####",
     "#a.....n.d#",
     "#y.......p#",
     "#####D#####",
     "###########"},
    {"###########",
     "#####U...##",
     "##....na###",
     "#######..##",
     "#r..#...###",
     "#.a.#...###",
     "###.###v.##",
     "#...#Y.Pd##",
     "#yp.B...p##",
     "#####D#####",
     "###########"},
    {"###########",
     "###..U..###",
     "###a.n..###",
     "###R###.###",
     "#...#..b..#",
     "#.r.#.#.d.#",
     "#P..###...#",
     "####..##B##",
     "#y..Y..b.p#",
     "#####D#####",
     "###########"},
    {"###########",
     "##...U...##",
     "##.m....p##",
     "##..###..##",
     "##.......##",
     "#####R#####",
     "#a...g....#",
     "#.P#n..#d.#",
     "#..#...#..#",
     "#####D#####",
     "###########"},
    {"###########",
     "###..U..###",
     "###.###.###",
     "###.###.###",
     "###.###.###",
     "##dm###ga##",
     "###.###.###",
     "##.......##",
     "##p..n..P##",
     "#####D#####",
     "###########"},
    {"###########",
     "###..U..###",
     "#...#...d.#",
     "#..m#...#.#",
     "##a.#...###",
     "###....n#r#",
     "#...###..w#",
     "#.p.#....P#",
     "###.....y.#",
     "#####D#####",
     "###########"},
    {"###########",
     "#####U..###",
     "#a..#....d#",
     "#....R#####",
     "#.#.#.###.#",
     "#.#.#..k..#",
     "#...###.###",
     "#.p.#w....#",
     "#..n....P.#",
     "#####D#####",
     "###########"},
    {"###########",
     "#####U#####",
     "###a.d..###",
     "###.###.###",
     "###.###.###",
     "##dg###wa##",
     "###.###.###",
     "###.....###",
     "###P.n.p###",
     "#####D#####",
     "###########"},
    {"###########",
     "##...G...##",
     "##.......##",
     "#####M#####",
     "###..n..###",
     "##.a...d.##",
     "##.###...##",
     "##P....wp##",
     "###.....###",
     "#####D#####",
     "###########"},
};

static tower_tile_t decode_tile(char c)
{
    switch (c) {
    case '#': return TOWER_TILE_WALL;
    case 'y': return TOWER_TILE_KEY_YELLOW;
    case 'b': return TOWER_TILE_KEY_BLUE;
    case 'r': return TOWER_TILE_KEY_RED;
    case 'Y': return TOWER_TILE_DOOR_YELLOW;
    case 'B': return TOWER_TILE_DOOR_BLUE;
    case 'R': return TOWER_TILE_DOOR_RED;
    case 'p': return TOWER_TILE_POTION;
    case 'P': return TOWER_TILE_BIG_POTION;
    case 'a': return TOWER_TILE_ATTACK;
    case 'd': return TOWER_TILE_DEFENCE;
    case 's': return TOWER_TILE_SLIME;
    case 'v': return TOWER_TILE_BAT;
    case 'k': return TOWER_TILE_KNIGHT;
    case 'X': return TOWER_TILE_SENTINEL;
    case 'm': return TOWER_TILE_MAGE;
    case 'g': return TOWER_TILE_GOLEM;
    case 'w': return TOWER_TILE_WRAITH;
    case 'M': return TOWER_TILE_DEMON;
    case 'S': return TOWER_TILE_SHOP;
    case 'n': return TOWER_TILE_NPC;
    case 'U': return TOWER_TILE_UP;
    case 'D': return TOWER_TILE_DOWN;
    case 'G': return TOWER_TILE_GOAL;
    default: return TOWER_TILE_FLOOR;
    }
}

static bool is_enemy(tower_tile_t t)
{
    return t >= TOWER_TILE_SLIME && t <= TOWER_TILE_DEMON;
}

static bool consumable(tower_tile_t t)
{
    return (t >= TOWER_TILE_KEY_YELLOW && t <= TOWER_TILE_DEMON) ||
           t == TOWER_TILE_GOAL;
}

static uint32_t enemy_gold(tower_tile_t t)
{
    switch (t) {
    case TOWER_TILE_SLIME: return 3;
    case TOWER_TILE_BAT: return 6;
    case TOWER_TILE_KNIGHT: return 12;
    case TOWER_TILE_SENTINEL: return 60;
    case TOWER_TILE_MAGE: return 18;
    case TOWER_TILE_GOLEM: return 26;
    case TOWER_TILE_WRAITH: return 34;
    case TOWER_TILE_DEMON: return 150;
    default: return 0;
    }
}

static tower_status_t set_status(tower_handle_t h, tower_status_t status)
{
    h->state.status = status;
    return status;
}

tower_result_t tower_create(const tower_config_t *config, tower_handle_t *ret_handle)
{
    tower_handle_t h;
    (void)config;
    if (!ret_handle) return TOWER_ERR_ARGUMENT;
    *ret_handle = NULL;
    h = calloc(1, sizeof *h);
    if (!h) return TOWER_ERR_MEMORY;
    tower_new(h);
    *ret_handle = h;
    return TOWER_OK;
}

void tower_delete(tower_handle_t h) { free(h); }

void tower_new(tower_handle_t h)
{
    unsigned f, x, y;
    if (!h) return;
    memset(&h->state, 0, sizeof h->state);
    h->state.x = 5; h->state.y = 9;
    h->state.hp = 100; h->state.attack = 12; h->state.defence = 4;
    for (f = 0; f < TOWER_FLOORS; ++f)
        for (y = 0; y < TOWER_HEIGHT; ++y)
            for (x = 0; x < TOWER_WIDTH; ++x)
                h->map[f][y * TOWER_WIDTH + x] = (uint8_t)decode_tile(initial_map[f][y][x]);
}

tower_result_t tower_read(tower_handle_t h, tower_snapshot_t *snapshot)
{
    if (!h || !snapshot) return TOWER_ERR_ARGUMENT;
    *snapshot = h->state;
    return TOWER_OK;
}

tower_tile_t tower_tile(tower_handle_t h, unsigned f, int x, int y)
{
    if (!h || f >= TOWER_FLOORS || x < 0 || x >= TOWER_WIDTH ||
        y < 0 || y >= TOWER_HEIGHT) return TOWER_TILE_WALL;
    return (tower_tile_t)h->map[f][y * TOWER_WIDTH + x];
}

tower_result_t tower_enemy_preview(tower_handle_t h, tower_tile_t tile,
                                   tower_enemy_preview_t *p)
{
    uint32_t hit, retaliation;
    uint64_t damage;
    if (!h || !p || !is_enemy(tile)) return TOWER_ERR_ARGUMENT;
    memset(p, 0, sizeof *p);
    p->tile = tile;
    switch (tile) {
    case TOWER_TILE_SLIME: p->hp = 24; p->attack = 6; p->defence = 2; p->gold = 3; break;
    case TOWER_TILE_BAT: p->hp = 35; p->attack = 11; p->defence = 4; p->gold = 6; break;
    case TOWER_TILE_KNIGHT: p->hp = 70; p->attack = 20; p->defence = 8; p->gold = 12; break;
    case TOWER_TILE_MAGE: p->hp = 120; p->attack = 22; p->defence = 10; p->gold = 18; break;
    case TOWER_TILE_GOLEM: p->hp = 180; p->attack = 30; p->defence = 16; p->gold = 26; break;
    case TOWER_TILE_WRAITH: p->hp = 260; p->attack = 36; p->defence = 22; p->gold = 34; break;
    case TOWER_TILE_DEMON: p->hp = 700; p->attack = 58; p->defence = 46; p->gold = 150; break;
    default: p->hp = 999; p->attack = 200; p->defence = 120; p->gold = 0; break;
    }
    if (h->state.attack <= p->defence) {
        p->damage = UINT32_MAX;
        return TOWER_OK;
    }
    hit = h->state.attack - p->defence;
    p->rounds = (p->hp + hit - 1) / hit;
    retaliation = p->attack > h->state.defence ? p->attack - h->state.defence : 0;
    damage = (uint64_t)(p->rounds - 1) * retaliation;
    p->damage = damage > UINT32_MAX ? UINT32_MAX : (uint32_t)damage;
    p->winnable = damage < h->state.hp;
    return TOWER_OK;
}

static void clear_pending(tower_handle_t h)
{
    h->state.phase = TOWER_PHASE_PLAY;
    h->state.pending_x = h->state.pending_y = 0;
    memset(&h->state.pending, 0, sizeof h->state.pending);
}

static void advance_turn(tower_handle_t h)
{
    if (h->state.turns < MAX_TURNS) ++h->state.turns;
}

tower_status_t tower_move(tower_handle_t h, int dx, int dy)
{
    int x, y;
    unsigned color;
    tower_tile_t tile;
    tower_enemy_preview_t preview;
    tower_status_t status = TOWER_STATUS_MOVED;
    if (!h) return TOWER_STATUS_WALL;
    if (h->state.phase == TOWER_PHASE_COMBAT) return TOWER_STATUS_CONFIRM;
    if (!((dx == 0 && (dy == -1 || dy == 1)) ||
          (dy == 0 && (dx == -1 || dx == 1)))) return set_status(h, TOWER_STATUS_WALL);
    x = (int)h->state.x + dx; y = (int)h->state.y + dy;
    tile = tower_tile(h, h->state.floor, x, y);
    if (tile == TOWER_TILE_WALL) return set_status(h, TOWER_STATUS_WALL);
    if (is_enemy(tile)) {
        tower_enemy_preview(h, tile, &preview);
        if (!preview.winnable) return set_status(h, TOWER_STATUS_TOO_STRONG);
        h->state.phase = TOWER_PHASE_COMBAT;
        h->state.pending_x = (unsigned)x; h->state.pending_y = (unsigned)y;
        h->state.pending = preview;
        return set_status(h, TOWER_STATUS_CONFIRM);
    }
    if (tile >= TOWER_TILE_DOOR_YELLOW && tile <= TOWER_TILE_DOOR_RED) {
        color = (unsigned)(tile - TOWER_TILE_DOOR_YELLOW);
        if (!h->state.keys[color]) return set_status(h, TOWER_STATUS_NEED_KEY);
        --h->state.keys[color];
        status = TOWER_STATUS_DOOR;
    } else if (tile >= TOWER_TILE_KEY_YELLOW && tile <= TOWER_TILE_KEY_RED) {
        ++h->state.keys[tile - TOWER_TILE_KEY_YELLOW]; status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_POTION || tile == TOWER_TILE_BIG_POTION) {
        h->state.hp += tile == TOWER_TILE_POTION ? 30 : 80; status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_ATTACK) {
        h->state.attack += 5; status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_DEFENCE) {
        h->state.defence += 3; status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_GOAL) {
        h->state.cleared = true; status = TOWER_STATUS_CLEARED;
    } else if (tile == TOWER_TILE_SHOP) {
        status = TOWER_STATUS_SHOP;
    } else if (tile == TOWER_TILE_NPC) {
        status = TOWER_STATUS_TALK;
    }
    if (consumable(tile)) h->map[h->state.floor][y * TOWER_WIDTH + x] = TOWER_TILE_FLOOR;
    h->state.x = (unsigned)x; h->state.y = (unsigned)y;
    if (tile == TOWER_TILE_UP || tile == TOWER_TILE_DOWN) {
        if (tile == TOWER_TILE_UP) {
            if (h->state.floor + 1 < TOWER_FLOORS) {
                h->state.floor += 1; h->state.x = 5; h->state.y = 9;
                status = TOWER_STATUS_STAIRS;
            }
        } else if (h->state.floor > 0) {
            h->state.floor -= 1; h->state.x = 5; h->state.y = 1;
            status = TOWER_STATUS_STAIRS;
        }
    }
    advance_turn(h);
    return set_status(h, status);
}

tower_status_t tower_confirm(tower_handle_t h)
{
    if (!h) return TOWER_STATUS_READY;
    if (h->state.phase != TOWER_PHASE_COMBAT) return h->state.status;
    h->state.hp -= h->state.pending.damage;
    h->state.gold += h->state.pending.gold;
    h->state.x = h->state.pending_x; h->state.y = h->state.pending_y;
    h->map[h->state.floor][h->state.y * TOWER_WIDTH + h->state.x] = TOWER_TILE_FLOOR;
    clear_pending(h);
    advance_turn(h);
    return set_status(h, TOWER_STATUS_VICTORY);
}

tower_status_t tower_cancel(tower_handle_t h)
{
    if (!h) return TOWER_STATUS_READY;
    if (h->state.phase != TOWER_PHASE_COMBAT) return h->state.status;
    clear_pending(h);
    return set_status(h, TOWER_STATUS_CANCELLED);
}

uint32_t tower_shop_price(const tower_snapshot_t *state)
{
    uint64_t bought, price;
    if (!state) return TOWER_SHOP_COST;
    bought = (uint64_t)state->shop_attack + state->shop_defence;
    price = TOWER_SHOP_COST + (uint64_t)TOWER_SHOP_STEP * bought;
    return price > UINT32_MAX ? UINT32_MAX : (uint32_t)price;
}

tower_status_t tower_buy(tower_handle_t h, int kind)
{
    uint32_t price;
    if (!h) return TOWER_STATUS_NEED_GOLD;
    if (h->state.phase != TOWER_PHASE_PLAY) return h->state.status;
    if (kind != 0 && kind != 1) return set_status(h, TOWER_STATUS_NEED_GOLD);
    price = tower_shop_price(&h->state);
    if (h->state.gold < price) return set_status(h, TOWER_STATUS_NEED_GOLD);
    h->state.gold -= price;
    if (kind == 0) {
        h->state.attack += TOWER_SHOP_GAIN;
        ++h->state.shop_attack;
    } else {
        h->state.defence += TOWER_SHOP_GAIN;
        ++h->state.shop_defence;
    }
    return set_status(h, TOWER_STATUS_ITEM);
}

static void put32(uint8_t *b, unsigned offset, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) b[offset + i] = (uint8_t)(value >> (8 * i));
}

static uint32_t get32(const uint8_t *b, unsigned offset)
{
    return (uint32_t)b[offset] | (uint32_t)b[offset + 1] << 8 |
           (uint32_t)b[offset + 2] << 16 | (uint32_t)b[offset + 3] << 24;
}

static uint32_t wire_checksum(const uint8_t *b)
{
    uint32_t hash = 2166136261u;
    unsigned i;
    for (i = 0; i < TOWER_SAVE_BYTES; ++i)
        if (i < 8 || i > 11) hash = (hash ^ b[i]) * 16777619u;
    return hash;
}

/* Explicit fields avoid ABI, alignment, endianness and bool representation issues. */
static void encode(tower_handle_t h, uint8_t *b)
{
    const tower_snapshot_t *s = &h->state;
    const tower_enemy_preview_t *p = &s->pending;
    unsigned offset = 12;
#define FIELD(v) do { put32(b, offset, (uint32_t)(v)); offset += 4; } while (0)
    memset(b, 0, TOWER_SAVE_BYTES);
    memcpy(b, "MTWR", 4); b[4] = SAVE_VERSION;
    b[6] = (uint8_t)TOWER_SAVE_BYTES; b[7] = (uint8_t)(TOWER_SAVE_BYTES >> 8);
    FIELD(s->phase); FIELD(s->status); FIELD(s->floor); FIELD(s->x); FIELD(s->y);
    FIELD(s->hp); FIELD(s->attack); FIELD(s->defence);
    FIELD(s->keys[0]); FIELD(s->keys[1]); FIELD(s->keys[2]);
    FIELD(s->gold); FIELD(s->turns); FIELD(s->cleared);
    FIELD(s->pending_x); FIELD(s->pending_y);
    FIELD(p->tile); FIELD(p->hp); FIELD(p->attack); FIELD(p->defence); FIELD(p->gold);
    FIELD(p->damage); FIELD(p->rounds); FIELD(p->winnable);
    FIELD(s->shop_attack); FIELD(s->shop_defence);
#undef FIELD
    memcpy(b + MAP_OFFSET, h->map, sizeof h->map);
    put32(b, 8, wire_checksum(b));
}

tower_result_t tower_export(tower_handle_t h, uint8_t *b, size_t size)
{
    if (!h || !b || size != TOWER_SAVE_BYTES) return TOWER_ERR_ARGUMENT;
    encode(h, b);
    return TOWER_OK;
}

static void decode(tower_handle_t h, const uint8_t *b)
{
    tower_snapshot_t *s = &h->state;
    tower_enemy_preview_t *p = &s->pending;
    unsigned offset = 12;
#define FIELD(v) do { (v) = get32(b, offset); offset += 4; } while (0)
    FIELD(s->phase); FIELD(s->status); FIELD(s->floor); FIELD(s->x); FIELD(s->y);
    FIELD(s->hp); FIELD(s->attack); FIELD(s->defence);
    FIELD(s->keys[0]); FIELD(s->keys[1]); FIELD(s->keys[2]);
    FIELD(s->gold); FIELD(s->turns); FIELD(s->cleared);
    FIELD(s->pending_x); FIELD(s->pending_y);
    FIELD(p->tile); FIELD(p->hp); FIELD(p->attack); FIELD(p->defence); FIELD(p->gold);
    FIELD(p->damage); FIELD(p->rounds); FIELD(p->winnable);
    FIELD(s->shop_attack); FIELD(s->shop_defence);
#undef FIELD
    memcpy(h->map, b + MAP_OFFSET, sizeof h->map);
}

static bool earned_less(uint32_t earned, uint64_t spent)
{ return (uint64_t)earned < spent; }

static bool valid_state(tower_handle_t h)
{
    const tower_snapshot_t *s = &h->state;
    uint32_t consumed[TOWER_TILE_COUNT] = {0};
    tower_enemy_preview_t p;
    unsigned f, x, y, color;
    uint32_t max_hp, gold;
    tower_tile_t current;
    if (s->phase > TOWER_PHASE_COMBAT || s->status > TOWER_STATUS_TALK ||
        s->floor >= TOWER_FLOORS || s->x >= TOWER_WIDTH || s->y >= TOWER_HEIGHT ||
        !s->hp || s->turns > MAX_TURNS) {
        return false;
    }
    for (f = 0; f < TOWER_FLOORS; ++f)
        for (y = 0; y < TOWER_HEIGHT; ++y)
            for (x = 0; x < TOWER_WIDTH; ++x) {
                tower_tile_t original = decode_tile(initial_map[f][y][x]);
                tower_tile_t tile = tower_tile(h, f, (int)x, (int)y);
                if (tile == original) continue;
                if (tile != TOWER_TILE_FLOOR || !consumable(original)) {
                    return false;
                }
                ++consumed[original];
            }
    max_hp = 100 + 30 * consumed[TOWER_TILE_POTION] +
             80 * consumed[TOWER_TILE_BIG_POTION];
    gold = 0;
    for (f = TOWER_TILE_SLIME; f <= TOWER_TILE_DEMON; ++f)
        gold += enemy_gold((tower_tile_t)f) * consumed[f];
    {
        uint64_t bought = (uint64_t)s->shop_attack + s->shop_defence;
        uint64_t spent;
        if (bought > 1000000u) return false;
        spent = bought * TOWER_SHOP_COST +
                (bought * (bought - 1u) / 2u) * TOWER_SHOP_STEP;
        if (earned_less(gold, spent) || s->gold != gold - (uint32_t)spent) {
            return false;
        }
    }
    if (s->hp > max_hp ||
        s->attack != 12 + 5 * consumed[TOWER_TILE_ATTACK] + TOWER_SHOP_GAIN * s->shop_attack ||
        s->defence != 4 + 3 * consumed[TOWER_TILE_DEFENCE] + TOWER_SHOP_GAIN * s->shop_defence ||
        s->cleared != (consumed[TOWER_TILE_GOAL] == 1) ||
        consumed[TOWER_TILE_SENTINEL]) {
        return false;
    }
    for (color = 0; color < 3; ++color) {
        uint32_t keys = consumed[TOWER_TILE_KEY_YELLOW + color];
        uint32_t doors = consumed[TOWER_TILE_DOOR_YELLOW + color];
        if (doors > keys || s->keys[color] != keys - doors) {
            return false;
        }
    }
    current = tower_tile(h, s->floor, (int)s->x, (int)s->y);
    if (current != TOWER_TILE_FLOOR && current != TOWER_TILE_UP &&
        current != TOWER_TILE_DOWN && current != TOWER_TILE_SHOP &&
        current != TOWER_TILE_NPC) {
        return false;
    }
    if (s->phase == TOWER_PHASE_COMBAT) {
        int dx, dy;
        if (s->pending_x >= TOWER_WIDTH || s->pending_y >= TOWER_HEIGHT ||
            s->status != TOWER_STATUS_CONFIRM) return false;
        dx = (int)s->pending_x - (int)s->x; dy = (int)s->pending_y - (int)s->y;
        if (abs(dx) + abs(dy) != 1) return false;
        if (tower_enemy_preview(h, tower_tile(h, s->floor, (int)s->pending_x,
            (int)s->pending_y), &p) != TOWER_OK || !p.winnable) return false;
        if (s->pending.tile != p.tile || s->pending.hp != p.hp ||
            s->pending.attack != p.attack || s->pending.defence != p.defence ||
            s->pending.gold != p.gold || s->pending.damage != p.damage ||
            s->pending.rounds != p.rounds || !s->pending.winnable) return false;
    } else {
        if (s->status == TOWER_STATUS_CONFIRM || s->pending_x || s->pending_y ||
            s->pending.tile || s->pending.hp || s->pending.attack || s->pending.defence ||
            s->pending.gold || s->pending.damage || s->pending.rounds || s->pending.winnable)
            return false;
    }
    return true;
}

tower_result_t tower_import(tower_handle_t h, const uint8_t *b, size_t size)
{
    tower_handle_t candidate;
    unsigned i;
    if (!h || !b) return TOWER_ERR_ARGUMENT;
    if (size != TOWER_SAVE_BYTES || memcmp(b, "MTWR", 4) || b[4] != SAVE_VERSION ||
        b[5] || b[6] != (uint8_t)TOWER_SAVE_BYTES || b[7] != (uint8_t)(TOWER_SAVE_BYTES >> 8) ||
        get32(b, 8) != wire_checksum(b) || get32(b, 12 + 13 * 4) > 1 ||
        get32(b, 12 + 23 * 4) > 1) return TOWER_ERR_SAVE;
    for (i = MAP_OFFSET + TOWER_FLOORS * MAP_CELLS; i < TOWER_SAVE_BYTES; ++i)
        if (b[i]) return TOWER_ERR_SAVE;
    candidate = calloc(1, sizeof *candidate);
    if (!candidate) return TOWER_ERR_MEMORY;
    decode(candidate, b);
    if (!valid_state(candidate)) {
        tower_delete(candidate); return TOWER_ERR_SAVE;
    }
    h->state = candidate->state;
    memcpy(h->map, candidate->map, sizeof h->map);
    tower_delete(candidate);
    return TOWER_OK;
}

uint32_t tower_hash(tower_handle_t h)
{
    if (!h) return 0;
    encode(h, h->wire);
    return get32(h->wire, 8);
}

const char *tower_status_text(tower_status_t status)
{
    switch (status) {
    case TOWER_STATUS_READY: return "Climb the tower";
    case TOWER_STATUS_MOVED: return "Choose your next step";
    case TOWER_STATUS_WALL: return "Stone blocks the way";
    case TOWER_STATUS_NEED_KEY: return "A matching key is needed";
    case TOWER_STATUS_ITEM: return "Supplies collected";
    case TOWER_STATUS_DOOR: return "Door opened";
    case TOWER_STATUS_CONFIRM: return "Fight? Confirm or cancel";
    case TOWER_STATUS_TOO_STRONG: return "Too strong: gather gems first";
    case TOWER_STATUS_VICTORY: return "Enemy defeated";
    case TOWER_STATUS_CANCELLED: return "Fight cancelled";
    case TOWER_STATUS_STAIRS: return "Floor changed";
    case TOWER_STATUS_CLEARED: return "Beacon lit! Tower cleared";
    case TOWER_STATUS_SHOP: return "Merchant: buy or leave";
    case TOWER_STATUS_NEED_GOLD: return "Not enough gold";
    case TOWER_STATUS_TALK: return "A voice calls out";
    default: return "";
    }
}

const char *tower_enemy_name(tower_tile_t tile)
{
    switch (tile) {
    case TOWER_TILE_SLIME: return "Slime";
    case TOWER_TILE_BAT: return "Bat";
    case TOWER_TILE_KNIGHT: return "Knight";
    case TOWER_TILE_SENTINEL: return "Sentinel";
    case TOWER_TILE_MAGE: return "Mage";
    case TOWER_TILE_GOLEM: return "Golem";
    case TOWER_TILE_WRAITH: return "Wraith";
    case TOWER_TILE_DEMON: return "Demon King";
    default: return "";
    }
}
