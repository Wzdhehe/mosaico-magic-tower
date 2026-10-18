#!/usr/bin/env python3
"""Render every authored NPC page through the real C story modal/RGB565 engine.

All changes are confined to an immutable lab snapshot. The test only injects
the selected authored strings into existing STORY/NPC_NAME slots, observes real
ui_draw calls, and compares rendered pixels. No alternate text/map renderer.
"""
import contextlib
import ctypes as C
import hashlib
import importlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets_src/level_design'))
from campaign_content import NPCS
from campaign_presentation import transform_view, transform_config
from campaign_rules import transform
from capture_campaign_design import engine_root, rgb565_image
from tower_design_lab import build
from verify_campaign_design import FLOORS

OUT = ROOT/'artifacts/campaign-dialog-layout'
BODY = (150,174,438,316)
NAME = (48,138,434,162)

TRACE_PREFIX = r'''
typedef struct {int x,y,scale,width;char text[512];} dialog_draw_t;
static dialog_draw_t dialog_draws[32];
static unsigned dialog_draw_count;
static bool dialog_trace_active;
'''
TRACE_WRAPPER = r'''
/* Observation only: the original drawing implementation still paints pixels. */
static void ui_draw(const char *text,int x,int y,int scale,Color color)
{
 if(dialog_trace_active && x==150 && y>=174 && dialog_draw_count<32u) {
  dialog_draw_t *d=&dialog_draws[dialog_draw_count++];
  d->x=x;d->y=y;d->scale=scale;d->width=ui_width(text,scale);
  snprintf(d->text,sizeof(d->text),"%s",text?text:"");
 }
 dialog_ui_draw_original(text,x,y,scale,color);
}
'''
FACADE = r'''
#include "tower_floors.h"
#include "mosaico_raylib_port.h"
static uint16_t dialog_pixels[GAME_WIDTH * GAME_HEIGHT];
static tower_handle_t dialog_model;
static game_snapshot_t dialog_snapshot;
static bool dialog_dirty;
esp_err_t mosaico_raylib_port_begin_frame(uint16_t **p,size_t *stride) {
 *p=dialog_pixels;*stride=GAME_WIDTH;return ESP_OK;
}
esp_err_t mosaico_raylib_port_present_frame(void) {return ESP_OK;}
const game_snapshot_t *game_read(game_handle_t game) {(void)game;return &dialog_snapshot;}
tower_tile_t game_tile(game_handle_t game,int x,int y) {
 (void)game;return tower_tile(dialog_model,dialog_snapshot.tower.floor,x,y);
}
bool game_needs_redraw(game_handle_t game) {(void)game;return dialog_dirty;}
void game_render_complete(game_handle_t game) {(void)game;dialog_dirty=false;}
int dialog_init(void) {
 if(tower_create(NULL,&dialog_model)!=TOWER_OK)return -1;
 InitWindow(GAME_WIDTH,GAME_HEIGHT,"Campaign NPC layout QA");return 0;
}
void dialog_close(void) {tower_delete(dialog_model);CloseWindow();}
int dialog_glyphs(const char *text) {
 uint32_t cp;int missing=0;
 while(text&&*text) {utf8_next(&text,&cp);if(cp>127&&!glyph_for(cp))++missing;}
 return missing;
}
unsigned dialog_count(void) {return dialog_draw_count;}
const dialog_draw_t *dialog_draw(unsigned index) {return index<dialog_draw_count?&dialog_draws[index]:NULL;}
int dialog_frame(unsigned floor,unsigned x,unsigned y,unsigned language,
 const char *name_zh,const char *name_en,const char *zh0,const char *zh1,
 const char *en0,const char *en1,uint16_t *out) {
 if(floor>=TOWER_FLOORS||!out)return -1;
 /* Exact preview injection only; production dispatch/paging is not claimed. */
 for(unsigned i=0;i<FLOOR_THEMES;++i) {
  NPC_NAME[i]=(text_t){name_zh,name_en};
  STORY[i]=(story_t){{zh0,zh1},{en0,en1}};
 }
 memset(&dialog_snapshot,0,sizeof(dialog_snapshot));
 tower_read(dialog_model,&dialog_snapshot.tower);
 dialog_snapshot.tower.floor=floor;dialog_snapshot.tower.x=x;dialog_snapshot.tower.y=y;
 dialog_snapshot.screen=GAME_DIALOG;dialog_snapshot.lang=(uint8_t)language;
 dialog_snapshot.volume=60;dialog_snapshot.brightness=GAME_BRIGHTNESS_DEFAULT;
 dialog_snapshot.floor_w=tower_floor_table[floor].w;dialog_snapshot.floor_h=tower_floor_table[floor].h;
 unsigned mx=dialog_snapshot.floor_w>TOWER_VIEW_W?(unsigned)(dialog_snapshot.floor_w-TOWER_VIEW_W):0u;
 unsigned my=dialog_snapshot.floor_h>TOWER_VIEW_H?(unsigned)(dialog_snapshot.floor_h-TOWER_VIEW_H):0u;
 dialog_snapshot.camera_x=(uint16_t)(x>5u?x-5u:0u);if(dialog_snapshot.camera_x>mx)dialog_snapshot.camera_x=(uint16_t)mx;
 dialog_snapshot.camera_y=(uint16_t)(y>5u?y-5u:0u);if(dialog_snapshot.camera_y>my)dialog_snapshot.camera_y=(uint16_t)my;
 dialog_draw_count=0;dialog_trace_active=false;dialog_dirty=true;
 bool ok=game_render((game_handle_t)&dialog_snapshot);
 dialog_trace_active=false;
 if(!ok||dialog_dirty)return -2;
 memcpy(out,dialog_pixels,sizeof(dialog_pixels));return 0;
}
'''


class Draw(C.Structure):
    _fields_ = [('x',C.c_int),('y',C.c_int),('scale',C.c_int),('width',C.c_int),('text',C.c_char*512)]


def cases():
    for npc in NPCS:
        for group in ('pages','extra_pages'):
            for index,page in enumerate(npc.get(group,[])):
                yield npc,f'{group}-{index}',page
        for group in ('carrying','done'):
            if group in npc:
                yield npc,group,npc[group]


def observe_draws(source):
    marker = 'static void ui_draw(const char *text,int x,int y,int scale,Color color)'
    assert source.count(marker) == 1, 'ui_draw observation anchor drift'
    source = source.replace(marker,marker.replace('ui_draw(','dialog_ui_draw_original('),1)
    start = source.index('static void dialog_ui_draw_original(')
    end = source.index('{',start)+1
    depth = 1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}')
        end += 1
    source = source[:end]+TRACE_WRAPPER+source[end:]
    source = source.replace('static const story_t STORY[','static story_t STORY[',1)
    source = source.replace('static const text_t NPC_NAME[','static text_t NPC_NAME[',1)
    assert 'static story_t STORY[' in source and 'static text_t NPC_NAME[' in source, 'authored-page injection anchor drift'
    start = source.index('static void story_modal(')
    begin = source.index('{',start)+1
    end,depth = begin,1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}')
        end += 1
    source = source[:end-1]+'dialog_trace_active=false;\n'+source[end-1:]
    source = source[:begin]+'\ndialog_trace_active=true;\n'+source[begin:]
    insertion = source.index('static const Color BG=')
    return source[:insertion]+TRACE_PREFIX+source[insertion:]+FACADE


def compile_dialog():
    OUT.mkdir(parents=True,exist_ok=True)
    model_lib = build(FLOORS,OUT/'model-build',transform)
    model = OUT/'model-build/model'
    source = transform_view((ROOT/'main/game_view.c').read_text(encoding='utf-8'))
    config = transform_config((ROOT/'main/game_config.h').read_text(encoding='utf-8'))
    assert re.search(r'GAME_WIDTH\s*=\s*480',config) and re.search(r'GAME_HEIGHT\s*=\s*480',config)
    # A hidden clipping rectangle would make pixel bounds falsely pass. Keep
    # parent drawing untouched and independently require full text consumption.
    paragraph = source[source.index('static int campaign_draw_paragraph('):source.index('static void story_modal(')]
    assert 'Scissor' not in paragraph and 'Clip' not in paragraph, 'dialogue must not clip overflowing text'
    fontgen = importlib.import_module('gen_ui_font')
    corpus = source+json.dumps(NPCS,ensure_ascii=False)
    chars = sorted({char for char in corpus if ord(char)>127})
    fontgen.OUT = model/'ui_font.h'
    fontgen.ui_characters = lambda: chars
    log = io.StringIO()
    with contextlib.redirect_stdout(log):
        assert fontgen.main() == 0, 'temporary CJK font generation failed'
    (OUT/'font-generation.log').write_text(log.getvalue(),encoding='utf-8')
    (model/'game_config.h').write_text(config,encoding='utf-8')
    (model/'game.h').write_bytes((ROOT/'main/game.h').read_bytes())
    (model/'dialog_view.c').write_text(observe_draws(source),encoding='utf-8')
    engine = engine_root(os.environ.get('MOSAICO_ENGINE_ROOT'))
    components = ('mosaico_game_2d','mosaico_raylib_fast','mosaico_game_fx','mosaico_game_tilemap')
    sources = [model/'dialog_view.c',model/'tower.c',model/'floors.c',engine/'host/host_asset_runtime.c',
               *[engine/'components'/name/(name+'.c') for name in components]]
    includes = [model,engine/'host/include',engine/'host',
                *[engine/'components'/name/'include' for name in ('mosaico_game_assets',*components)]]
    cc = os.environ.get('CC') or shutil.which('gcc') or r'D:\espbulid\mingw64\bin\gcc.exe'
    library = OUT/('campaign_dialog.dll' if os.name=='nt' else 'campaign_dialog.so')
    command = [cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-shared','-DMOSAICO_HOST_SIMULATION=1']
    if os.name != 'nt': command += ['-fPIC']
    command += list(map(str,sources))
    for include in includes: command += ['-I',str(include)]
    command += ['-lm','-o',str(library)]
    env = os.environ.copy()
    env['PATH'] = str(Path(cc).parent)+os.pathsep+env.get('PATH','')
    env['TMP'] = env['TEMP'] = str(OUT)
    compiled = subprocess.run(command,capture_output=True,text=True,env=env)
    (OUT/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
    assert compiled.returncode == 0, 'Actual dialog renderer compile failed:\n'+compiled.stderr
    lib = C.CDLL(str(library))
    lib._model_lib = model_lib
    lib.dialog_frame.argtypes = [C.c_uint]*4+[C.c_char_p]*6+[C.POINTER(C.c_uint16)]
    lib.dialog_glyphs.argtypes = [C.c_char_p]
    lib.dialog_draw.argtypes = [C.c_uint]
    lib.dialog_draw.restype = C.POINTER(Draw)
    lib.dialog_count.restype = C.c_uint
    lib.dialog_close.restype = None
    return lib,model


def main():
    original_font = (ROOT/'main/ui_font.h').read_bytes()
    lib,model = compile_dialog()
    assert lib.dialog_init() == 0
    buffer = (C.c_uint16*(480*480))()
    results = []
    try:
        for npc,kind,page in cases():
            assert len(page['zh']) == len(page['en']) == 2, (npc['name_zh'],kind)
            texts = [npc['name_zh'],npc['name_en'],*page['zh'],*page['en']]
            assert all(lib.dialog_glyphs(text.encode()) == 0 for text in texts), ('missing CJK glyph',npc['name_zh'],kind)
            for language,lang_name in enumerate(('zh','en')):
                common = [npc['floor']-1,*npc['position'],language]
                assert lib.dialog_frame(*common,*([b'']*6),buffer) == 0
                baseline = rgb565_image(buffer)
                assert lib.dialog_frame(*common,*[text.encode() for text in texts],buffer) == 0
                image = rgb565_image(buffer)
                draws = [lib.dialog_draw(i).contents for i in range(lib.dialog_count())]
                assert 1 <= len(draws) <= 6, ('body line count',npc['name_zh'],kind,lang_name,len(draws))
                body_text = ''.join(draw.text.decode() for draw in draws)
                expected = ''.join(page[lang_name])
                assert ''.join(body_text.split()) == ''.join(expected.split()), ('body truncated or altered',npc['name_zh'],kind,lang_name,body_text,expected)
                for draw in draws:
                    assert draw.scale == 1, ('body font is not normal16px',npc['name_zh'],kind,lang_name,draw.scale)
                    assert draw.x == BODY[0] and draw.y >= BODY[1] and draw.y+16 <= BODY[3] and draw.width <= BODY[2]-BODY[0], ('body draw bounds',npc['name_zh'],kind,lang_name,draw.x,draw.y,draw.width)
                difference = ImageChops.difference(image,baseline)
                assert difference.crop(BODY).getbbox() is not None, ('body produced no actual ink',npc['name_zh'],kind,lang_name)
                assert difference.crop(NAME).getbbox() is not None, ('speaker produced no actual ink',npc['name_zh'],kind,lang_name)
                difference.paste((0,0,0),BODY)
                difference.paste((0,0,0),NAME)
                assert difference.getbbox() is None, ('text ink escaped body/name boxes',npc['name_zh'],kind,lang_name,difference.getbbox())
                if npc.get('easter_egg') and kind == 'pages-0':
                    assert page['zh'] == ['我是这个游戏的真正开发者，','gpt6.1sol。']
                    image.save(OUT/f'gpt-first-page-{lang_name}.png')
                results.append(dict(floor=npc['floor'],position=npc['position'],name=npc['name_zh'],page=kind,language=lang_name,lines=len(draws),max_width=max(draw.width for draw in draws)))
    finally:
        lib.dialog_close()
    assert (ROOT/'main/ui_font.h').read_bytes() == original_font, 'production font changed'
    report = dict(kind='DESIGN PREVIEW - actual story_modal with authored-page injection; production dispatch/paging not integrated',
                  screen=[480,480],body=BODY,name=NAME,cases=results,
                  generated_font_sha256=hashlib.sha256((model/'ui_font.h').read_bytes()).hexdigest(),
                  snapshot_view_sha256=hashlib.sha256((model/'dialog_view.c').read_bytes()).hexdigest())
    (OUT/'layout-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'PASS: {len(results)} authored NPC language/page cases; glyphs, complete text, normal16px body, <=6 lines, draw bounds and actual RGB565 ink bounds')
    print(f'GPT first-page 480x480 screenshots: {OUT}')


if __name__ == '__main__':
    main()
