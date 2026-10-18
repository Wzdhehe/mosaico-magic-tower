"""Designer-owned numerical table and exact C integration patch.

Only a lab snapshot is transformed. Production integration is a separate step.
The real C movement/combat/save functions remain the implementation under test.
"""
# Columns are HP/attack/defence. Rows s,v,k,X,m,g,w,M. Gold remains the
# existing unscaled C table (3/6/12/60/18/26/34/150); shops remain 15+10*n.
REGIONAL_ENEMIES = [
    [(40,13,5),(60,25,9),(100,36,14),(600,120,90),(160,39,18),(200,48,28),(260,52,30),(700,150,100)],
    [(90,36,16),(110,49,22),(190,62,30),(900,200,140),(240,63,35),(300,79,48),(370,86,55),(1100,220,150)],
    [(140,60,30),(200,78,38),(300,94,50),(1200,230,190),(360,103,56),(450,121,72),(550,128,78),(1400,250,180)],
    [(200,88,45),(280,108,56),(400,128,70),(1200,230,160),(460,139,78),(560,151,95),(700,158,100),(1200,170,110)],
]
BOSSES = {20:('g',260,62,24),30:('g',400,90,43),40:('w',650,126,70),50:('M',1200,170,110)}
# NPC, later quest object, reward-mouth. Coordinates are (zero-based floor,x,y).
QUESTS = [((10,1,9),(17,7,7),(10,6,9)),
          ((20,3,10),(33,3,5),(20,3,8)),
          ((32,9,5),(42,13,11),(32,10,11))]
HIDDEN = [(7,(2,7),[(1,8,3,9)]),(10,(6,9),[(7,7,11,9)]),
          (17,(4,5),[(5,5,7,7)]),(20,(3,8),[(1,5,3,7)]),
          (27,(11,8),[(9,5,13,7)]),(32,(10,11),[(11,11,11,12),(11,13,13,15)]),
          (36,(5,6),[(3,7,7,9)]),(46,(9,11),[(10,11,10,11),(11,11,13,15)])]

def replace_once(source, old, new):
    assert source.count(old)==1, ('C integration anchor drift',old[:100],source.count(old))
    return source.replace(old,new,1)

def transform(source):
    source=replace_once(source,'#define SAVE_VERSION 7u','#define SAVE_VERSION 8u /* authored campaign revision */')
    # Keep the original first-chapter arithmetic byte-for-byte equivalent.
    table=',\n'.join('{'+','.join('{'+','.join(map(str,v))+'}' for v in region)+'}' for region in REGIONAL_ENEMIES)
    old='    p->gold = enemy_gold(tile);'
    new='''    /* Fixed regional identities let players compare upgrade thresholds.
     * The first chapter retains its already verified floor scaling. */
    if (floor >= 10u) {
        static const uint32_t profile[4][8][3] = {'''+table+'''};
        const uint32_t *v = profile[floor / 10u - 1u][(unsigned)tile - TOWER_TILE_SLIME];
        p->hp = v[0]; p->attack = v[1]; p->defence = v[2];
    }
'''
    for floor,(tile,hp,attack,defence) in BOSSES.items():
        enum={'g':'GOLEM','w':'WRAITH','M':'DEMON'}[tile]
        new+=f'    if (floor == {floor-1}u && tile == TOWER_TILE_{enum}) {{ p->hp={hp}u; p->attack={attack}u; p->defence={defence}u; }}\n'
    source=replace_once(source,old,new+old)
    # State reuses legal, existing event-cell flags. No new save field or
    # changes to resource validation; v8 still has 2632 bytes, but rejects v7.
    marker='static tower_tile_t tile_at('
    helpers='''/* Sluice is the only reversible event. Marks are consumed event cells. */
bool tower_sluice_south(tower_handle_t h) {
    return h && flag_get(h, 13u, 3u, 4u);
}
unsigned tower_sigils(tower_handle_t h) {
    if (!h) return 0u;
    return (flag_get(h,11u,4u,3u)?1u:0u) |
           (flag_get(h,26u,5u,6u)?2u:0u) |
           (flag_get(h,34u,11u,9u)?4u:0u);
}

'''
    helpers+='static bool campaign_npc(unsigned f,unsigned x,unsigned y) {\n    return '+ ' || '.join(f'(f=={n[0]}u && x=={n[1]}u && y=={n[2]}u)' for n,_,_ in QUESTS)+';\n}\n'
    helpers+='unsigned tower_quest_state(tower_handle_t h,unsigned q) {\n    if (!h) return 0u;\n    switch(q) {\n'
    for q,(npc,item,gate) in enumerate(QUESTS):
        helpers+=f'    case {q}u: return flag_get(h,{npc[0]}u,{npc[1]}u,{npc[2]}u)?2u:flag_get(h,{item[0]}u,{item[1]}u,{item[2]}u)?1u:0u;\n'
    helpers+='    default: return 0u;\n    }\n}\n\n'
    helpers+='static bool campaign_hidden(const struct tower_t *h,unsigned f,unsigned x,unsigned y) {\n'
    for floor,(gx,gy),rects in HIDDEN:
        conditions=' || '.join(f'(x>={x1}u && x<={x2}u && y>={y1}u && y<={y2}u)' for x1,y1,x2,y2 in rects)
        helpers+=f'    if (f=={floor}u && !flag_get(h,{floor}u,{gx}u,{gy}u) && ({conditions})) return true;\n'
    helpers+='    return false;\n}\n\n'
    source=replace_once(source,marker,helpers+marker)
    old='    if (flag_get(h, floor, (unsigned)x, (unsigned)y)) return TOWER_TILE_FLOOR;'
    new='''    if (campaign_hidden(h,floor,(unsigned)x,(unsigned)y)) return TOWER_TILE_WALL;
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
'''
    for q,(_,_,gate) in enumerate(QUESTS):
        new+=f'    if (floor=={gate[0]}u && x=={gate[1]} && y=={gate[2]} && tower_quest_state((tower_handle_t)h,{q}u)!=2u) return TOWER_TILE_WALL;\n'
    new+=old
    source=replace_once(source,old,new)
    source=replace_once(source,'    /* A secret wall opens on contact but does not pull the player through. */',
        '    /* Entering a darker floor mouth reveals its authored chamber. */')
    source=replace_once(source,'''        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
        advance_turn(h);
        return set_status(h, TOWER_STATUS_SECRET);''',
        '''        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
        h->state.x = (unsigned)x;
        h->state.y = (unsigned)y;
        advance_turn(h);
        return set_status(h, TOWER_STATUS_SECRET);''')
    old='''    } else if (tile == TOWER_TILE_NPC) {
        status = TOWER_STATUS_TALK;'''
    new=old+'\n'
    for q,(npc,item,_) in enumerate(QUESTS):
        new+=f'        if (h->state.floor=={npc[0]}u && x=={npc[1]} && y=={npc[2]} && tower_quest_state(h,{q}u)==1u) flag_set(h,{npc[0]}u,{npc[1]}u,{npc[2]}u);\n'
    source=replace_once(source,old,new.rstrip())
    source=replace_once(source,'                if (!consumable(original)) return false;',
        '                if (!consumable(original) && !(original==TOWER_TILE_NPC && campaign_npc(f,x,y))) return false;\n                if (campaign_hidden(h,f,x,y)) return false;')
    old='''    if (consumable(tile)) {
        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
    }'''
    new='''    if (tile == TOWER_TILE_EVENT && h->state.floor == 13u && x == 3 && y == 4) {
        const unsigned index = 4u * TOWER_MAX_W + 3u;
        h->flags[13][index / 8u] ^= (uint8_t)(1u << (index % 8u));
    } else if (consumable(tile)) {
        flag_set(h, h->state.floor, (unsigned)x, (unsigned)y);
    }'''
    source=replace_once(source,old,new)
    # The single persistent valve is a legal standing tile even in its
    # unset state; all other events are consumed before standing on them.
    old='    if (!player_standable(current)) return false;'
    new='''    if (!player_standable(current) &&
        !(current == TOWER_TILE_EVENT && s->floor == 13u && s->x == 3u && s->y == 4u))
        return false;
    /* A looted three-mark room cannot exist without all three recorded marks. */
    if (flag_get(h,46u,9u,11u) && tower_sigils((tower_handle_t)h) != 7u) return false;'''
    for q,(npc,item,gate) in enumerate(QUESTS):
        new+=f'\n    if (flag_get(h,{npc[0]}u,{npc[1]}u,{npc[2]}u) && !flag_get(h,{item[0]}u,{item[1]}u,{item[2]}u)) return false;'
        new+=f'\n    if (flag_get(h,{gate[0]}u,{gate[1]}u,{gate[2]}u) && tower_quest_state((tower_handle_t)h,{q}u)!=2u) return false;'
    source=replace_once(source,old,new)
    return source
