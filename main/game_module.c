// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
#include "campaign_dialogue.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#include "host_storage.h"
#include "game_preferences.h"
typedef struct {
    game_handle_t game;
    char save_path[768];
    char preferences_path[784];
} host_state_t;
static bool save_preferences(void *context,const game_preferences_t *value)
{
    host_state_t *s=context;
    uint8_t bytes[GAME_PREFERENCES_BYTES];
    return game_preferences_encode(value,bytes) &&
        game_host_save(s->preferences_path,bytes,sizeof bytes);
}
static bool load_preferences(void *context,game_preferences_t *out)
{
    host_state_t *s=context;
    uint8_t bytes[GAME_PREFERENCES_BYTES];
    return game_host_load(s->preferences_path,bytes,sizeof bytes) &&
        game_preferences_decode(bytes,out);
}
static bool set_brightness(int value) { return value>=10 && value<=100; }
static int initialize(void *value,const char *asset_root)
{
    (void)asset_root;
    host_state_t *s=value;
    /* __FILE__ is absolute in the native simulator's compiler command. */
    if(snprintf(s->save_path,sizeof(s->save_path),"%s",__FILE__) >= (int)sizeof(s->save_path)) return -1;
    for(int i=0;i<2;++i) {
        char *a=strrchr(s->save_path,'/'), *b=strrchr(s->save_path,'\\');
        char *end=a && b?(a>b?a:b):(a?a:b);
        if(!end) return -1;
        *end='\0';
    }
    const size_t length=strlen(s->save_path);
    if(length+sizeof("/magic_tower.save")>sizeof(s->save_path)) return -1;
    memcpy(s->save_path+length,"/magic_tower.save",sizeof("/magic_tower.save"));
    if(snprintf(s->preferences_path,sizeof(s->preferences_path),"%s.prefs",s->save_path) >=
       (int)sizeof(s->preferences_path)) return -1;
    const game_config_t config={.width=GAME_WIDTH,.height=GAME_HEIGHT,
        .save=game_host_save,.load=game_host_load,.storage_context=s->save_path,
        .load_preferences=load_preferences,.save_preferences=save_preferences,
        .preferences_context=s,.set_brightness=set_brightness};
    if(!game_create(&config,&s->game)) return -1;
    InitWindow(GAME_WIDTH,GAME_HEIGHT,GAME_NAME);
    SetTargetFPS(GAME_TICK_HZ);
    return 0;
}
static void shutdown(void *value)
{
    host_state_t *s=value; game_delete(s->game); s->game=NULL;
}
static void input(void *value,const mosaico_host_input_v1_t *event)
{
    host_state_t *s=value;
    if(!event) return;
    if(event->type==MOSAICO_HOST_INPUT_POINTER && event->track_id==0)
        game_set_pointer(s->game,event->x,event->y,event->pressed);
    else if(event->type==MOSAICO_HOST_INPUT_ACTION) game_set_action(s->game,event->code,event->pressed);
    else if(event->type==MOSAICO_HOST_INPUT_CONTROL) {
        if(event->code==MOSAICO_HOST_CONTROL_PAUSE) game_set_paused(s->game,true);
        else if(event->code==MOSAICO_HOST_CONTROL_RESUME) game_set_paused(s->game,false);
        else if(event->code==MOSAICO_HOST_CONTROL_RESET) game_reset(s->game);
    }
}
static void update(void *value) { game_update(((host_state_t *)value)->game); }
static int render(void *value) { return game_render(((host_state_t *)value)->game)?0:-1; }
static uint32_t state_hash(const void *value) { return game_hash(((const host_state_t *)value)->game); }
static int state_json(const void *value,char *output,size_t capacity)
{
    if(!output || !capacity) return -1;
    const host_state_t *host=value;
    const game_snapshot_t *s=game_read(host->game);
    const char *screen=s->screen==GAME_TITLE?"title":s->screen==GAME_NEW_CONFIRM?"new_confirm":
        s->screen==GAME_SHOP?"shop":s->screen==GAME_DIALOG?"dialog":s->screen==GAME_OVER?"over":
        s->screen==GAME_VICTORY?"victory":s->screen==GAME_TELEPORT?"teleport":
        s->screen==GAME_RESTART_MENU?"restart_menu":"dungeon";
    const char *phase=s->screen==GAME_TITLE?"title":s->screen==GAME_NEW_CONFIRM?"new_confirm":s->screen==GAME_SHOP?"shop":
        s->tower.phase==TOWER_PHASE_COMBAT?"combat":s->paused?"paused":"playing";
    const int size=snprintf(output,capacity,
        "{\"phase\":\"%s\",\"screen\":\"%s\",\"tick\":%lu,\"floor\":%lu,\"floors\":%d,\"x\":%lu,\"y\":%lu,"
        "\"hp\":%lu,\"attack\":%lu,\"defence\":%lu,\"keys\":[%lu,%lu,%lu],\"gold\":%lu,"
        "\"turns\":%lu,\"cleared\":%s,\"damage\":%lu,\"status\":%d,\"has_save\":%s,\"has_checkpoint\":%s,"
        "\"chapters\":%lu,\"anchors\":[%d,%d,%d,%d,%d,%d],\"anchor_cursor\":%u,"
        "\"shop_attack\":%lu,\"shop_defence\":%lu,"
        "\"lang\":\"%s\",\"notice\":%d,\"pointer_down\":%s,\"settings_open\":%s,"
        "\"volume\":%u,\"brightness\":%u,\"preferences_save_failed\":%s,"
        "\"battery_percent\":%u,\"battery_state\":%u,\"music\":%u,"
        "\"camera\":[%u,%u],\"floor_size\":[%u,%u],\"dialogue_id\":%u,\"dialogue_page\":%u,\"dialogue_pages\":%u,\"dialogue_variant\":%u,"
        "\"state_hash\":\"%08lx\"}",phase,screen,
        (unsigned long)s->tick,(unsigned long)s->tower.floor,TOWER_FLOORS,
        (unsigned long)s->tower.x,(unsigned long)s->tower.y,
        (unsigned long)s->tower.hp,(unsigned long)s->tower.attack,(unsigned long)s->tower.defence,
        (unsigned long)s->tower.keys[0],(unsigned long)s->tower.keys[1],(unsigned long)s->tower.keys[2],
        (unsigned long)s->tower.gold,(unsigned long)s->tower.turns,s->tower.cleared?"true":"false",
        (unsigned long)s->tower.pending.damage,(int)s->tower.status,s->has_save?"true":"false",
        s->has_checkpoint?"true":"false",(unsigned long)s->tower.chapters,
        tower_anchor_unlocked(&s->tower,0),tower_anchor_unlocked(&s->tower,1),
        tower_anchor_unlocked(&s->tower,2),tower_anchor_unlocked(&s->tower,3),
        tower_anchor_unlocked(&s->tower,4),tower_anchor_unlocked(&s->tower,5),
        s->anchor_cursor,(unsigned long)s->tower.shop_attack,(unsigned long)s->tower.shop_defence,
        s->lang==GAME_LANG_EN?"en":"zh",(int)s->notice_id,
        s->pointer_down?"true":"false",s->settings_open?"true":"false",
        s->volume,s->brightness,s->preferences_save_failed?"true":"false",
        s->battery_percent,(unsigned)s->battery_state,s->music,
        s->camera_x,s->camera_y,s->floor_w,s->floor_h,s->dialogue_id,s->dialogue_page,
        campaign_dialogue_pages(s->dialogue_id,s->dialogue_variant),s->dialogue_variant,(unsigned long)state_hash(value));
    return size<0 || (size_t)size>=capacity?-1:size;
}
static const mosaico_game_module_v1_t s_module={
    .descriptor={MOSAICO_HOST_GAME_ABI_V1,GAME_NAME,"Magic Tower - Fifty Floors",GAME_WIDTH,GAME_HEIGHT,GAME_TICK_HZ,1},
    .state_size=sizeof(host_state_t),.initialize=initialize,.shutdown=shutdown,.input=input,
    .update=update,.render=render,.state_hash=state_hash,.state_json=state_json
};
const mosaico_game_module_v1_t *mosaico_game_module_v1(void) { return &s_module; }
