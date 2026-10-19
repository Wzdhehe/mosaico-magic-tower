"""Replay an approved route through actual game input, dialogs and autosaves."""
import ctypes as C
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/campaign-game'
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    cc=os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    env=os.environ.copy();env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','');env['TMP']=env['TEMP']=str(OUT)
    modern='dialogue_id;' in (ROOT/'main/game.h').read_text(encoding='utf-8')
    meta='return s->dialogue_id;' if modern else 'return 65535u;'
    page='return s->dialogue_page;' if modern else 'return 0u;'
    variant='return s->dialogue_variant;' if modern else 'return 0u;'
    bridge=r'''#include "game.h"
#include "game_config.h"
#include "campaign_dialogue.h"
#include <string.h>
static game_handle_t g; static unsigned saves; static unsigned char bytes[TOWER_SAVE_BYTES];
static bool has;
static bool store(void*c,const uint8_t*b,size_t n){(void)c;memcpy(bytes,b,n);has=true;++saves;return true;}
static bool load(void*c,uint8_t*b,size_t n){(void)c;if(!has)return false;memcpy(b,bytes,n);return true;}
int begin(void){game_config_t c={.width=480,.height=480,.save=store,.load=load};return game_create(&c,&g);}
void end(void){game_delete(g);}
void key(int a){game_set_action(g,a,true);game_set_action(g,a,false);game_update(g);}
void click(int x,int y){game_set_pointer(g,x,y,true);game_set_pointer(g,0,0,false);game_update(g);}
unsigned val(unsigned f){const game_snapshot_t*s=game_read(g);unsigned v[]={s->screen,s->tower.floor,s->tower.x,s->tower.y,s->tower.hp,s->tower.attack,s->tower.defence,s->tower.gold,s->tower.turns,s->tower.phase,s->tower.cleared,saves,s->camera_x,s->camera_y,s->tower.chapters,s->music};return v[f];}
unsigned reset_action(void){return GAME_ACTION_RESET;}
unsigned dialog(void){const game_snapshot_t*s=game_read(g);(void)s; META}
unsigned page(void){const game_snapshot_t*s=game_read(g);(void)s; PAGE}
unsigned variant(void){const game_snapshot_t*s=game_read(g);(void)s; VARIANT}
unsigned pages(void){return campaign_dialogue_pages((uint16_t)dialog(),variant());}
unsigned screen_dialog(void){return GAME_DIALOG;} unsigned screen_shop(void){return GAME_SHOP;}
unsigned screen_dungeon(void){return GAME_DUNGEON;} unsigned screen_victory(void){return GAME_VICTORY;}
'''.replace('META',meta).replace('PAGE',page).replace('VARIANT',variant)
    (OUT/'bridge.c').write_text(bridge,encoding='utf-8')
    binary=OUT/('game.dll' if os.name=='nt' else'game.so')
    cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2','-shared']
    if os.name!='nt':cmd+=['-fPIC']
    cmd+=['-I',str(ROOT/'main'),str(OUT/'bridge.c'),*[str(ROOT/'main'/n) for n in ('game.c','tower.c','tower_floors.c')],'-o',str(binary)]
    subprocess.run(cmd,env=env,check=True)
    if os.name=='nt':C_dir=os.add_dll_directory(str(Path(cc).parent))
    lib=C.CDLL(str(binary));lib.val.argtypes=[C.c_uint];lib.val.restype=C.c_uint
    lib.key.argtypes=[C.c_int];lib.click.argtypes=[C.c_int,C.c_int]
    assert lib.begin();assert lib.val(15)==0;lib.key(6);assert lib.val(11)==1
    trace=json.loads((ROOT/'artifacts/campaign-production/secrets_and_errands.json').read_text(encoding='utf-8'))
    dialog_screen,shop,dungeon,victory=[getattr(lib,'screen_'+n)() for n in ('dialog','shop','dungeon','victory')]
    turns_written=1;diags=set();deliveries=set();gpt_pages=[];teaching=False;modal_checked=set();tracks={0}
    def restart_cancel():
        screen=lib.val(0)
        if screen in modal_checked:return
        before=[lib.val(i) for i in range(1,16)]+[lib.dialog(),lib.page(),lib.variant()]
        lib.key(lib.reset_action());assert lib.val(0)==8
        lib.key(5);assert lib.val(0)==2  # New game still needs confirmation.
        lib.key(0);assert lib.val(0)==8
        lib.key(0)
        assert lib.val(0)==screen,('restart cancel lost originating modal',screen,lib.val(0))
        assert [lib.val(i) for i in range(1,16)]+[lib.dialog(),lib.page(),lib.variant()]==before
        modal_checked.add(screen)
    try:
        for a in trace['actions']:
            action=a['action'];kind=action[0]
            while lib.val(0)==dialog_screen:
                restart_cancel()
                f,x,y=lib.val(1),lib.val(2),lib.val(3)
                if (f,x,y)==(7,3,6) and not teaching:
                    lib.key(6)
                    assert lib.val(0)==dialog_screen,'NPC teaching extra page was discarded instead of advanced'
                    teaching=True
                if modern:
                    assert lib.dialog()!=65535,'A dialog has no coordinate-dispatched authored entry'
                    diags.add(lib.dialog())
                    if (f,x,y)==(46,12,14):gpt_pages.append(lib.page())
                    if (f,x,y) in ((10,1,9),(20,3,10),(32,9,5)) and lib.variant()==1:deliveries.add((f,x,y))
                    prior=lib.page();total=lib.pages();lib.key(6)
                    if prior+1<total:assert lib.val(0)==dialog_screen and lib.page()==prior+1
                    else:assert lib.val(0)==dungeon
                else:lib.key(6)
            if lib.val(0) in (victory,shop):restart_cancel()
            if lib.val(0)==victory:lib.key(6)
            if kind=='move':
                if lib.val(0)==shop:lib.key(0)
                dx,dy=action[1:];lib.key({(-1,0):0,(1,0):1,(0,-1):2,(0,1):5}[(dx,dy)])
            elif kind=='confirm':lib.key(6)
            elif kind=='buy':
                assert lib.val(0)==shop
                lib.click(230,240 if action[1]==0 else 282)
            elif kind=='warp':
                if lib.val(0)==shop:lib.key(0)
                lib.click(185,460)
                index=action[1];lib.click(78+(index%2)*174+75,152+(index//2)*70+28)
            else:raise AssertionError(action)
            after=a['after']
            expected_music=8 if lib.val(0)==victory else (7 if after['cleared'] else (6 if after['floor']==49 else 1+after['floor']//10))
            assert lib.val(15)==expected_music,('music routing',after['floor'],lib.val(15),expected_music)
            tracks.add(lib.val(15))
            for index,name in ((1,'floor'),(2,'x'),(3,'y'),(4,'hp'),(5,'attack'),(6,'defence'),(7,'gold'),(8,'turns')):
                assert lib.val(index)==after[name],(action,name,lib.val(index),after[name])
            if kind in ('move','warp') and after['floor']!=a['before']['floor']:turns_written+=1
            assert lib.val(11)==turns_written,('unexpected save count',action,lib.val(11),turns_written)
        assert teaching and len(deliveries)==3
        assert gpt_pages==[0,1,2],('GPT pages were not visited',gpt_pages)
        if lib.val(0)==victory:
            lib.key(6)
            assert lib.val(15)==7,'Continued exploration must restore Home'
            tracks.add(lib.val(15))
        assert tracks==set(range(9)),('Not all region/boss/victory/exploration tracks were routed',tracks)
        assert lib.val(10) and lib.val(14)==31 and (lib.val(12),lib.val(13))==(0,0)
        print('PASS: actual game input reaches 50F; teaching pages, three deliveries, autosave counts, final11x11 camera')
        print('Dialogs encountered:',len(diags),'checkpoint writes:',turns_written)
    finally:lib.end()
if __name__=='__main__':main()
