#include "tower.h"
#include "tower_floors.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* The saved map is a per-floor bitmap of consumed cells, not a tile array.
 * Its geometry has to be constant so that a floor can change shape without
 * changing the wire format: the largest campaign floor is 19x19, and every
 * floor reserves TOWER_MAX_CELLS bits. TOWER_FLAG_BYTES (from tower.h) rounds
 * that to whole bytes, so the surplus high bits are always zero. */
#define TOWER_MAX_W 19u
#define TOWER_MAX_H 19u
#define TOWER_MAX_CELLS (TOWER_MAX_W * TOWER_MAX_H)

#define SAVE_VERSION 8u /* authored campaign revision */
#define MAX_TURNS 1000000000u

/* tower_snapshot_t order, one little-endian u32 each. The header reserves
 * TOWER_SAVE_FIELDS == 30 slots while the snapshot documents 29 fields, so the
 * 30th slot is written as zero and rejected on import when it is not. */
enum {
    FIELD_PHASE = 0, FIELD_STATUS, FIELD_FLOOR, FIELD_X, FIELD_Y, FIELD_HP,
    FIELD_ATTACK, FIELD_DEFENCE, FIELD_KEY_YELLOW, FIELD_KEY_BLUE, FIELD_KEY_RED,
    FIELD_GOLD, FIELD_TURNS, FIELD_SHOP_ATTACK, FIELD_SHOP_DEFENCE, FIELD_CLEARED,
    FIELD_EXPLORED_LO, FIELD_EXPLORED_HI, FIELD_PENDING_X, FIELD_PENDING_Y,
    FIELD_PENDING_TILE, FIELD_PENDING_HP, FIELD_PENDING_ATTACK,
    FIELD_PENDING_DEFENCE, FIELD_PENDING_GOLD, FIELD_PENDING_DAMAGE,
    FIELD_PENDING_ROUNDS, FIELD_PENDING_WINNABLE, FIELD_CHAPTERS,
    FIELD_COUNT
};

#define SAVE_HEADER_BYTES 12u
#define SAVE_FIELD_OFFSET(i) (SAVE_HEADER_BYTES + (unsigned)(i) * 4u)
#define SAVE_BITMAP_OFFSET (SAVE_HEADER_BYTES + TOWER_SAVE_FIELDS * 4u)
#define SAVE_RESERVED_OFFSET (SAVE_HEADER_BYTES + (TOWER_SAVE_FIELDS - 1u) * 4u)

_Static_assert(TOWER_MAX_W <= (unsigned)TOWER_FLOOR_W,
               "bitmap width exceeds the flag budget in tower.h");
_Static_assert(TOWER_MAX_H <= (unsigned)TOWER_FLOOR_H,
               "bitmap height exceeds the flag budget in tower.h");
_Static_assert(TOWER_MAX_CELLS <= TOWER_FLAG_BYTES * 8u,
               "19x19 bitmap does not fit TOWER_FLAG_BYTES");
_Static_assert((unsigned)FIELD_COUNT == 29u,
               "the documented snapshot has 29 u32 fields");
_Static_assert((unsigned)FIELD_COUNT + 1u == TOWER_SAVE_FIELDS,
               "one reserved slot completes TOWER_SAVE_FIELDS");
_Static_assert(SAVE_RESERVED_OFFSET + 4u == SAVE_BITMAP_OFFSET,
               "reserved slot must sit directly before the bitmap");
_Static_assert(SAVE_BITMAP_OFFSET + TOWER_FLOORS * TOWER_FLAG_BYTES ==
                   TOWER_SAVE_BYTES,
               "wire bitmap does not match TOWER_SAVE_BYTES");

struct tower_t {
    tower_snapshot_t state;
    uint8_t flags[TOWER_FLOORS][TOWER_FLAG_BYTES];
    uint8_t wire[TOWER_SAVE_BYTES];
};

/* One character per tile. Unknown characters are plain floor so that a map
 * table with decorative glyphs still loads; the generator only emits the
 * characters listed here. */
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
    case '%': return TOWER_TILE_SECRET;
    case '!': return TOWER_TILE_EVENT;
    default: return TOWER_TILE_FLOOR;
    }
}

static bool is_enemy(tower_tile_t t)
{
    return t >= TOWER_TILE_SLIME && t <= TOWER_TILE_DEMON;
}

/* Everything a move turns into TOWER_TILE_FLOOR. Rows, walls and the two
 * stair tiles are permanent; shops and NPCs are revisited without consuming. */
static bool consumable(tower_tile_t t)
{
    return (t >= TOWER_TILE_KEY_YELLOW && t <= TOWER_TILE_DEMON) ||
           t == TOWER_TILE_GOAL || t == TOWER_TILE_SECRET ||
           t == TOWER_TILE_EVENT;
}

/* A tile the player can legally stand on after an action. Every exclusive
 * tile (loot, door, enemy, secret, event, goal) is consumed before the player
 * lands on it, so only these remain. */
static bool player_standable(tower_tile_t t)
{
    return t == TOWER_TILE_FLOOR || t == TOWER_TILE_UP ||
           t == TOWER_TILE_DOWN || t == TOWER_TILE_SHOP ||
           t == TOWER_TILE_NPC;
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

static bool enemy_base(tower_tile_t t, uint32_t *hp, uint32_t *attack,
                       uint32_t *defence)
{
    switch (t) {
    case TOWER_TILE_SLIME: *hp = 24; *attack = 6; *defence = 2; return true;
    case TOWER_TILE_BAT: *hp = 35; *attack = 11; *defence = 4; return true;
    case TOWER_TILE_KNIGHT: *hp = 70; *attack = 20; *defence = 8; return true;
    case TOWER_TILE_SENTINEL: *hp = 999; *attack = 200; *defence = 120; return true;
    case TOWER_TILE_MAGE: *hp = 120; *attack = 22; *defence = 10; return true;
    case TOWER_TILE_GOLEM: *hp = 180; *attack = 30; *defence = 16; return true;
    case TOWER_TILE_WRAITH: *hp = 260; *attack = 36; *defence = 22; return true;
    case TOWER_TILE_DEMON: *hp = 700; *attack = 58; *defence = 46; return true;
    default: return false;
    }
}

/* Floor f scales hp/attack/defence by (100 + 10*f)/100 with integer division.
 * Gold is deliberately unscaled. Both tower_move and valid_state go through
 * this helper so the preview and the save validator can never disagree. */
static bool preview_for(uint32_t floor, const tower_snapshot_t *s,
                        tower_tile_t tile, tower_enemy_preview_t *p)
{
    uint32_t hp, attack, defence, scale, hit, retaliation;
    uint64_t damage;
    if (!s || !p || !is_enemy(tile) ||
        !enemy_base(tile, &hp, &attack, &defence)) {
        return false;
    }
    memset(p, 0, sizeof *p);
    scale = 100u + 10u * floor;
    p->tile = tile;
    p->hp = hp * scale / 100u;
    p->attack = attack * scale / 100u;
    p->defence = defence * scale / 100u;
    /* Fixed regional identities let players compare upgrade thresholds.
     * The first chapter retains its already verified floor scaling. */
    if (floor >= 10u) {
        static const uint32_t profile[4][8][3] = {{{40,13,5},{60,25,9},{100,36,14},{600,120,90},{160,39,18},{200,48,28},{260,52,30},{700,150,100}},
{{90,36,16},{110,49,22},{190,62,30},{900,200,140},{240,63,35},{300,79,48},{370,86,55},{1100,220,150}},
{{140,60,30},{200,78,38},{300,94,50},{1200,230,190},{360,103,56},{450,121,72},{550,128,78},{1400,250,180}},
{{200,88,45},{280,108,56},{400,128,70},{1200,230,160},{460,139,78},{560,151,95},{700,158,100},{1200,170,110}}};
        const uint32_t *v = profile[floor / 10u - 1u][(unsigned)tile - TOWER_TILE_SLIME];
        p->hp = v[0]; p->attack = v[1]; p->defence = v[2];
    }
    if (floor == 19u && tile == TOWER_TILE_GOLEM) { p->hp=260u; p->attack=62u; p->defence=24u; }
    if (floor == 29u && tile == TOWER_TILE_GOLEM) { p->hp=400u; p->attack=90u; p->defence=43u; }
    if (floor == 39u && tile == TOWER_TILE_WRAITH) { p->hp=650u; p->attack=126u; p->defence=70u; }
    if (floor == 49u && tile == TOWER_TILE_DEMON) { p->hp=1200u; p->attack=170u; p->defence=110u; }
    p->gold = enemy_gold(tile);
    if (s->attack <= p->defence) {
        /* Cannot deal even one point: the fight is impossible by formula. */
        p->damage = UINT32_MAX;
        p->rounds = 0;
        p->winnable = false;
        return true;
    }
    hit = s->attack - p->defence;
    p->rounds = (p->hp + hit - 1u) / hit;
    retaliation = p->attack > s->defence ? p->attack - s->defence : 0u;
    damage = (uint64_t)(p->rounds - 1u) * retaliation;
    p->damage = damage > UINT32_MAX ? UINT32_MAX : (uint32_t)damage;
    p->winnable = damage < (uint64_t)s->hp;
    return true;
}

static const tower_floor_desc_t *floor_desc(unsigned floor)
{
    const tower_floor_desc_t *d;
    if (floor >= TOWER_FLOORS) return NULL;
    d = &tower_floor_table[floor];
    if (!d->rows || d->w == 0 || d->h == 0 ||
        (unsigned)d->w > TOWER_MAX_W || (unsigned)d->h > TOWER_MAX_H) {
        return NULL;
    }
    return d;
}

static bool flag_get(const struct tower_t *h, unsigned floor, unsigned x,
                     unsigned y)
{
    unsigned index = y * TOWER_MAX_W + x;
    return (h->flags[floor][index / 8u] & (uint8_t)(1u << (index % 8u))) != 0;
}

static void flag_set(struct tower_t *h, unsigned floor, unsigned x, unsigned y)
{
    unsigned index = y * TOWER_MAX_W + x;
    h->flags[floor][index / 8u] |= (uint8_t)(1u << (index % 8u));
}

/* Sluice is the only reversible event. Marks are consumed event cells. */
bool tower_sluice_south(tower_handle_t h) {
    return h && flag_get(h, 13u, 3u, 4u);
}
unsigned tower_sigils(tower_handle_t h) {
    if (!h) return 0u;
    return (flag_get(h,11u,4u,3u)?1u:0u) |
           (flag_get(h,26u,5u,6u)?2u:0u) |
           (flag_get(h,34u,11u,9u)?4u:0u);
}

static bool campaign_npc(unsigned f,unsigned x,unsigned y) {
    return (f==10u && x==1u && y==9u) || (f==20u && x==3u && y==10u) || (f==32u && x==9u && y==5u);
}
unsigned tower_quest_state(tower_handle_t h,unsigned q) {
    if (!h) return 0u;
    switch(q) {
    case 0u: return flag_get(h,10u,1u,9u)?2u:flag_get(h,17u,7u,7u)?1u:0u;
    case 1u: return flag_get(h,20u,3u,10u)?2u:flag_get(h,33u,3u,5u)?1u:0u;
    case 2u: return flag_get(h,32u,9u,5u)?2u:flag_get(h,42u,13u,11u)?1u:0u;
    default: return 0u;
    }
}

static bool campaign_hidden(const struct tower_t *h,unsigned f,unsigned x,unsigned y) {
    if (f==7u && !flag_get(h,7u,2u,7u) && ((x>=1u && x<=3u && y>=8u && y<=9u))) return true;
    if (f==10u && !flag_get(h,10u,6u,9u) && ((x>=7u && x<=11u && y>=7u && y<=9u))) return true;
    if (f==17u && !flag_get(h,17u,4u,5u) && ((x>=5u && x<=7u && y>=5u && y<=7u))) return true;
    if (f==20u && !flag_get(h,20u,3u,8u) && ((x>=1u && x<=3u && y>=5u && y<=7u))) return true;
    if (f==27u && !flag_get(h,27u,11u,8u) && ((x>=9u && x<=13u && y>=5u && y<=7u))) return true;
    if (f==32u && !flag_get(h,32u,10u,11u) && ((x>=11u && x<=11u && y>=11u && y<=12u) || (x>=11u && x<=13u && y>=13u && y<=15u))) return true;
    if (f==36u && !flag_get(h,36u,5u,6u) && ((x>=3u && x<=7u && y>=7u && y<=9u))) return true;
    if (f==46u && !flag_get(h,46u,9u,11u) && ((x>=10u && x<=10u && y>=11u && y<=11u) || (x>=11u && x<=13u && y>=11u && y<=15u))) return true;
    return false;
}

static tower_tile_t tile_at(const struct tower_t *h, unsigned floor, int x,
                            int y)
{
    const tower_floor_desc_t *d;
    if (!h || floor >= TOWER_FLOORS || x < 0 || y < 0) return TOWER_TILE_WALL;
    d = floor_desc(floor);
    if (!d || (unsigned)x >= (unsigned)d->w || (unsigned)y >= (unsigned)d->h) {
        return TOWER_TILE_WALL;
    }
    if (campaign_hidden(h,floor,(unsigned)x,(unsigned)y)) return TOWER_TILE_WALL;
    /* Event stays visible in either state so it can be operated again. */
    if (floor == 13u && x == 3 && y == 4 && d->rows[y][x] == '!')
        return TOWER_TILE_EVENT;
    if (floor == 14u && x == 5) {
        if ((y == 2 && tower_sluice_south((tower_handle_t)h)) ||
            (y == 10 && !tower_sluice_south((tower_handle_t)h)))
            return TOWER_TILE_WALL;
    }
    if (floor == 46u && x == 9 && y == 11 &&
        tower_sigils((tower_handle_t)h) != 7u) return TOWER_TILE_WALL;
    if (campaign_npc(floor,(unsigned)x,(unsigned)y)) return TOWER_TILE_NPC;
    if (floor==10u && x==6 && y==9 && tower_quest_state((tower_handle_t)h,0u)!=2u) return TOWER_TILE_WALL;
    if (floor==20u && x==3 && y==8 && tower_quest_state((tower_handle_t)h,1u)!=2u) return TOWER_TILE_WALL;
    if (floor==32u && x==10 && y==11 && tower_quest_state((tower_handle_t)h,2u)!=2u) return TOWER_TILE_WALL;
    if (flag_get(h, floor, (unsigned)x, (unsigned)y)) return TOWER_TILE_FLOOR;
    return decode_tile(d->rows[y][x]);
}

static tower_status_t set_status(tower_handle_t h, tower_status_t status)
{
    h->state.status = status;
    return status;
}

static void mark_explored(struct tower_t *h, unsigned floor)
{
    if (floor < 32u) {
        h->state.explored_lo |= 1u << floor;
    } else if (floor < TOWER_FLOORS) {
        h->state.explored_hi |= 1u << (floor - 32u);
    }
}

static bool find_tile(const struct tower_t *h, unsigned floor,
                      tower_tile_t want, unsigned *x, unsigned *y)
{
    const tower_floor_desc_t *d = floor_desc(floor);
    unsigned cx, cy;
    if (!d) return false;
    for (cy = 0; cy < (unsigned)d->h; ++cy) {
        for (cx = 0; cx < (unsigned)d->w; ++cx) {
            if (tile_at(h, floor, (int)cx, (int)cy) == want) {
                if (x) *x = cx;
                if (y) *y = cy;
                return true;
            }
        }
    }
    return false;
}

static bool find_walkable(const struct tower_t *h, unsigned floor,
                          unsigned *x, unsigned *y)
{
    const tower_floor_desc_t *d = floor_desc(floor);
    unsigned cx, cy;
    if (!d) return false;
    for (cy = 0; cy < (unsigned)d->h; ++cy) {
        for (cx = 0; cx < (unsigned)d->w; ++cx) {
            if (player_standable(tile_at(h, floor, (int)cx, (int)cy))) {
                if (x) *x = cx;
                if (y) *y = cy;
                return true;
            }
        }
    }
    return false;
}

/* Stairs are linked by tile: climbing floor f's 'U' lands on floor f+1's 'D'
 * and descending lands on the 'U' below. The fallback only matters for a
 * malformed floor table; generated floors always carry exactly one of each. */
static void place_at_stairs(struct tower_t *h, unsigned floor,
                            tower_tile_t want)
{
    unsigned x = 0, y = 0;
    if (!find_tile(h, floor, want, &x, &y) &&
        !find_walkable(h, floor, &x, &y)) {
        x = 0;
        y = 0;
    }
    h->state.x = x;
    h->state.y = y;
}

static void enter_floor(struct tower_t *h, unsigned floor)
{
    h->state.floor = floor;
    mark_explored(h, floor);
}

static void clear_pending(tower_handle_t h)
{
    h->state.phase = TOWER_PHASE_PLAY;
    h->state.pending_x = 0;
    h->state.pending_y = 0;
    memset(&h->state.pending, 0, sizeof h->state.pending);
}

static void advance_turn(tower_handle_t h)
{
    if (h->state.turns < MAX_TURNS) ++h->state.turns;
}

static bool explored_bit(const tower_snapshot_t *s, unsigned floor)
{
    if (!s || floor >= TOWER_FLOORS) return false;
    if (floor < 32u) return ((s->explored_lo >> floor) & 1u) != 0;
    return ((s->explored_hi >> (floor - 32u)) & 1u) != 0;
}

/* Anchor 0 is floor 1, anchor n is the last floor of chapter n. */
unsigned tower_anchor_floor(unsigned anchor)
{
    if (anchor == 0) return 0;
    if (anchor >= TOWER_ANCHORS) return TOWER_FLOORS - 1u;
    return anchor * 10u - 1u;
}

bool tower_chapter_complete(const tower_snapshot_t *state, unsigned chapter)
{
    if (!state || chapter < 1u || chapter > TOWER_CHAPTERS) return false;
    return ((state->chapters >> (chapter - 1u)) & 1u) != 0;
}

bool tower_anchor_unlocked(const tower_snapshot_t *state, unsigned anchor)
{
    if (!state || anchor >= TOWER_ANCHORS) return false;
    if (anchor == 0) return true;
    return tower_chapter_complete(state, anchor);
}

/* The goal tile is the chapter gate: on the last floor it ends the campaign,
 * everywhere else it records that this ten-floor chapter is done. The chapter
 * number follows the floor that carries the tile, so a mis-placed goal still
 * has one unambiguous meaning; tools reject goals off a chapter boundary. */
static void complete_chapter(struct tower_t *h, unsigned floor, bool last)
{
    h->state.chapters |= 1u << (floor / 10u);
    if (last) h->state.cleared = true;
}

tower_result_t tower_create(const tower_config_t *config,
                            tower_handle_t *ret_handle)
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
    if (!h) return;
    memset(&h->state, 0, sizeof h->state);
    memset(h->flags, 0, sizeof h->flags);
    h->state.phase = TOWER_PHASE_PLAY;
    h->state.status = TOWER_STATUS_READY;
    h->state.hp = 100;
    h->state.attack = 12;
    h->state.defence = 4;
    mark_explored(h, 0);
    place_at_stairs(h, 0, TOWER_TILE_DOWN);
}

tower_result_t tower_read(tower_handle_t h, tower_snapshot_t *snapshot)
{
    if (!h || !snapshot) return TOWER_ERR_ARGUMENT;
    *snapshot = h->state;
    return TOWER_OK;
}

tower_tile_t tower_tile(tower_handle_t h, unsigned floor, int x, int y)
{
    return tile_at(h, floor, x, y);
}

tower_tile_t tower_tile_current(tower_handle_t h, int x, int y)
{
    if (!h) return TOWER_TILE_WALL;
    return tile_at(h, h->state.floor, x, y);
}

/* No handle is available here, so these report the model's capacity
 * (TOWER_FLOOR_W/H from tower.h). An individual floor may be smaller; the
 * shape is observed by probing tower_tile()/tower_tile_current(). */
unsigned tower_floor_width(void) { return TOWER_FLOOR_W; }
unsigned tower_floor_height(void) { return TOWER_FLOOR_H; }
unsigned tower_floors(void) { return TOWER_FLOORS; }

bool tower_explored(const tower_snapshot_t *state, unsigned floor)
{
    return explored_bit(state, floor);
}

tower_result_t tower_enemy_preview(tower_handle_t h, tower_tile_t tile,
                                   tower_enemy_preview_t *preview)
{
    if (!h || !preview) return TOWER_ERR_ARGUMENT;
    if (!preview_for(h->state.floor, &h->state, tile, preview)) {
        return TOWER_ERR_ARGUMENT;
    }
    return TOWER_OK;
}

tower_status_t tower_move(tower_handle_t h, int dx, int dy)
{
    int x, y;
    unsigned color;
    tower_tile_t tile;
    tower_enemy_preview_t preview;
    tower_status_t status = TOWER_STATUS_MOVED;

    if (!h) return TOWER_STATUS_WALL;
    if (h->state.hp == 0) return set_status(h, TOWER_STATUS_DEAD);
    if (h->state.phase != TOWER_PHASE_PLAY) return TOWER_STATUS_CONFIRM;
    if (!((dx == 0 && (dy == -1 || dy == 1)) ||
          (dy == 0 && (dx == -1 || dx == 1)))) {
        return set_status(h, TOWER_STATUS_WALL);
    }

    x = (int)h->state.x + dx;
    y = (int)h->state.y + dy;
    tile = tile_at(h, h->state.floor, x, y);

    if (tile == TOWER_TILE_WALL) return set_status(h, TOWER_STATUS_WALL);

    /* Entering a darker floor mouth reveals its authored chamber. */
    if (tile == TOWER_TILE_SECRET) {
        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
        h->state.x = (unsigned)x;
        h->state.y = (unsigned)y;
        advance_turn(h);
        return set_status(h, TOWER_STATUS_SECRET);
    }

    if (is_enemy(tile)) {
        if (!preview_for(h->state.floor, &h->state, tile, &preview)) {
            return set_status(h, TOWER_STATUS_WALL);
        }
        /* Only a fight where not a single point of damage lands is refused.
         * A losing fight is legal: the preview already shows the damage. */
        if (h->state.attack <= preview.defence) {
            return set_status(h, TOWER_STATUS_TOO_STRONG);
        }
        h->state.phase = TOWER_PHASE_COMBAT;
        h->state.pending_x = (unsigned)x;
        h->state.pending_y = (unsigned)y;
        h->state.pending = preview;
        return set_status(h, TOWER_STATUS_CONFIRM);
    }

    if (tile >= TOWER_TILE_DOOR_YELLOW && tile <= TOWER_TILE_DOOR_RED) {
        color = (unsigned)(tile - TOWER_TILE_DOOR_YELLOW);
        if (!h->state.keys[color]) return set_status(h, TOWER_STATUS_NEED_KEY);
        --h->state.keys[color];
        status = TOWER_STATUS_DOOR;
    } else if (tile >= TOWER_TILE_KEY_YELLOW && tile <= TOWER_TILE_KEY_RED) {
        color = (unsigned)(tile - TOWER_TILE_KEY_YELLOW);
        ++h->state.keys[color];
        status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_POTION || tile == TOWER_TILE_BIG_POTION) {
        h->state.hp += tile == TOWER_TILE_POTION ? 30u : 80u;
        status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_ATTACK) {
        h->state.attack += 5;
        status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_DEFENCE) {
        h->state.defence += 3;
        status = TOWER_STATUS_ITEM;
    } else if (tile == TOWER_TILE_GOAL) {
        /* Only the final floor ends the campaign; every other goal closes its
         * chapter and leaves the tower open. */
        complete_chapter(h, h->state.floor,
                         h->state.floor + 1u == TOWER_FLOORS);
        status = h->state.cleared ? TOWER_STATUS_CLEARED
                                  : TOWER_STATUS_CHAPTER;
    } else if (tile == TOWER_TILE_SHOP) {
        status = TOWER_STATUS_SHOP;
    } else if (tile == TOWER_TILE_NPC) {
        status = TOWER_STATUS_TALK;
        if (h->state.floor==10u && x==1 && y==9 && tower_quest_state(h,0u)==1u) flag_set(h,10u,1u,9u);
        if (h->state.floor==20u && x==3 && y==10 && tower_quest_state(h,1u)==1u) flag_set(h,20u,3u,10u);
        if (h->state.floor==32u && x==9 && y==5 && tower_quest_state(h,2u)==1u) flag_set(h,32u,9u,5u);
    } else if (tile == TOWER_TILE_EVENT) {
        status = TOWER_STATUS_EVENT;
    }

    if (tile == TOWER_TILE_EVENT && h->state.floor == 13u && x == 3 && y == 4) {
        const unsigned index = 4u * TOWER_MAX_W + 3u;
        h->flags[13][index / 8u] ^= (uint8_t)(1u << (index % 8u));
    } else if (consumable(tile)) {
        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
    }
    h->state.x = (unsigned)x;
    h->state.y = (unsigned)y;

    if (tile == TOWER_TILE_UP) {
        if (h->state.floor + 1u < TOWER_FLOORS) {
            /* Taking a chapter's exit stair is an explicit completion event:
             * it is not inferred later from the explored bitmap. */
            const unsigned leaving = h->state.floor;
            enter_floor(h, leaving + 1u);
            if (leaving % 10u == 9u) complete_chapter(h, leaving, false);
            place_at_stairs(h, h->state.floor, TOWER_TILE_DOWN);
            status = TOWER_STATUS_STAIRS;
        }
    } else if (tile == TOWER_TILE_DOWN) {
        if (h->state.floor > 0) {
            enter_floor(h, h->state.floor - 1u);
            place_at_stairs(h, h->state.floor, TOWER_TILE_UP);
            status = TOWER_STATUS_STAIRS;
        }
    }

    advance_turn(h);
    return set_status(h, status);
}

tower_status_t tower_confirm(tower_handle_t h)
{
    unsigned floor, x, y;
    if (!h) return TOWER_STATUS_READY;
    if (h->state.phase != TOWER_PHASE_COMBAT) return h->state.status;

    /* Lethal damage ends the run: the enemy survives, no gold, no movement. */
    if (h->state.pending.damage >= h->state.hp) {
        h->state.hp = 0;
        clear_pending(h);
        advance_turn(h);
        return set_status(h, TOWER_STATUS_DEAD);
    }

    h->state.hp -= h->state.pending.damage;
    h->state.gold += h->state.pending.gold;
    floor = h->state.floor;
    x = h->state.pending_x;
    y = h->state.pending_y;
    h->state.x = x;
    h->state.y = y;
    flag_set(h, floor, x, y);
    if (h->state.pending.tile == TOWER_TILE_DEMON &&
        floor == TOWER_FLOORS - 1u) {
        complete_chapter(h, floor, true);
        clear_pending(h);
        advance_turn(h);
        return set_status(h, TOWER_STATUS_CLEARED);
    }
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
    if (h->state.hp == 0) return set_status(h, TOWER_STATUS_DEAD);
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

tower_status_t tower_teleport(tower_handle_t h, unsigned anchor)
{
    unsigned floor;
    if (!h) return TOWER_STATUS_WALL;
    if (h->state.hp == 0) return set_status(h, TOWER_STATUS_DEAD);
    if (h->state.phase != TOWER_PHASE_PLAY) return h->state.status;
    if (anchor >= TOWER_ANCHORS) return set_status(h, TOWER_STATUS_WALL);
    if (!tower_anchor_unlocked(&h->state, anchor)) {
        return set_status(h, TOWER_STATUS_LOCKED);
    }
    floor = tower_anchor_floor(anchor);
    /* Standing on the anchor already: a no-op must not look like a floor
     * change, because that is what triggers the caller's auto-save. */
    if (floor == h->state.floor) return set_status(h, TOWER_STATUS_READY);
    enter_floor(h, floor);
    place_at_stairs(h, floor, TOWER_TILE_DOWN);
    return set_status(h, TOWER_STATUS_STAIRS);
}

static void put32(uint8_t *b, unsigned offset, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) b[offset + i] = (uint8_t)(value >> (8u * i));
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
    for (i = 0; i < TOWER_SAVE_BYTES; ++i) {
        if (i < 8u || i > 11u) hash = (hash ^ b[i]) * 16777619u;
    }
    return hash;
}

/* Explicit little-endian fields avoid ABI, alignment, endianness and bool
 * representation issues. Field order is exactly tower_snapshot_t order. */
static void encode(const struct tower_t *h, uint8_t *b)
{
    const tower_snapshot_t *s = &h->state;
    memset(b, 0, TOWER_SAVE_BYTES);
    memcpy(b, "MTWR", 4);
    b[4] = (uint8_t)SAVE_VERSION;
    b[5] = 0;
    b[6] = (uint8_t)(TOWER_SAVE_BYTES & 0xffu);
    b[7] = (uint8_t)((TOWER_SAVE_BYTES >> 8) & 0xffu);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PHASE), (uint32_t)s->phase);
    put32(b, SAVE_FIELD_OFFSET(FIELD_STATUS), (uint32_t)s->status);
    put32(b, SAVE_FIELD_OFFSET(FIELD_FLOOR), s->floor);
    put32(b, SAVE_FIELD_OFFSET(FIELD_X), s->x);
    put32(b, SAVE_FIELD_OFFSET(FIELD_Y), s->y);
    put32(b, SAVE_FIELD_OFFSET(FIELD_HP), s->hp);
    put32(b, SAVE_FIELD_OFFSET(FIELD_ATTACK), s->attack);
    put32(b, SAVE_FIELD_OFFSET(FIELD_DEFENCE), s->defence);
    put32(b, SAVE_FIELD_OFFSET(FIELD_KEY_YELLOW), s->keys[0]);
    put32(b, SAVE_FIELD_OFFSET(FIELD_KEY_BLUE), s->keys[1]);
    put32(b, SAVE_FIELD_OFFSET(FIELD_KEY_RED), s->keys[2]);
    put32(b, SAVE_FIELD_OFFSET(FIELD_GOLD), s->gold);
    put32(b, SAVE_FIELD_OFFSET(FIELD_TURNS), s->turns);
    put32(b, SAVE_FIELD_OFFSET(FIELD_SHOP_ATTACK), s->shop_attack);
    put32(b, SAVE_FIELD_OFFSET(FIELD_SHOP_DEFENCE), s->shop_defence);
    put32(b, SAVE_FIELD_OFFSET(FIELD_CLEARED), (uint32_t)s->cleared);
    put32(b, SAVE_FIELD_OFFSET(FIELD_EXPLORED_LO), s->explored_lo);
    put32(b, SAVE_FIELD_OFFSET(FIELD_EXPLORED_HI), s->explored_hi);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_X), s->pending_x);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_Y), s->pending_y);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_TILE), (uint32_t)s->pending.tile);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_HP), s->pending.hp);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_ATTACK), s->pending.attack);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_DEFENCE), s->pending.defence);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_GOLD), s->pending.gold);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_DAMAGE), s->pending.damage);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_ROUNDS), s->pending.rounds);
    put32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_WINNABLE),
          (uint32_t)s->pending.winnable);
    put32(b, SAVE_FIELD_OFFSET(FIELD_CHAPTERS), s->chapters);
    put32(b, SAVE_RESERVED_OFFSET, 0);
    memcpy(b + SAVE_BITMAP_OFFSET, h->flags, sizeof h->flags);
    put32(b, 8, wire_checksum(b));
}

static void decode(struct tower_t *h, const uint8_t *b)
{
    tower_snapshot_t *s = &h->state;
    s->phase = (tower_phase_t)get32(b, SAVE_FIELD_OFFSET(FIELD_PHASE));
    s->status = (tower_status_t)get32(b, SAVE_FIELD_OFFSET(FIELD_STATUS));
    s->floor = get32(b, SAVE_FIELD_OFFSET(FIELD_FLOOR));
    s->x = get32(b, SAVE_FIELD_OFFSET(FIELD_X));
    s->y = get32(b, SAVE_FIELD_OFFSET(FIELD_Y));
    s->hp = get32(b, SAVE_FIELD_OFFSET(FIELD_HP));
    s->attack = get32(b, SAVE_FIELD_OFFSET(FIELD_ATTACK));
    s->defence = get32(b, SAVE_FIELD_OFFSET(FIELD_DEFENCE));
    s->keys[0] = get32(b, SAVE_FIELD_OFFSET(FIELD_KEY_YELLOW));
    s->keys[1] = get32(b, SAVE_FIELD_OFFSET(FIELD_KEY_BLUE));
    s->keys[2] = get32(b, SAVE_FIELD_OFFSET(FIELD_KEY_RED));
    s->gold = get32(b, SAVE_FIELD_OFFSET(FIELD_GOLD));
    s->turns = get32(b, SAVE_FIELD_OFFSET(FIELD_TURNS));
    s->shop_attack = get32(b, SAVE_FIELD_OFFSET(FIELD_SHOP_ATTACK));
    s->shop_defence = get32(b, SAVE_FIELD_OFFSET(FIELD_SHOP_DEFENCE));
    s->cleared = get32(b, SAVE_FIELD_OFFSET(FIELD_CLEARED)) != 0;
    s->explored_lo = get32(b, SAVE_FIELD_OFFSET(FIELD_EXPLORED_LO));
    s->explored_hi = get32(b, SAVE_FIELD_OFFSET(FIELD_EXPLORED_HI));
    s->pending_x = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_X));
    s->pending_y = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_Y));
    s->pending.tile =
        (tower_tile_t)get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_TILE));
    s->pending.hp = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_HP));
    s->pending.attack = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_ATTACK));
    s->pending.defence = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_DEFENCE));
    s->pending.gold = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_GOLD));
    s->pending.damage = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_DAMAGE));
    s->pending.rounds = get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_ROUNDS));
    s->pending.winnable =
        get32(b, SAVE_FIELD_OFFSET(FIELD_PENDING_WINNABLE)) != 0;
    s->chapters = get32(b, SAVE_FIELD_OFFSET(FIELD_CHAPTERS));
    memcpy(h->flags, b + SAVE_BITMAP_OFFSET, sizeof h->flags);
}

/* Reject a decoded save that the game could never have produced. Everything
 * is derived from the initial map plus the consumed bitmap, so the checks are
 * exact rather than heuristic:
 *   - a consumed bit may only cover a consumable/secret/event cell inside the
 *     floor's real shape; the reserved high bits are zero;
 *   - key/door and gold/shop arithmetic must balance;
 *   - attack/defence/hp follow from the consumed loot;
 *   - explored floors form a contiguous prefix (stairs move by one floor and
 *     teleport is limited to chapter anchors that were already reached), so the
 *     current floor is always covered and no unreachable floor can be marked;
 *   - the chapter mask only claims a chapter backed by evidence: its gate tile
 *     was consumed or the run left that chapter's exit stair. Chapter five
 *     additionally requires cleared. Chapter state stays its own saved field;
 *   - the player stands on a walkable tile and, if fighting, the pending
 *     preview is the one the model would compute;
 *   - cleared is set exactly when the goal on the last floor was stepped on or
 *     the demon king fell there. A goal on any other floor ends its chapter and
 *     must not end the campaign. */
static bool valid_state(const struct tower_t *h)
{
    const tower_snapshot_t *s = &h->state;
    uint32_t consumed[TOWER_TILE_COUNT];
    uint64_t explored, earned, bought, spent;
    uint32_t max_hp, goal_mask = 0;
    unsigned f, x, y, color, k, demon_last = 0, goal_last = 0;
    tower_tile_t current;
    bool expect_cleared;

    memset(consumed, 0, sizeof consumed);
    if (s->phase > TOWER_PHASE_COMBAT || s->status > TOWER_STATUS_CHAPTER ||
        s->floor >= TOWER_FLOORS || s->turns > MAX_TURNS ||
        s->chapters >= (1u << TOWER_CHAPTERS)) {
        return false;
    }

    for (f = 0; f < TOWER_FLOORS; ++f) {
        const tower_floor_desc_t *d = floor_desc(f);
        for (y = 0; y < TOWER_MAX_H; ++y) {
            for (x = 0; x < TOWER_MAX_W; ++x) {
                tower_tile_t original;
                if (!flag_get(h, f, x, y)) continue;
                if (!d || x >= (unsigned)d->w || y >= (unsigned)d->h) {
                    return false;
                }
                original = decode_tile(d->rows[y][x]);
                if (!consumable(original) && !(original==TOWER_TILE_NPC && campaign_npc(f,x,y))) return false;
                if (campaign_hidden(h,f,x,y)) return false;
                ++consumed[original];
                if (original == TOWER_TILE_DEMON && f == TOWER_FLOORS - 1u) {
                    ++demon_last;
                }
                if (original == TOWER_TILE_GOAL) {
                    goal_mask |= 1u << (f / 10u);
                    if (f == TOWER_FLOORS - 1u) ++goal_last;
                }
            }
        }
        for (k = TOWER_MAX_CELLS; k < TOWER_FLAG_BYTES * 8u; ++k) {
            if (h->flags[f][k / 8u] & (uint8_t)(1u << (k % 8u))) return false;
        }
    }

    for (color = 0; color < 3; ++color) {
        uint32_t keys = consumed[TOWER_TILE_KEY_YELLOW + color];
        uint32_t doors = consumed[TOWER_TILE_DOOR_YELLOW + color];
        if (doors > keys || s->keys[color] != keys - doors) return false;
    }

    earned = 0;
    for (f = TOWER_TILE_SLIME; f <= TOWER_TILE_DEMON; ++f) {
        earned += (uint64_t)enemy_gold((tower_tile_t)f) * consumed[f];
    }
    bought = (uint64_t)s->shop_attack + s->shop_defence;
    if (bought > 1000000u) return false;
    spent = bought * TOWER_SHOP_COST +
            (bought * (bought - 1u) / 2u) * TOWER_SHOP_STEP;
    if (earned < spent || (uint64_t)s->gold != earned - spent) return false;

    max_hp = 100u + 30u * consumed[TOWER_TILE_POTION] +
             80u * consumed[TOWER_TILE_BIG_POTION];
    if (s->hp > max_hp) return false;
    if (s->hp == 0) {
        if (s->status != TOWER_STATUS_DEAD ||
            s->phase != TOWER_PHASE_PLAY) {
            return false;
        }
    } else if (s->status == TOWER_STATUS_DEAD) {
        return false;
    }
    if ((uint64_t)s->attack != 12u + 5u * consumed[TOWER_TILE_ATTACK] +
                                     3u * s->shop_attack) {
        return false;
    }
    if ((uint64_t)s->defence != 4u + 3u * consumed[TOWER_TILE_DEFENCE] +
                                       3u * s->shop_defence) {
        return false;
    }

    expect_cleared = goal_last > 0 || demon_last > 0;
    if (s->cleared != expect_cleared) return false;

    explored = (uint64_t)s->explored_lo | ((uint64_t)s->explored_hi << 32);
    if ((explored >> TOWER_FLOORS) != 0) return false;
    if ((explored & (UINT64_C(1) << s->floor)) == 0) return false;
    k = 63;
    while (((explored >> k) & 1u) == 0) --k;
    if (((UINT64_C(1) << (k + 1u)) - 1u) != explored) return false;
    /* Chapter evidence. A consumed gate tile always records its chapter; a
     * chapter without a gate tile must have been left through its exit stair,
     * which is the only way the next chapter's first floor became explored.
     * Chapter five exists only once the campaign is cleared. */
    if ((s->chapters & (1u << (TOWER_CHAPTERS - 1u))) !=
        (s->cleared ? 1u << (TOWER_CHAPTERS - 1u) : 0u)) {
        return false;
    }
    for (k = 0; k + 1u < TOWER_CHAPTERS; ++k) {
        const uint32_t bit = 1u << k;
        if ((goal_mask & bit) && !(s->chapters & bit)) return false;
        if (!(s->chapters & bit) || (goal_mask & bit)) continue;
        if (!((explored >> (10u * (k + 1u))) & 1u)) return false;
    }

    if (s->x >= TOWER_MAX_W || s->y >= TOWER_MAX_H) return false;
    current = tile_at(h, s->floor, (int)s->x, (int)s->y);
    if (!player_standable(current) &&
        !(current == TOWER_TILE_EVENT && s->floor == 13u && s->x == 3u && s->y == 4u))
        return false;
    /* A looted three-mark room cannot exist without all three recorded marks. */
    if (flag_get(h,46u,9u,11u) && tower_sigils((tower_handle_t)h) != 7u) return false;
    if (flag_get(h,10u,1u,9u) && !flag_get(h,17u,7u,7u)) return false;
    if (flag_get(h,10u,6u,9u) && tower_quest_state((tower_handle_t)h,0u)!=2u) return false;
    if (flag_get(h,20u,3u,10u) && !flag_get(h,33u,3u,5u)) return false;
    if (flag_get(h,20u,3u,8u) && tower_quest_state((tower_handle_t)h,1u)!=2u) return false;
    if (flag_get(h,32u,9u,5u) && !flag_get(h,42u,13u,11u)) return false;
    if (flag_get(h,32u,10u,11u) && tower_quest_state((tower_handle_t)h,2u)!=2u) return false;

    if (s->phase == TOWER_PHASE_COMBAT) {
        tower_enemy_preview_t p;
        int dx, dy;
        if (s->status != TOWER_STATUS_CONFIRM) return false;
        if (s->pending_x >= TOWER_MAX_W || s->pending_y >= TOWER_MAX_H) {
            return false;
        }
        dx = (int)s->pending_x - (int)s->x;
        dy = (int)s->pending_y - (int)s->y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx + dy != 1) return false;
        if (!preview_for(s->floor, s,
                         tile_at(h, s->floor, (int)s->pending_x,
                                 (int)s->pending_y), &p)) {
            return false;
        }
        if (s->attack <= p.defence) return false;
        if (s->pending.tile != p.tile || s->pending.hp != p.hp ||
            s->pending.attack != p.attack || s->pending.defence != p.defence ||
            s->pending.gold != p.gold || s->pending.damage != p.damage ||
            s->pending.rounds != p.rounds ||
            s->pending.winnable != p.winnable) {
            return false;
        }
    } else {
        if (s->status == TOWER_STATUS_CONFIRM) return false;
        if (s->pending_x || s->pending_y || s->pending.tile ||
            s->pending.hp || s->pending.attack || s->pending.defence ||
            s->pending.gold || s->pending.damage || s->pending.rounds ||
            s->pending.winnable) {
            return false;
        }
    }
    return true;
}

tower_result_t tower_export(tower_handle_t h, uint8_t *bytes, size_t size)
{
    if (!h || !bytes || size != TOWER_SAVE_BYTES) return TOWER_ERR_ARGUMENT;
    encode(h, bytes);
    return TOWER_OK;
}

tower_result_t tower_import(tower_handle_t h, const uint8_t *bytes, size_t size)
{
    tower_handle_t candidate;
    if (!h || !bytes) return TOWER_ERR_ARGUMENT;
    if (size != TOWER_SAVE_BYTES) return TOWER_ERR_SAVE;
    if (memcmp(bytes, "MTWR", 4) != 0 || bytes[4] != (uint8_t)SAVE_VERSION ||
        bytes[5] != 0) {
        return TOWER_ERR_SAVE;
    }
    if (((uint32_t)bytes[6] | (uint32_t)bytes[7] << 8) != TOWER_SAVE_BYTES) {
        return TOWER_ERR_SAVE;
    }
    if (get32(bytes, 8) != wire_checksum(bytes)) return TOWER_ERR_SAVE;
    if (get32(bytes, SAVE_RESERVED_OFFSET) != 0) return TOWER_ERR_SAVE;
    /* bool fields are stored as u32; anything but 0/1 is not a valid save. */
    if (get32(bytes, SAVE_FIELD_OFFSET(FIELD_CLEARED)) > 1u ||
        get32(bytes, SAVE_FIELD_OFFSET(FIELD_PENDING_WINNABLE)) > 1u) {
        return TOWER_ERR_SAVE;
    }
    candidate = calloc(1, sizeof *candidate);
    if (!candidate) return TOWER_ERR_MEMORY;
    decode(candidate, bytes);
    if (!valid_state(candidate)) {
        tower_delete(candidate);
        return TOWER_ERR_SAVE;
    }
    h->state = candidate->state;
    memcpy(h->flags, candidate->flags, sizeof h->flags);
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
    case TOWER_STATUS_DEAD: return "You have fallen";
    case TOWER_STATUS_SECRET: return "A secret passage opens";
    case TOWER_STATUS_EVENT: return "Something stirs";
    case TOWER_STATUS_LOCKED: return "This anchor is not unlocked";
    case TOWER_STATUS_CHAPTER: return "Chapter cleared: anchor lit";
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
