/* Replay all Python-planned fork choices through the shipped C model.
 * The runner creates strategy_routes.h in a temporary build directory. */
#include "tower.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const signed char *actions;
    size_t count;
    uint32_t floor, x, y, hp, attack, defence, keys[3], gold, turns;
    bool cleared;
    bool mage_on_six, golem_on_nine;
} strategy_t;

#include "strategy_routes.h"

static tower_snapshot_t read_state(tower_handle_t tower)
{
    tower_snapshot_t state;
    assert(tower_read(tower, &state) == TOWER_OK);
    return state;
}

static void replay(const strategy_t *route)
{
    tower_handle_t tower = NULL, restored = NULL;
    uint8_t wire[TOWER_SAVE_BYTES], reencoded[TOWER_SAVE_BYTES];
    tower_snapshot_t state;
    size_t i;
    assert(tower_create(NULL, &tower) == TOWER_OK);
    assert(tower_create(NULL, &restored) == TOWER_OK);
    for (i = 0; i < route->count; ++i) {
        signed char action = route->actions[i];
        if (action == 6) {
            (void)tower_confirm(tower);
        } else {
            int dx = action == 0 ? -1 : action == 1 ? 1 : 0;
            int dy = action == 2 ? -1 : action == 5 ? 1 : 0;
            tower_status_t status;
            assert(action == 0 || action == 1 || action == 2 || action == 5);
            status = tower_move(tower, dx, dy);
            if (status == TOWER_STATUS_WALL || status == TOWER_STATUS_NEED_KEY ||
                status == TOWER_STATUS_TOO_STRONG) {
                fprintf(stderr, "%s action %zu refused: %d\n", route->name, i, status);
                assert(false);
            }
        }
        /* Exercise both pending combat and normal saves, including map changes.
         * Continue from the imported handle so every later step uses recovery. */
        assert(tower_export(tower, wire, sizeof wire) == TOWER_OK);
        assert(tower_import(restored, wire, sizeof wire) == TOWER_OK);
        assert(tower_export(restored, reencoded, sizeof reencoded) == TOWER_OK);
        assert(memcmp(wire, reencoded, sizeof wire) == 0);
        {
            tower_handle_t previous = tower;
            tower = restored;
            restored = previous;
        }
    }
    state = read_state(tower);
    assert(state.phase == TOWER_PHASE_PLAY && state.cleared == route->cleared);
    assert(state.floor == route->floor && state.x == route->x && state.y == route->y);
    assert(state.hp == route->hp && state.attack == route->attack);
    assert(state.defence == route->defence && state.gold == route->gold);
    assert(state.turns == route->turns && state.hp > 0);
    assert(state.keys[0] == route->keys[0] && state.keys[1] == route->keys[1] &&
           state.keys[2] == route->keys[2]);
    assert(state.shop_attack == 0 && state.shop_defence == 0);
    /* Each strategy must actually take its chosen fork and leave the other
     * guardian standing, rather than silently clearing both routes. */
    assert(tower_tile(tower, 5, 3, 5) ==
           (route->mage_on_six ? TOWER_TILE_FLOOR : TOWER_TILE_MAGE));
    assert(tower_tile(tower, 5, 7, 5) ==
           (route->mage_on_six ? TOWER_TILE_GOLEM : TOWER_TILE_FLOOR));
    assert(tower_tile(tower, 8, 3, 5) ==
           (route->golem_on_nine ? TOWER_TILE_FLOOR : TOWER_TILE_GOLEM));
    assert(tower_tile(tower, 8, 7, 5) ==
           (route->golem_on_nine ? TOWER_TILE_WRAITH : TOWER_TILE_FLOOR));
    printf("PASS: %s, %zu actions, HP %lu, save/recovery after every action\n",
           route->name, route->count, (unsigned long)state.hp);
    tower_delete(restored);
    tower_delete(tower);
}

int main(void)
{
    size_t i;
    for (i = 0; i < sizeof STRATEGIES / sizeof STRATEGIES[0]; ++i)
        replay(&STRATEGIES[i]);
    return 0;
}
