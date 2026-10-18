"""Exact presentation delta for the authored campaign; applied to lab copies.

NPC dispatch/paging, camera input, retry-menu transitions still require the
separate integration contract. These transforms never mutate main sources.
"""
import json
import re
from campaign_rules import replace_once

ENGLISH_NAMES='Broken Gate|Side Court|Old Archive|Watchtower|Armory|Supply Post|Lookout|Old Camp|Sealed Hall|Captain|Two Banks|Pump Room|Outer Levee|Valve Hall|Sunken Stores|Ferry Court|Old Dormitory|Return Channel|Joined Banks|Ferryman|Loading Yard|Cold Furnace|Warehouse|Fine Casting|Exchange|Ash Return|Ledger Room|Sealed Furnace|Cold Bridge|Furnace Heart|Court Gates|Royal Treasury|Chapel|Two Barracks|Forbidden Books|Long Gallery|Royal Bed|Side Stores|Oath Gate|Oathkeeper|Broken Stairs|Observatory|Mirror Gallery|Star Stores|Crown Hall|Echo Stairs|Return Room|Seal Court|Antechamber|Crown Throne'.split('|')

PARAGRAPH_C=r'''/* Same normal 16px glyphs for every authored page. Pixel-width wrapping
 * keeps mixed Latin/CJK text inside 288px; no font shrinking or clipping. */
static int campaign_draw_paragraph(const char *text,int x,int y,int width,int lines)
{
    int used=0;
    while(text && *text) {
        const char *start=text,*end=text,*last_space=NULL;
        int pixels=0;
        while(*end) {
            const char *next=end; uint32_t cp;
            utf8_next(&next,&cp);
            const int step=cp<0x80?12:17;
            if(pixels+step-1>width || next-start>=190) break;
            if(cp==' ') last_space=end;
            pixels+=step; end=next;
        }
        if(used>=lines || end==start) return -1;
        if(*end && last_space && last_space>start) end=last_space;
        char line[192]; const size_t bytes=(size_t)(end-start);
        memcpy(line,start,bytes);line[bytes]=0;
        ui_draw(line,x,y+used*22,1,WHITE_INK);++used;
        text=end;while(*text==' ')++text;
    }
    return used;
}

'''

def transform_view(source):
    from region1_blueprint import FLOORS as a
    from region2_blueprint import FLOORS as b
    from region3_blueprint import FLOORS as c
    from region4_blueprint import FLOORS as d
    from region5_blueprint import FLOORS as e
    names='static const text_t SCENE_NAME[TOWER_FLOORS]={\n'
    for f,en in zip(a+b+c+d+e,ENGLISH_NAMES):
        names+='    {'+json.dumps(f.name,ensure_ascii=False)+','+json.dumps(en)+'},\n'
    names+='};'
    source,n=re.subn(r'static const text_t SCENE_NAME\[FLOOR_THEMES\]=\{.*?\n\};',lambda _:names,source,flags=re.S)
    assert n==1
    source=replace_once(source,'''    const unsigned slot=floor_slot(s);
    const scene_t *sc=&SCENES[slot];''',
        '''    const unsigned slot=s->tower.floor;
    static const unsigned region_scene[5]={SCENE_GATE,SCENE_CISTERN,SCENE_ARMORY,SCENE_ARCHIVE,SCENE_ASTRAL};
    const scene_t *sc=&SCENES[slot==49u?SCENE_THRONE:region_scene[slot/10u]];''')
    # Finale is deliberately 11x11: existing throne carpet x=5, beacon(5,1)
    # and princess(6,1) already match the authored single-screen composition.
    source=replace_once(source,'            else tile_sprite(tile,sx,sy,0,sc->accent);',
        '''            else tile_sprite(tile,sx,sy,0,sc->accent);
            if(tile==TOWER_TILE_SECRET) {
                DrawRectangle(sx+1,sy+1,30,30,Fade(INK,.20f));
                DrawLine(sx+4,sy+29,sx+27,sy+29,sc->wall_lo);
            }''')
    source=replace_once(source,'static void story_modal(const game_snapshot_t *s)',PARAGRAPH_C+'static void story_modal(const game_snapshot_t *s)')
    start=source.index('    if(s->lang==GAME_LANG_EN){',source.index('static void story_modal('))
    end=source.index('    button(s,&T_TALK_CONTINUE',start)
    source=source[:start]+'''    const char *const *lines=s->lang==GAME_LANG_EN?story->en:story->zh;
    const int first=campaign_draw_paragraph(lines[0],150,174,288,6);
    if(first>=0) (void)campaign_draw_paragraph(lines[1],150,174+first*22,288,6-first);
'''+source[end:]
    # Old adaptive helpers become unused. Retain them for other views only if
    # referenced; remove their complete definitions from this exact snapshot.
    for name in ('ui_draw_cjk16','cjk_line_count','closing_punctuation','ui_draw_wrap_cjk'):
        # These form a contiguous group ending immediately before button().
        # Prune below using balanced braces rather than a greedy regex.
        pattern=re.compile(r'static (?:void|int|bool) '+name+r'\([^;]*?\)\s*\{',re.S)
        match=pattern.search(source)
        if match and len(re.findall(r'\b'+name+r'\s*\(',source))==1:
            pos=match.end();depth=1
            while depth:
                depth+=(source[pos]=='{')-(source[pos]=='}');pos+=1
            source=source[:match.start()]+source[pos:]
    # Remove leftover adaptive helpers after dependants were removed.
    for name in ('ui_draw_cjk16','cjk_line_count','closing_punctuation'):
        pattern=re.compile(r'static (?:void|int|bool) '+name+r'\([^;]*?\)\s*\{',re.S)
        match=pattern.search(source)
        if match and len(re.findall(r'\b'+name+r'\s*\(',source))==1:
            pos=match.end();depth=1
            while depth:
                depth+=(source[pos]=='{')-(source[pos]=='}');pos+=1
            source=source[:match.start()]+source[pos:]
    return source

def transform_config(source):
    # The user replaced camera buttons with edge paging. Production geometry
    # already expresses that decision; there are no camera touch rectangles.
    return source
