// SPDX-License-Identifier: MIT
// Original geometric pixel art and a generated 16x16 CJK bitmap subset.
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
#include "campaign_dialogue.h"
#include "mosaico_raylib_fast.h"
#include "ui_font.h"

static const Color BG={14,19,28,255}, INK={9,13,20,255}, PANEL={25,32,44,255};
static const Color STONE={52,62,77,255}, EDGE={77,87,99,255};
static const Color BRASS={226,188,112,255}, WHITE_INK={234,230,215,255};
static const Color MUTED={139,158,167,255}, MINT={127,216,180,255};
static const Color RED_INK={225,110,111,255};
static const Color KEY_BLUE={107,179,232,255};
static const Color ROBE={156,126,196,255}, GOLEM_STONE={154,148,136,255};

enum {
    SCENE_GATE, SCENE_ARMORY, SCENE_ARCHIVE, SCENE_GARDEN, SCENE_CISTERN,
    SCENE_BARRACKS, SCENE_CRYPT, SCENE_SAND, SCENE_ASTRAL, SCENE_THRONE
};
typedef struct {
    Color bg, floor_a, floor_b, wall, wall_hi, wall_lo, accent, carpet;
    unsigned char style;
} scene_t;
/* Each floor gets its own quiet palette; the four-quadrant plans keep the
 * scene readable behind bright pickups and enemies. */
/* Both floor sizes and the scene roster grow by region of ten floors, so the
 * ten authored themes and stories repeat every ten floors (floor 50 is the
 * tenth theme, the throne). */
enum { FLOOR_THEMES = 10 };
static const scene_t SCENES[FLOOR_THEMES]={
 {{12,17,24,255},{29,38,50,255},{25,33,44,255},{58,68,84,255},{88,99,114,255},{39,47,59,255},{226,188,112,255},{60,40,34,255},SCENE_GATE},
 {{20,16,13,255},{42,35,27,255},{37,31,24,255},{76,61,45,255},{112,90,64,255},{49,39,30,255},{218,166,102,255},{54,40,27,255},SCENE_ARMORY},
 {{16,13,24,255},{38,30,51,255},{33,26,45,255},{62,50,80,255},{94,78,118,255},{42,33,56,255},{177,147,224,255},{44,30,50,255},SCENE_ARCHIVE},
 {{8,20,17,255},{31,50,38,255},{27,44,34,255},{54,72,59,255},{84,106,85,255},{35,49,40,255},{140,220,170,255},{40,58,38,255},SCENE_GARDEN},
 {{7,17,23,255},{25,45,51,255},{22,39,45,255},{45,72,80,255},{72,106,112,255},{30,48,54,255},{107,190,232,255},{26,46,48,255},SCENE_CISTERN},
 {{22,14,14,255},{47,32,29,255},{41,28,26,255},{80,52,46,255},{118,78,66,255},{51,33,31,255},{230,120,110,255},{70,35,30,255},SCENE_BARRACKS},
 {{17,12,22,255},{39,29,45,255},{34,25,40,255},{62,48,68,255},{94,74,98,255},{41,32,47,255},{198,154,216,255},{52,32,50,255},SCENE_CRYPT},
 {{27,22,13,255},{64,51,33,255},{57,46,30,255},{98,78,48,255},{138,110,68,255},{64,51,34,255},{232,194,116,255},{72,50,32,255},SCENE_SAND},
 {{8,11,25,255},{26,30,56,255},{22,26,49,255},{44,50,86,255},{73,83,128,255},{29,34,60,255},{132,222,224,255},{34,36,66,255},SCENE_ASTRAL},
 {{18,9,12,255},{47,27,29,255},{40,23,25,255},{68,39,39,255},{106,60,52,255},{44,26,28,255},{232,192,112,255},{92,28,34,255},SCENE_THRONE},
};

typedef struct { const char *zh, *en; } text_t;
static const text_t T_TITLE={"魔塔","MAGIC TOWER"};
static const text_t T_SUBTITLE={"五十层试炼","FIFTY FLOORS"};
static const text_t T_NEW={"新的旅程","NEW JOURNEY"};
static const text_t T_CONTINUE={"继续旅程","CONTINUE"};
static const text_t T_NO_SAVE={"暂无存档","NO SAVE YET"};
static const text_t T_HINT1={"点击相邻格子或滑动移动","Tap a neighbour or swipe to step"};
static const text_t T_HINT2={"查看伤害，选择战斗","Check the cost. Choose your battles"};
static const text_t T_FLOOR={"第 %u 层","FLOOR %02u"};
static const text_t T_SUMMIT={"塔顶","SUMMIT"};
static const text_t T_KEY={"钥匙","KEY"};
static const text_t T_GOLD={"金币","GOLD"};
static const text_t T_NEW_GAME={"新游戏","NEW"};
static const text_t T_DAMAGE={"伤害","DAMAGE"};
static const text_t T_VOLUME={"音量","VOLUME"};
static const text_t T_SETTINGS={"设置","SETTINGS"};
static const text_t T_LANGUAGE={"语言","LANGUAGE"};
static const text_t T_BRIGHTNESS={"亮度","BRIGHTNESS"};
static const text_t T_CHINESE={"中文","中文"};
static const text_t T_ENGLISH={"ENGLISH","ENGLISH"};
static const text_t T_REPOSITORY={"GitHub 仓库","GitHub REPOSITORY"};
static const text_t T_PREFERENCES_FAILED={"设置保存失败，请重试","Settings not saved. Try again"};
static const text_t T_BACK={"返回","BACK"};
static const text_t T_FIGHT={"战斗","FIGHT"};
static const text_t T_RESTART={"重新开始？","RESTART?"};
static const text_t T_UNSAVED={"未保存的进度将丢失","All progress is lost"};
static const text_t T_PAUSED={"暂停","PAUSED"};
static const text_t T_SUMMIT_REACHED={"魔王陨落，公主获救","The king falls. You win."};
static const text_t T_SHOP={"商店","SHOP"};
static const text_t T_SHOP_ATK={"攻击 +3","ATK +3"};
static const text_t T_SHOP_DEF={"防御 +3","DEF +3"};
static const text_t T_LEAVE={"离开","LEAVE"};
static const text_t T_BOUGHT={"已购买","BOUGHT"};
static const text_t T_NO_GOLD={"金币不足","NOT ENOUGH GOLD"};
static const text_t T_TALK_CONTINUE={"继续","CONTINUE"};
static const text_t T_TELEPORT={"传送","WARP"};
static const text_t T_TELEPORT_TITLE={"传送 · 章节锚点","WARP · CHAPTER ANCHORS"};
static const text_t T_ANCHOR_LOCKED={"未解锁","LOCKED"};
static const text_t T_ANCHOR_HERE={"当前","HERE"};
static const text_t T_ANCHOR_HINT={"通关一章，点亮下一个传送点","Clear chapters. Unlock anchors"};
static const text_t T_RESTART_GAME={"重开","RESTART"};
static const text_t T_RESTART_HINT={"本层未保存的操作将丢失","Unsaved steps are lost"};
static const text_t T_RETRY_FAILED={"读取失败，当前旅程未变","Load failed. No changes"};
static const text_t T_CANCEL={"取消","CANCEL"};
static const text_t T_LOAD_CHECKPOINT={"从检查点重试","RETRY CHECKPOINT"};
static const text_t T_DEAD={"你战败了","DEFEATED"};
static const text_t T_VICTORY={"通关","VICTORY"};
static const text_t T_EXPLORE={"继续探索","EXPLORE"};
static const text_t T_TURNS={"移动","MOVES"};

static const text_t SCENE_NAME[TOWER_FLOORS]={
    {"破门","Broken Gate"},
    {"偏院","Side Court"},
    {"旧库","Old Archive"},
    {"哨楼","Watchtower"},
    {"军械库","Armory"},
    {"军需站","Supply Post"},
    {"守望台","Lookout"},
    {"旧营房","Old Camp"},
    {"封门前厅","Sealed Hall"},
    {"队长之门","Captain"},
    {"双岸","Two Banks"},
    {"泵房","Pump Room"},
    {"外堤","Outer Levee"},
    {"阀厅","Valve Hall"},
    {"沉库","Sunken Stores"},
    {"渡守庭","Ferry Court"},
    {"旧宿舍","Old Dormitory"},
    {"回流渠","Return Channel"},
    {"双岸合流","Joined Banks"},
    {"渡守","Ferryman"},
    {"运料场","Loading Yard"},
    {"废炉","Cold Furnace"},
    {"分货库","Warehouse"},
    {"精铸室","Fine Casting"},
    {"交易所","Exchange"},
    {"灰烬回道","Ash Return"},
    {"账房","Ledger Room"},
    {"封存炉","Sealed Furnace"},
    {"熄火桥","Cold Bridge"},
    {"炉心","Furnace Heart"},
    {"王城前庭","Court Gates"},
    {"王室税库","Royal Treasury"},
    {"礼拜堂","Chapel"},
    {"双翼兵营","Two Barracks"},
    {"禁书室","Forbidden Books"},
    {"长廊","Long Gallery"},
    {"旧王寝","Royal Bed"},
    {"王城侧库","Side Stores"},
    {"守誓门","Oath Gate"},
    {"守誓者","Oathkeeper"},
    {"裂阶","Broken Stairs"},
    {"旧观测室","Observatory"},
    {"镜廊","Mirror Gallery"},
    {"星钥仓","Star Stores"},
    {"冠冕厅","Crown Hall"},
    {"回响阶","Echo Stairs"},
    {"归还之室","Return Room"},
    {"封印庭","Seal Court"},
    {"王座前室","Antechamber"},
    {"星冠王座","Crown Throne"},
};
/* The five regions named by docs/TOWER50_DESIGN_SPINE.md; one per pair of
 * chapter anchors (10, 20, 30, 40, 50). */
static const text_t REGION_NAME[5]={
    {"断门城塞","BROKEN GATE"}, {"沉水回廊","SUNKEN HALL"},
    {"灰烬工坊","ASH WORKS"}, {"静默王城","STILL COURT"},
    {"星冠高塔","STAR CROWN"},
};

static const char *tx(const game_snapshot_t *s, const text_t *t)
{ return s->lang==GAME_LANG_EN?t->en:t->zh; }

static const text_t STATUS_TEXT[]={
    {"向上攀登","Climb the tower"},
    {"选择下一步","Choose your next step"},
    {"石块挡住了去路","Stone blocks the way"},
    {"需要对应颜色的钥匙","A matching key is needed"},
    {"获得补给","Supplies collected"},
    {"门开了","Door opened"},
    {"战斗？确认或取消","Fight? Confirm or cancel"},
    {"太强了：先收集宝石","Too strong: gather gems first"},
    {"敌人已击败","Enemy defeated"},
    {"已取消战斗","Fight cancelled"},
    {"楼层已切换","Floor changed"},
    {"塔顶信标已点亮！","Beacon lit! Tower cleared"},
    {"商店：购买或离开","Merchant: buy or leave"},
    {"金币不足","Not enough gold"},
    {"有人想和你说话","Someone wants to talk"},
    {"你战败了","You have fallen"},
    {"密室开了","A secret chamber opened"},
    {"有变动","Something changed"},
    {"传送点尚未解锁","This anchor is still locked"},
    {"本章已通，传送点已解锁","Chapter cleared: anchor lit"}
};
static const text_t NOTICE_TEXT[]={
    {"",""},
    {"向上攀登","Climb the tower"},
    {"旅程已保存","Journey saved."},
    {"暂无存档","No saved journey."},
    {"存档已损坏，旅程未变","Save damaged. Journey unchanged."},
    {"旅程已恢复","Journey restored."},
    {"保存失败，请重试","Save failed. Try again."},
    {"塔顶到达！可返回或探索","SUMMIT REACHED! Return or explore."},
    {"本章已通，传送点已解锁","Chapter cleared - anchor unlocked."}
};

static const ui_glyph_t *glyph_for(uint32_t cp)
{
    /* gen_ui_font.py writes glyphs sorted by Unicode codepoint.
     * Binary search avoids a linear scan of the CJK table for each label. */
    size_t lo=0, hi=UI_GLYPH_COUNT;
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        if(ui_glyphs[mid].codepoint<cp) lo=mid+1;
        else hi=mid;
    }
    return lo<UI_GLYPH_COUNT && ui_glyphs[lo].codepoint==cp
        ? &ui_glyphs[lo] : NULL;
}
static int utf8_next(const char **p,uint32_t *cp)
{
    const unsigned char *s=(const unsigned char *)*p;
    if(s[0]<0x80){*cp=s[0];*p+=1;return 1;}
    if((s[0]&0xE0)==0xC0&&s[1]){*cp=(uint32_t)((s[0]&0x1F)<<6)|(s[1]&0x3F);*p+=2;return 2;}
    if((s[0]&0xF0)==0xE0&&s[1]&&s[2]){*cp=(uint32_t)((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F);*p+=3;return 3;}
    *cp=s[0];*p+=1;return 1;
}
static int ui_width(const char *text,int scale)
{
    int width=0; uint32_t cp;
    if(!text) return 0;
    while(*text){utf8_next(&text,&cp);width+=(cp<0x80?12:17)*scale;}
    return width?width-1:0;
}
static void ui_draw(const char *text,int x,int y,int scale,Color color)
{
    const char *p=text; uint32_t cp;
    if(!text) return;
    while(*p){
        utf8_next(&p,&cp);
        if(cp<0x80){
            if(cp=='+'){
                MosaicoFastDrawRectangle(x+2*scale,y+8*scale,8*scale,2*scale,color);
                MosaicoFastDrawRectangle(x+5*scale,y+5*scale,2*scale,8*scale,color);
            } else {
                char one[2]={(char)cp,0};
                MosaicoFastDrawText(one,x,y+2*scale,16*scale,color);
            }
            x+=12*scale;
            continue;
        }
        const ui_glyph_t *g=glyph_for(cp);
        if(g){
            for(int row=0;row<16;++row){
                const uint8_t hi=g->bitmap[row*2],lo=g->bitmap[row*2+1];
                int start=-1;
                for(int col=0;col<=16;++col){
                    const bool on=col<16&&(col<8?(hi&(1U<<(7-col))):(lo&(1U<<(15-col))));
                    if(on&&start<0) start=col;
                    if(!on&&start>=0){
                        MosaicoFastDrawRectangle(x+start*scale,y+row*scale,(col-start)*scale,scale,color);
                        start=-1;
                    }
                }
            }
        }
        x+=17*scale;
    }
}
static void ui_center(const char *text,int y,int scale,Color color)
{ ui_draw(text,(GAME_WIDTH-ui_width(text,scale))/2,y,scale,color); }
static void ui_center_fit(const char *text,int y,int scale,Color color,int max_width)
{
    while(scale>1&&ui_width(text,scale)>max_width) --scale;
    ui_draw(text,(GAME_WIDTH-ui_width(text,scale))/2,y,scale,color);
}
static void ui_draw_cjk15(const char *text,int x,int y,Color color)
{
    const char *p=text; uint32_t cp;
    if(!text) return;
    while(*p){
        utf8_next(&p,&cp);
        if(cp<0x80){
            char one[2]={(char)cp,0};
            MosaicoFastDrawText(one,x,y+4,20,color);
            x+=18;
            continue;
        }
        const ui_glyph_t *g=glyph_for(cp);
        if(g){
            for(int dy=0;dy<24;++dy){
                const int sy=dy*2/3;
                const uint8_t hi=g->bitmap[sy*2],lo=g->bitmap[sy*2+1];
                int start=-1;
                for(int dx=0;dx<=24;++dx){
                    bool on=false;
                    if(dx<24){
                        const int sx=dx*2/3;
                        on=sx<8?(hi&(1U<<(7-sx))):(lo&(1U<<(15-sx)));
                    }
                    if(on&&start<0) start=dx;
                    if(!on&&start>=0){
                        MosaicoFastDrawRectangle(x+start,y+dy,dx-start,1,color);
                        start=-1;
                    }
                }
            }
        }
        x+=25;
    }
}




static void button(const game_snapshot_t *s,const text_t *label,int x,int y,int w,int h,bool active)
{
    const char *text=label?tx(s,label):"";
    DrawRectangle(x,y,w,h,active?BRASS:PANEL);
    DrawRectangleLines(x,y,w,h,active?BRASS:EDGE);
    if(text[0]) ui_draw(text,x+(w-ui_width(text,1))/2,y+(h-16)/2,1,active?INK:WHITE_INK);
}
static int footer_button_x(int index)
{ return GAME_FOOTER_X + index*(GAME_BUTTON_W+GAME_BUTTON_GAP); }
static int camera_max_x(const game_snapshot_t *s)
{ return s->floor_w>TOWER_VIEW_W?(int)s->floor_w-TOWER_VIEW_W:0; }
static int camera_max_y(const game_snapshot_t *s)
{ return s->floor_h>TOWER_VIEW_H?(int)s->floor_h-TOWER_VIEW_H:0; }
/* A hint is paint only: no hit rectangles, held controls or idle work. */
static void camera_edges(const game_snapshot_t *s,Color background)
{
    const int extent=GAME_TILE*TOWER_VIEW_W;
    for(int band=0;band<2;++band) {
        const int inset=band*GAME_CAMERA_EDGE_HINT/2;
        const int width=GAME_CAMERA_EDGE_HINT/2;
        const Color tint=Fade(background,band==0?.62f:.28f);
        if(s->camera_x>0)
            DrawRectangle(GAME_MAP_X+inset,GAME_MAP_Y,width,extent,tint);
        if(s->camera_x<(unsigned)camera_max_x(s))
            DrawRectangle(GAME_MAP_X+extent-inset-width,GAME_MAP_Y,width,extent,tint);
        if(s->camera_y>0)
            DrawRectangle(GAME_MAP_X,GAME_MAP_Y+inset,extent,width,tint);
        if(s->camera_y<(unsigned)camera_max_y(s))
            DrawRectangle(GAME_MAP_X,GAME_MAP_Y+extent-inset-width,extent,width,tint);
    }
}
static void ascii_center(const char *text,int y,int size,Color color)
{ MosaicoFastDrawText(text,(GAME_WIDTH-MosaicoFastMeasureText(text,size))/2,y,size,color); }
static void combat_stat(const char *label,unsigned long value,int cx)
{
    char text[24];
    snprintf(text,sizeof(text),"%s %lu",label,value);
    MosaicoFastDrawText(text,cx-MosaicoFastMeasureText(text,16)/2,207,16,WHITE_INK);
}

static void key_sprite(int x,int y,Color color)
{
    DrawRectangle(x+5,y+8,8,8,color); DrawRectangle(x+7,y+10,4,4,INK);
    DrawRectangle(x+12,y+11,10,3,color); DrawRectangle(x+17,y+12,3,6,color);
    DrawRectangle(x+21,y+12,2,4,color);
}
static void knight(int x,int y,bool hero)
{
    const Color armor=hero?WHITE_INK:(Color){142,163,184,255};
    DrawRectangle(x+5,y+10,15,13,hero?RED_INK:(Color){79,91,116,255});
    DrawRectangle(x+8,y+4,11,10,armor); DrawRectangle(x+6,y+7,15,4,armor);
    DrawRectangle(x+9,y+9,10,3,INK); DrawPixel(x+16,y+10,BRASS);
    DrawRectangle(x+9,y+15,9,6,armor);
    DrawRectangle(x+7,y+21,5,3,STONE); DrawRectangle(x+16,y+21,5,3,STONE);
    DrawRectangle(x+20,y+5,2,14,BRASS); DrawRectangle(x+18,y+17,6,2,BRASS);
    DrawRectangle(x+3,y+13,5,8,MINT);
}
static void mage(int x,int y,uint32_t tick)
{
    const int bob=(tick/20)%2;
    DrawRectangle(x+6,y+9+bob,14,14,ROBE);
    DrawRectangle(x+9,y+5+bob,8,6,(Color){214,196,236,255});
    DrawRectangle(x+10,y+8+bob,2,2,INK); DrawRectangle(x+15,y+8+bob,2,2,INK);
    DrawRectangle(x+5,y+22,16,2,INK);
    DrawRectangle(x+21,y+6+bob,2,14,BRASS);
    DrawRectangle(x+20,y+3+bob,4,4,MINT);
    DrawPixel(x+22,y+1+bob,MINT);
}
static void golem(int x,int y)
{
    DrawRectangle(x+4,y+6,18,17,GOLEM_STONE);
    DrawRectangle(x+2,y+11,4,9,(Color){120,114,104,255});
    DrawRectangle(x+20,y+11,4,9,(Color){120,114,104,255});
    DrawRectangle(x+7,y+9,14,5,(Color){178,172,160,255});
    DrawRectangle(x+8,y+13,3,3,RED_INK); DrawRectangle(x+17,y+13,3,3,RED_INK);
    DrawRectangle(x+9,y+19,8,2,INK);
    DrawRectangle(x+6,y+23,14,2,INK);
    DrawLine(x+10,y+6,x+14,y+1,EDGE); DrawLine(x+14,y+1,x+18,y+6,EDGE);
}
static void npc_sprite(int x,int y,uint32_t tick)
{
    const Color hood={116,108,136,255}, glow={172,214,224,255};
    DrawRectangle(x+7,y+7,12,16,hood);
    DrawRectangle(x+5,y+20,16,4,hood);
    DrawRectangle(x+9,y+3,8,6,(Color){146,138,166,255});
    DrawRectangle(x+9,y+9,8,6,INK);
    DrawPixel(x+10,y+10+(tick/18)%2,glow); DrawPixel(x+15,y+10+(tick/18)%2,glow);
    DrawRectangle(x+4,y+23,18,2,(Color){74,70,90,255});
    DrawRectangle(x+20,y+11,2,9,BRASS);
}
static void npc_portrait(int x,int y,uint32_t tick)
{
    const Color hood={72,66,92,255}, trim={112,104,136,255};
    const Color face={200,186,196,255}, dark={30,26,40,255};
    const Color glow={172,214,224,255};
    DrawCircle(x+48,y+54,34,Fade(glow,.08f));
    DrawTriangle((Vector2){x+18,y+140},(Vector2){x+78,y+140},(Vector2){x+48,y+48},trim);
    DrawRectangle(x+18,y+98,60,42,trim);
    DrawCircle(x+48,y+54,30,hood);
    DrawCircle(x+48,y+58,23,dark);
    DrawCircle(x+48,y+60,14,face);
    DrawRectangle(x+40,y+56+(tick/18)%2,4,5,glow);
    DrawRectangle(x+53,y+56+(tick/18)%2,4,5,glow);
    DrawRectangle(x+44,y+69,8,2,INK);
    DrawTriangle((Vector2){x+18,y+54},(Vector2){x+28,y+96},(Vector2){x+40,y+54},hood);
    DrawTriangle((Vector2){x+78,y+54},(Vector2){x+68,y+96},(Vector2){x+56,y+54},hood);
    DrawRectangle(x+78,y+34,3,104,BRASS);
    DrawCircle(x+79,y+32,5,glow);
    DrawRectangle(x+14,y+138,68,4,INK);
}
static void wraith(int x,int y,uint32_t tick)
{
    const int bob=(tick/14)%2;
    const Color body={114,150,176,255}, wisp=Fade((Color){170,226,232,255},.55f);
    DrawTriangle((Vector2){x+4,y+3+bob},(Vector2){x+11,y+8},(Vector2){x+4,y+14+bob},wisp);
    DrawTriangle((Vector2){x+22,y+3+bob},(Vector2){x+15,y+8},(Vector2){x+22,y+14+bob},wisp);
    DrawRectangle(x+8,y+6+bob,10,13,body);
    DrawRectangle(x+6,y+17+bob,14,4,Fade(body,.85f));
    for(int i=0;i<4;++i) DrawRectangle(x+6+i*4,y+21+bob,3,3+i%2,wisp);
    DrawPixel(x+10,y+10+bob,RED_INK); DrawPixel(x+15,y+10+bob,RED_INK);
    DrawRectangle(x+10,y+15+bob,6,2,INK);
}
static void demon(int x,int y,uint32_t tick)
{
    const int bob=(tick/20)%2;
    const Color skin={112,62,76,255}, armor={58,50,70,255};
    DrawTriangle((Vector2){x+5,y+8+bob},(Vector2){x+2,y+1+bob},(Vector2){x+10,y+6+bob},BRASS);
    DrawTriangle((Vector2){x+21,y+8+bob},(Vector2){x+24,y+1+bob},(Vector2){x+16,y+6+bob},BRASS);
    DrawRectangle(x+6,y+6+bob,14,11,skin);
    DrawRectangle(x+8,y+9+bob,3,3,RED_INK); DrawRectangle(x+15,y+9+bob,3,3,RED_INK);
    DrawRectangle(x+9,y+14+bob,8,2,INK);
    DrawRectangle(x+4,y+17+bob,18,7,armor);
    DrawRectangle(x+8,y+19+bob,10,2,RED_INK);
    DrawRectangle(x+2,y+19+bob,4,7,skin); DrawRectangle(x+20,y+19+bob,4,7,skin);
    DrawRectangle(x+7,y+24+bob,5,2,INK); DrawRectangle(x+14,y+24+bob,5,2,INK);
}
static void princess(int x,int y,uint32_t tick)
{
    const int bob=(tick/24)%2;
    DrawRectangle(x+9,y+4+bob,9,7,(Color){242,214,170,255});
    DrawRectangle(x+8,y+2+bob,11,3,BRASS);
    DrawRectangle(x+10,y+7+bob,2,2,INK); DrawRectangle(x+15,y+7+bob,2,2,INK);
    DrawTriangle((Vector2){x+6,y+12+bob},(Vector2){x+21,y+12+bob},(Vector2){x+13,y+24+bob},(Color){232,170,196,255});
    DrawRectangle(x+5,y+12+bob,5,3,(Color){232,170,196,255});
    DrawRectangle(x+18,y+12+bob,5,3,(Color){232,170,196,255});
    DrawPixel(x+22,y+2+bob,BRASS);
}
static void beacon(int x,int y,uint32_t tick)
{
    const int flick=(tick/5)%3;
    DrawRectangle(x+3,y+1,26,27,Fade(BRASS,.12f));
    DrawRectangle(x+7,y+18,18,7,STONE);
    DrawRectangle(x+10,y+10,12,10,BRASS);
    DrawRectangle(x+11,y+4-flick,10,8,RED_INK);
    DrawRectangle(x+13,y+1-flick,6,5,(Color){255,196,120,255});
    DrawPixel(x+15,y-flick,(Color){255,238,180,255});
    DrawPixel(x+10,y+7-flick,(Color){255,170,90,255});
    DrawPixel(x+21,y+7-flick,(Color){255,170,90,255});
    DrawRectangle(x+12,y+13,8,2,INK);
}
static void shop_sprite(int x,int y)
{
    DrawRectangle(x+3,y+12,20,8,PANEL);
    DrawRectangle(x+3,y+12,20,2,BRASS);
    DrawRectangle(x+10,y+4,7,8,(Color){214,196,236,255});
    DrawRectangle(x+11,y+6,2,2,INK); DrawRectangle(x+15,y+6,2,2,INK);
    DrawRectangle(x+8,y+12,11,5,ROBE);
    DrawRectangle(x+12,y+14,4,3,BRASS);
    DrawRectangle(x+4,y+20,3,3,STONE); DrawRectangle(x+20,y+20,3,3,STONE);
}
static void tile_sprite(tower_tile_t tile,int x,int y,uint32_t tick,Color accent)
{
    if(tile==TOWER_TILE_WALL) {
        DrawRectangle(x+1,y+1,GAME_TILE-2,GAME_TILE-2,STONE);
        DrawLine(x+2,y+2,x+GAME_TILE-3,y+2,EDGE);
        DrawLine(x+1,y+16,x+GAME_TILE-2,y+16,INK);
        DrawLine(x+16,y+3,x+16,y+15,INK);
        DrawLine(x+9,y+17,x+9,y+GAME_TILE-2,INK);
        return;
    }
    x+=(GAME_TILE-26)/2;
    y+=(GAME_TILE-26)/2;
    if(tile>=TOWER_TILE_KEY_YELLOW && tile<=TOWER_TILE_KEY_RED) {
        key_sprite(x,y,tile==TOWER_TILE_KEY_YELLOW?BRASS:tile==TOWER_TILE_KEY_BLUE?(Color){107,179,232,255}:RED_INK);
    } else if(tile>=TOWER_TILE_DOOR_YELLOW && tile<=TOWER_TILE_DOOR_RED) {
        Color c=tile==TOWER_TILE_DOOR_YELLOW?BRASS:tile==TOWER_TILE_DOOR_BLUE?(Color){107,179,232,255}:RED_INK;
        DrawRectangle(x+4,y+3,18,22,c); DrawRectangle(x+7,y+6,12,19,(Color){62,48,38,255});
        DrawRectangle(x+12,y+6,2,19,c); DrawRectangle(x+15,y+14,3,3,c);
    } else if(tile==TOWER_TILE_POTION || tile==TOWER_TILE_BIG_POTION) {
        DrawRectangle(x+10,y+3,7,3,BRASS); DrawRectangle(x+11,y+6,5,4,WHITE_INK);
        DrawRectangle(x+7,y+10,13,12,WHITE_INK); DrawRectangle(x+9,y+13,9,7,RED_INK);
        DrawRectangle(x+9,y+11,3,3,WHITE);
    } else if(tile==TOWER_TILE_ATTACK || tile==TOWER_TILE_DEFENCE) {
        Color c=tile==TOWER_TILE_ATTACK?RED_INK:(Color){107,179,232,255};
        DrawTriangle((Vector2){x+13,y+4},(Vector2){x+4,y+12},(Vector2){x+13,y+23},c);
        DrawTriangle((Vector2){x+13,y+4},(Vector2){x+13,y+23},(Vector2){x+22,y+12},Fade(c,.65f));
        DrawLine(x+13,y+5,x+7,y+12,WHITE_INK);
    } else if(tile==TOWER_TILE_SLIME) {
        const int bob=(tick/16)%2;
        DrawRectangle(x+5,y+12-bob,17,10+bob,(Color){89,159,124,255});
        DrawRectangle(x+8,y+9-bob,11,13+bob,MINT);
        DrawRectangle(x+9,y+13,3,3,INK); DrawRectangle(x+16,y+13,3,3,INK);
        DrawRectangle(x+5,y+22,18,2,INK);
    } else if(tile==TOWER_TILE_BAT) {
        Color c={177,137,204,255}; int flap=(tick/8)%2?2:0;
        DrawTriangle((Vector2){x+2,y+7+flap},(Vector2){x+12,y+12},(Vector2){x+5,y+20},c);
        DrawTriangle((Vector2){x+24,y+7+flap},(Vector2){x+14,y+12},(Vector2){x+21,y+20},c);
        DrawRectangle(x+10,y+9,7,13,c); DrawPixel(x+11,y+12,RED_INK); DrawPixel(x+15,y+12,RED_INK);
    } else if(tile==TOWER_TILE_KNIGHT) knight(x,y,false);
    else if(tile==TOWER_TILE_MAGE) mage(x,y,tick);
    else if(tile==TOWER_TILE_GOLEM) golem(x,y);
    else if(tile==TOWER_TILE_WRAITH) wraith(x,y,tick);
    else if(tile==TOWER_TILE_DEMON) demon(x,y,tick);
    else if(tile==TOWER_TILE_NPC) npc_sprite(x,y,tick);
    else if(tile==TOWER_TILE_SENTINEL) {
        DrawRectangle(x+4,y+5,18,19,(Color){116,72,78,255});
        DrawRectangle(x+2,y+8,22,7,(Color){161,91,91,255});
        DrawRectangle(x+6,y+10,5,3,RED_INK); DrawRectangle(x+16,y+10,5,3,RED_INK);
        DrawRectangle(x+9,y+18,9,3,INK);
    } else if(tile==TOWER_TILE_SHOP) shop_sprite(x,y);
    else if(tile==TOWER_TILE_UP || tile==TOWER_TILE_DOWN) {
        for(int i=0;i<4;++i) {
            int row=tile==TOWER_TILE_UP?i:3-i;
            DrawRectangle(x+4+i*3,y+5+row*4,19-i*3,3,accent);
        }
    } else if(tile==TOWER_TILE_GOAL) {
        DrawRectangle(x+8,y+11,11,11,BRASS); DrawRectangle(x+5,y+6,4,8,BRASS);
        DrawRectangle(x+12,y+4,4,12,BRASS); DrawRectangle(x+19,y+6,4,8,BRASS);
        DrawRectangle(x+9,y+14,10,3,RED_INK);
    }
}

static void scene_floor(const scene_t *sc,int sx,int sy,int x,int y)
{
    Color base=((x+y)&1)?sc->floor_a:sc->floor_b;
    if(sc->style==SCENE_THRONE && x==5) base=sc->carpet;
    DrawRectangle(sx,sy,GAME_TILE,GAME_TILE,base);
    switch(sc->style) {
    case SCENE_GATE:
        DrawLine(sx+2,sy+16,sx+15,sy+16,sc->wall_lo);
        DrawLine(sx+17,sy+16,sx+29,sy+16,sc->wall_lo);
        break;
    case SCENE_ARMORY:
        DrawLine(sx+3,sy+24,sx+28,sy+24,sc->wall_lo);
        DrawRectangle(sx+6,sy+7,2,2,sc->wall_hi);
        DrawRectangle(sx+24,sy+7,2,2,sc->wall_hi);
        break;
    case SCENE_ARCHIVE:
        DrawLine(sx+4,sy+3,sx+4,sy+14,sc->wall_lo);
        DrawLine(sx+27,sy+18,sx+27,sy+29,sc->wall_lo);
        if(((x*3+y)&7)==0) DrawRectangle(sx+14,sy+25,4,2,sc->accent);
        break;
    case SCENE_GARDEN:
        DrawRectangle(sx+4+((x*3+y*5)%12),sy+23,2,3,sc->accent);
        DrawRectangle(sx+20-((x+y*3)%10),sy+26,3,2,sc->wall_hi);
        break;
    case SCENE_CISTERN:
        DrawLine(sx+3,sy+24,sx+12+((x+y)%8),sy+24,sc->wall_hi);
        DrawPixel(sx+22,sy+22,sc->accent);
        break;
    case SCENE_BARRACKS:
        DrawLine(sx+2,sy+12,sx+29,sy+12,sc->wall_lo);
        DrawRectangle(sx+23,sy+22,4,3,sc->accent);
        break;
    case SCENE_CRYPT:
        DrawLine(sx+5,sy+25,sx+10,sy+20,sc->wall_lo);
        DrawLine(sx+19,sy+5,sx+25,sy+11,sc->wall_lo);
        break;
    case SCENE_SAND:
        DrawLine(sx+3,sy+22,sx+14,sy+22,sc->wall_hi);
        DrawLine(sx+16,sy+26,sx+29,sy+26,sc->wall_hi);
        DrawPixel(sx+8,sy+7,sc->accent);
        break;
    case SCENE_ASTRAL:
        DrawPixel(sx+4+((x*7+y*3)%24),sy+4+((x*5+y*11)%24),sc->accent);
        DrawPixel(sx+4+((x*11+y*7)%24),sy+4+((x*3+y*5)%24),sc->wall_hi);
        break;
    case SCENE_THRONE:
        if(x==5) {
            DrawLine(sx+2,sy,sx+2,sy+31,sc->accent);
            DrawLine(sx+29,sy,sx+29,sy+31,sc->accent);
        } else if(((x+y)&3)==0) DrawPixel(sx+16,sy+16,sc->accent);
        break;
    default: break;
    }
}
static void scene_wall(const scene_t *sc,int sx,int sy,int x,int y)
{
    DrawRectangle(sx+1,sy+1,30,30,sc->wall);
    DrawRectangle(sx+1,sy+1,30,2,sc->wall_hi);
    DrawLine(sx+1,sy+16,sx+30,sy+16,sc->wall_lo);
    DrawLine(sx+16,sy+3,sx+16,sy+15,sc->wall_lo);
    DrawLine(sx+9,sy+17,sx+9,sy+30,sc->wall_lo);
    switch(sc->style) {
    case SCENE_GATE:
        if(((x+y)&1)==0) DrawLine(sx+21,sy+21,sx+25,sy+26,sc->wall_lo);
        break;
    case SCENE_ARMORY:
        DrawRectangle(sx+4,sy+4,2,2,sc->accent);
        DrawRectangle(sx+26,sy+4,2,2,sc->accent);
        DrawRectangle(sx+4,sy+26,2,2,sc->accent);
        DrawRectangle(sx+26,sy+26,2,2,sc->accent);
        break;
    case SCENE_ARCHIVE:
        DrawRectangle(sx+3,sy+4,2,10,sc->wall_hi);
        DrawRectangle(sx+27,sy+18,2,10,sc->wall_hi);
        break;
    case SCENE_GARDEN:
        DrawRectangle(sx+1,sy+27,30,3,sc->wall_lo);
        DrawRectangle(sx+4+((x*5+y)%18),sy+4,2,5,sc->accent);
        break;
    case SCENE_CISTERN:
        DrawLine(sx+2,sy+25,sx+29,sy+25,sc->wall_hi);
        DrawPixel(sx+8+((x+y)%16),sy+6,sc->accent);
        break;
    case SCENE_BARRACKS:
        if(((x*3+y)&3)==0) DrawRectangle(sx+14,sy+2,4,14,sc->accent);
        else DrawLine(sx+2,sy+23,sx+29,sy+23,sc->wall_lo);
        break;
    case SCENE_CRYPT:
        DrawPixel(sx+10,sy+10,sc->wall_hi);
        DrawPixel(sx+21,sy+10,sc->wall_hi);
        break;
    case SCENE_SAND:
        DrawLine(sx+3,sy+8,sx+29,sy+8,sc->wall_lo);
        DrawLine(sx+3,sy+24,sx+24,sy+24,sc->wall_lo);
        if(((x*7+y)&3)==0) DrawRectangle(sx+15,sy+14,2,3,sc->accent);
        break;
    case SCENE_ASTRAL:
        DrawPixel(sx+5+((x*7+y*3)%22),sy+5+((x*5+y*11)%22),sc->accent);
        DrawPixel(sx+5+((x*11+y*7)%22),sy+5+((x*3+y*5)%22),sc->wall_hi);
        break;
    case SCENE_THRONE:
        DrawRectangle(sx+1,sy+28,30,2,sc->accent);
        DrawPixel(sx+6,sy+6,sc->wall_hi);
        break;
    default: break;
    }
}

static void castle(int cy)
{
    for(int k=0;k<3;++k) {
        int x=193+k*33, y=cy+(k==1?-10:0);
        DrawRectangle(x,y,26,55,STONE);
        for(int i=0;i<3;++i) DrawRectangle(x+i*10,y-8,6,10,BRASS);
        for(int i=1;i<4;++i) DrawLine(x,y+i*13,x+24,y+i*13,EDGE);
        if(k==1) {
            DrawRectangle(x+9,y+11,8,10,INK);
            DrawRectangle(x+6,y+33,14,4,INK);
            DrawRectangle(x+8,y+37,10,18,INK);
        } else {
            DrawRectangle(x+9,y+10,8,15,INK);
        }
    }
    DrawRectangle(240,cy-36,2,20,BRASS);
    DrawTriangle((Vector2){242,cy-36},(Vector2){242,cy-22},(Vector2){260,cy-29},RED_INK);
}
static void settings_button(const game_snapshot_t *s)
{
    const int x=GAME_SETTINGS_X,y=GAME_SETTINGS_Y;
    const int cx=x+GAME_SETTINGS_W/2,cy=y+GAME_SETTINGS_H/2;
    const Color ink=s->settings_open?BRASS:WHITE_INK;
    DrawRectangle(x,y,GAME_SETTINGS_W,GAME_SETTINGS_H,PANEL);
    DrawRectangleLines(x,y,GAME_SETTINGS_W,GAME_SETTINGS_H,s->settings_open?BRASS:EDGE);
    DrawRectangle(cx-4,cy-14,8,28,ink);
    DrawRectangle(cx-14,cy-4,28,8,ink);
    DrawRectangle(cx-10,cy-10,20,20,ink);
    DrawCircle(cx,cy,6,PANEL);
}
static void settings_slider(int x,int y,int w,int h,int percent)
{
    const int filled=w*percent/100;
    DrawRectangle(x,y,w,h,EDGE);
    DrawRectangle(x,y,filled,h,BRASS);
    int knob=x+filled;
    if(knob>x+w-2) knob=x+w-2;
    DrawRectangle(knob-4,y-4,8,h+8,WHITE_INK);
}
static void settings_modal(const game_snapshot_t *s)
{
    char text[12];
    DrawRectangle(0,0,GAME_WIDTH,GAME_HEIGHT,Fade(INK,.84f));
    DrawRectangle(GAME_SETTINGS_PANEL_X,GAME_SETTINGS_PANEL_Y,
                  GAME_SETTINGS_PANEL_W,GAME_SETTINGS_PANEL_H,PANEL);
    DrawRectangleLines(GAME_SETTINGS_PANEL_X,GAME_SETTINGS_PANEL_Y,
                       GAME_SETTINGS_PANEL_W,GAME_SETTINGS_PANEL_H,BRASS);
    ui_draw(tx(s,&T_SETTINGS),80,88,2,BRASS);
    DrawRectangle(GAME_SETTINGS_CLOSE_X,GAME_SETTINGS_CLOSE_Y,
                  GAME_SETTINGS_CLOSE_W,GAME_SETTINGS_CLOSE_H,INK);
    DrawRectangleLines(GAME_SETTINGS_CLOSE_X,GAME_SETTINGS_CLOSE_Y,
                       GAME_SETTINGS_CLOSE_W,GAME_SETTINGS_CLOSE_H,EDGE);
    const int cx=GAME_SETTINGS_CLOSE_X+GAME_SETTINGS_CLOSE_W/2;
    const int cy=GAME_SETTINGS_CLOSE_Y+GAME_SETTINGS_CLOSE_H/2;
    DrawLine(cx-6,cy-6,cx+6,cy+6,WHITE_INK);
    DrawLine(cx-6,cy+6,cx+6,cy-6,WHITE_INK);
    ui_draw(tx(s,&T_LANGUAGE),80,151,1,WHITE_INK);
    button(s,&T_CHINESE,GAME_SETTINGS_ZH_X,GAME_SETTINGS_ZH_Y,
           GAME_SETTINGS_ZH_W,GAME_SETTINGS_ZH_H,s->lang==GAME_LANG_ZH);
    button(s,&T_ENGLISH,GAME_SETTINGS_EN_X,GAME_SETTINGS_EN_Y,
           GAME_SETTINGS_EN_W,GAME_SETTINGS_EN_H,s->lang==GAME_LANG_EN);
    ui_draw(tx(s,&T_VOLUME),80,195,1,WHITE_INK);
    snprintf(text,sizeof(text),"%u%%",s->volume);
    ui_draw(text,400-ui_width(text,1),195,1,BRASS);
    settings_slider(GAME_SETTINGS_VOLUME_X,GAME_SETTINGS_VOLUME_Y,
                    GAME_SETTINGS_VOLUME_W,GAME_SETTINGS_VOLUME_H,s->volume);
    ui_draw(tx(s,&T_BRIGHTNESS),80,271,1,WHITE_INK);
    snprintf(text,sizeof(text),"%u%%",s->brightness);
    ui_draw(text,400-ui_width(text,1),271,1,BRASS);
    settings_slider(GAME_SETTINGS_BRIGHTNESS_X,GAME_SETTINGS_BRIGHTNESS_Y,
                    GAME_SETTINGS_BRIGHTNESS_W,GAME_SETTINGS_BRIGHTNESS_H,
                    (s->brightness-GAME_BRIGHTNESS_MIN)*100/(GAME_BRIGHTNESS_MAX-GAME_BRIGHTNESS_MIN));
    ui_center(tx(s,&T_REPOSITORY),343,1,MUTED);
    ascii_center("github.com/Wzdhehe/",368,16,KEY_BLUE);
    ascii_center("mosaico-magic-tower",388,16,KEY_BLUE);
    if(s->preferences_save_failed) ui_center(tx(s,&T_PREFERENCES_FAILED),418,1,RED_INK);
}
static void title_label(const game_snapshot_t *s)
{
    if(s->lang==GAME_LANG_EN) {
        ui_center(tx(s,&T_TITLE),205,2,WHITE_INK);
        return;
    }
    const int x=(GAME_WIDTH-ui_width(tx(s,&T_TITLE),2))/2;
    /* 魔's ink fills rows 1..15, 塔's rows 0..13. A 3 px rise for 塔 centres
     * their visual bounds without changing the shared 16 px font cell. */
    ui_draw("魔",x,205,2,WHITE_INK);
    ui_draw("塔",x+34,208,2,WHITE_INK);
}
static void title(const game_snapshot_t *s)
{
    ascii_center("MOSAICO ORIGINAL",42,16,MUTED);
    ascii_center("Created by Ponkan",62,16,MUTED);
    castle(135);
    title_label(s);
    ui_center(tx(s,&T_SUBTITLE),248,1,BRASS);
    button(s,&T_NEW,140,280,200,38,true);
    button(s,s->has_save?&T_CONTINUE:&T_NO_SAVE,140,326,200,34,false);
    ui_center(tx(s,&T_HINT1),383,1,MUTED);
    ui_center(tx(s,&T_HINT2),404,1,MUTED);
    ascii_center(GAME_VERSION,448,16,MUTED);
}
static bool gain_message(const game_snapshot_t *s,char *out,size_t size,Color *color)
{
    const bool en=s->lang==GAME_LANG_EN;
    *color=MINT;
    switch(s->gain_kind){
    case GAME_GAIN_HP:
        snprintf(out,size,"%s  HP +%lu",en?"HEALING POTION":"生命药水",(unsigned long)s->gain_value);
        *color=MINT; return true;
    case GAME_GAIN_ATTACK:
        snprintf(out,size,"%s  ATK +%lu",en?"ATTACK GEM":"攻击宝石",(unsigned long)s->gain_value);
        *color=RED_INK; return true;
    case GAME_GAIN_DEFENCE:
        snprintf(out,size,"%s  DEF +%lu",en?"DEFENCE GEM":"防御宝石",(unsigned long)s->gain_value);
        *color=KEY_BLUE; return true;
    case GAME_GAIN_KEY:
        snprintf(out,size,"%s  +%lu",
                 s->gain_color==1?(en?"BLUE KEY":"蓝钥匙"):
                 s->gain_color==2?(en?"RED KEY":"红钥匙"):(en?"YELLOW KEY":"黄钥匙"),
                 (unsigned long)s->gain_value);
        *color=s->gain_color==1?KEY_BLUE:s->gain_color==2?RED_INK:BRASS; return true;
    case GAME_GAIN_GOLD:
        snprintf(out,size,"%s  +%lu",en?"GOLD":"金币",(unsigned long)s->gain_value);
        *color=BRASS; return true;
    default: return false;
    }
}
/* A terminal-bodied tower builder, not the robed village NPC. Map and dialog
 * use the same original pixel drawing at integer scales, without animation. */
static void gpt_robot(int x,int y,int scale)
{
    const Color shell={151,200,183,255},screen={24,48,45,255};
    DrawRectangle(x+15*scale,y+1*scale,2*scale,4*scale,MINT);
    DrawRectangle(x+7*scale,y+5*scale,18*scale,12*scale,shell);
    DrawRectangle(x+9*scale,y+7*scale,14*scale,7*scale,screen);
    DrawRectangle(x+11*scale,y+9*scale,3*scale,2*scale,MINT);
    DrawRectangle(x+18*scale,y+9*scale,3*scale,2*scale,MINT);
    DrawRectangle(x+10*scale,y+18*scale,12*scale,10*scale,EDGE);
    DrawRectangle(x+13*scale,y+20*scale,6*scale,5*scale,MINT);
    DrawRectangle(x+15*scale,y+21*scale,2*scale,3*scale,screen);
    DrawRectangle(x+5*scale,y+19*scale,4*scale,7*scale,shell);
    DrawRectangle(x+23*scale,y+19*scale,4*scale,7*scale,shell);
    DrawRectangle(x+9*scale,y+28*scale,5*scale,3*scale,BRASS);
    DrawRectangle(x+18*scale,y+28*scale,5*scale,3*scale,BRASS);
}
static bool is_gpt(const game_snapshot_t *s,int x,int y)
{ return s->tower.floor==46u && x==12 && y==14; }
/* Chinese uses the readable 24px bitmap path; English keeps 16px text.
 * Wrap by actual advances; complete authored pages must fit without clipping. */
static int campaign_draw_paragraph(const char *text,int x,int y,int width,int lines,bool large)
{
    int used=0;
    while(text && *text) {
        const char *start=text,*end=text,*last_space=NULL;
        int pixels=0;
        while(*end) {
            const char *next=end; uint32_t cp;
            utf8_next(&next,&cp);
            const int step=cp<0x80?(large?18:12):(large?25:17);
            if(pixels+step-1>width || next-start>=94) break;
            if(cp==' ') last_space=end;
            pixels+=step; end=next;
        }
        if(used>=lines || end==start) return -1;
        if(*end && last_space && last_space>start) end=last_space;
        char line[96]; const size_t bytes=(size_t)(end-start);
        memcpy(line,start,bytes);line[bytes]=0;
        if(large) ui_draw_cjk15(line,x,y+used*28,WHITE_INK);
        else ui_draw(line,x,y+used*22,1,WHITE_INK);
        ++used;
        text=end;while(*text==' ')++text;
    }
    return used;
}

static void story_modal(const game_snapshot_t *s)
{
    const campaign_page_t *story=campaign_dialogue_page(s->dialogue_id,s->dialogue_variant,s->dialogue_page);
    if(!story || s->dialogue_id>=CAMPAIGN_DIALOGUE_COUNT) return;
    const campaign_dialogue_t *entry=&campaign_dialogues[s->dialogue_id];
    const char *name=s->lang==GAME_LANG_EN?entry->name_en:entry->name_zh;
    DrawRectangle(0,0,480,480,Fade(INK,.82f));
    DrawRectangle(30,120,420,260,PANEL);
    DrawRectangleLines(30,120,420,260,BRASS);
    ui_draw_cjk15(name,48,138,BRASS);
    DrawRectangle(38,160,96,150,INK);
    DrawRectangleLines(38,160,96,150,EDGE);
    if(is_gpt(s,(int)s->tower.x,(int)s->tower.y)) gpt_robot(38,185,3);
    else npc_portrait(38,160,s->tick);
    DrawLine(142,160,142,316,EDGE);
    const char *const *lines=s->lang==GAME_LANG_EN?story->en:story->zh;
    const bool large=s->lang!=GAME_LANG_EN;
    const int budget=large?5:6,line_height=large?28:22;
    const int first=campaign_draw_paragraph(lines[0],150,174,288,budget,large);
    if(first>=0) (void)campaign_draw_paragraph(lines[1],150,174+first*line_height,288,budget-first,large);
    button(s,&T_TALK_CONTINUE,180,330,120,32,false);
}
static bool view_has(const game_snapshot_t *s,int x,int y)
{
    return x>=(int)s->camera_x && x<(int)s->camera_x+TOWER_VIEW_W &&
           y>=(int)s->camera_y && y<(int)s->camera_y+TOWER_VIEW_H;
}
static void event_sprite(const game_snapshot_t *s,int x,int y,int sx,int sy)
{
    const int cx=sx+GAME_TILE/2, cy=sy+GAME_TILE/2;
    /* 14F water valve: a brass wheel below the valve keeper. */
    if(s->tower.floor==13u && x==3 && y==4) {
        DrawCircle(cx,cy,8,BRASS);
        DrawCircle(cx,cy,3,INK);
        DrawLine(cx-7,cy,cx+7,cy,INK);
        DrawLine(cx,cy-7,cx,cy+7,INK);
        DrawPixel(cx,cy,WHITE_INK);
        return;
    }
    /* 12F / 27F / 35F star marks. */
    if((s->tower.floor==11u && x==4 && y==3) ||
       (s->tower.floor==26u && x==5 && y==6) ||
       (s->tower.floor==34u && x==11 && y==9)) {
        DrawTriangle((Vector2){cx,cy-8},(Vector2){cx-5,cy},(Vector2){cx+5,cy},MINT);
        DrawTriangle((Vector2){cx,cy+8},(Vector2){cx-5,cy},(Vector2){cx+5,cy},MINT);
        DrawCircle(cx,cy,2,INK);
        return;
    }
    /* 18F / 34F / 43F errand objects. */
    if((s->tower.floor==17u && x==7 && y==7) ||
       (s->tower.floor==33u && x==3 && y==5) ||
       (s->tower.floor==42u && x==13 && y==11)) {
        DrawRectangle(cx-6,cy-4,12,9,(Color){139,103,63,255});
        DrawRectangle(cx-4,cy-7,8,4,BRASS);
        DrawLine(cx-6,cy,cx+6,cy,INK);
        DrawPixel(cx,cy-1,WHITE_INK);
        return;
    }
    DrawRectangle(cx-3,cy-3,6,6,Fade(BRASS,.8f));
}
static void dungeon(game_handle_t game,const game_snapshot_t *s)
{
    char text[96];
    char label[24];
    const unsigned slot=s->tower.floor;
    static const unsigned region_scene[5]={SCENE_GATE,SCENE_CISTERN,SCENE_ARMORY,SCENE_ARCHIVE,SCENE_ASTRAL};
    const scene_t *sc=&SCENES[slot==49u?SCENE_THRONE:region_scene[slot/10u]];
    DrawRectangle(0,0,GAME_WIDTH,GAME_HEIGHT,sc->bg);
    DrawRectangleLines(3,3,GAME_WIDTH-6,GAME_HEIGHT-6,sc->wall_hi);
    if(s->tower.floor+1>=TOWER_FLOORS)
        snprintf(label,sizeof(label),"%s",tx(s,&T_SUMMIT));
    else
        snprintf(label,sizeof(label),tx(s,&T_FLOOR),s->tower.floor+1);
    snprintf(text,sizeof(text),"%s · %s",label,tx(s,&SCENE_NAME[slot]));
    ui_center_fit(text,10,1,sc->accent,320);
    snprintf(text,sizeof(text),"HP %lu   ATK %lu   DEF %lu",(unsigned long)s->tower.hp,
             (unsigned long)s->tower.attack,(unsigned long)s->tower.defence);
    ascii_center(text,31,20,WHITE_INK);
    {
        /* A uint32_t value needs up to ten digits and a terminator. */
        char k0[11],k1[11],k2[11],gold[11];
        snprintf(k0,sizeof(k0),"%lu",(unsigned long)s->tower.keys[0]);
        snprintf(k1,sizeof(k1),"%lu",(unsigned long)s->tower.keys[1]);
        snprintf(k2,sizeof(k2),"%lu",(unsigned long)s->tower.keys[2]);
        snprintf(gold,sizeof(gold),"%lu",(unsigned long)s->tower.gold);
        const char *key_label=tx(s,&T_KEY), *gold_label=tx(s,&T_GOLD);
        const int sep=5, label_gap=7, group_gap=18, slash_w=ui_width("/",1);
        int w=ui_width(key_label,1)+label_gap;
        w+=ui_width(k0,1)+sep+slash_w+sep+ui_width(k1,1)+sep+slash_w+sep+ui_width(k2,1);
        w+=group_gap+ui_width(gold_label,1)+label_gap+ui_width(gold,1);
        int x=(GAME_WIDTH-w)/2;
        ui_draw(key_label,x,54,1,MUTED); x+=ui_width(key_label,1)+label_gap;
        ui_draw(k0,x,54,1,BRASS); x+=ui_width(k0,1)+sep;
        ui_draw("/",x,54,1,MUTED); x+=slash_w+sep;
        ui_draw(k1,x,54,1,KEY_BLUE); x+=ui_width(k1,1)+sep;
        ui_draw("/",x,54,1,MUTED); x+=slash_w+sep;
        ui_draw(k2,x,54,1,RED_INK); x+=ui_width(k2,1)+group_gap;
        ui_draw(gold_label,x,54,1,MUTED); x+=ui_width(gold_label,1)+label_gap;
        ui_draw(gold,x,54,1,BRASS);
    }
    /* Only the sides that end at a real map edge get a frame. A side with more
     * map beyond it stays open, so the extension is obvious at low brightness
     * without relying on the translucent band. */
    {
        const int x=GAME_MAP_X, y=GAME_MAP_Y;
        const int w=GAME_TILE*TOWER_VIEW_W, h=GAME_TILE*TOWER_VIEW_H;
        if(s->camera_y==0) DrawRectangle(x-4,y-4,w+8,4,sc->wall_hi);
        if(s->camera_y>=(unsigned)camera_max_y(s)) DrawRectangle(x-4,y+h,w+8,4,sc->wall_hi);
        if(s->camera_x==0) DrawRectangle(x-4,y-4,4,h+8,sc->wall_hi);
        if(s->camera_x>=(unsigned)camera_max_x(s)) DrawRectangle(x+w,y-4,4,h+8,sc->wall_hi);
        const int x0=x-3, y0=y-3, x1=x+w+2, y1=y+h+2;
        if(s->camera_x==0) DrawLine(x0,y0,x0,y1,sc->accent);
        if(s->camera_x>=(unsigned)camera_max_x(s)) DrawLine(x1,y0,x1,y1,sc->accent);
        if(s->camera_y==0) DrawLine(x0,y0,x1,y0,sc->accent);
        if(s->camera_y>=(unsigned)camera_max_y(s)) DrawLine(x0,y1,x1,y1,sc->accent);
    }
    /* Every map lookup and hit test goes through the camera origin. */
    for(int vy=0;vy<TOWER_VIEW_H;++vy) for(int vx=0;vx<TOWER_VIEW_W;++vx) {
        const int x=(int)s->camera_x+vx, y=(int)s->camera_y+vy;
        const int sx=GAME_MAP_X+vx*GAME_TILE, sy=GAME_MAP_Y+vy*GAME_TILE;
        const tower_tile_t tile=game_tile(game,x,y);
        if(tile==TOWER_TILE_WALL || tile==TOWER_TILE_SECRET) {
            scene_wall(sc,sx,sy,x,y);
            if(tile==TOWER_TILE_SECRET) {
                /* A concealed door keeps the wall's masonry, then darkens it.
                 * It must not read as a floor tile: the brick seams and the
                 * recessed edge are the cue, so low-brightness panels still
                 * show a darker wall block rather than a normal path. */
                DrawRectangle(sx+1,sy+1,GAME_TILE-2,GAME_TILE-2,Fade(INK,.36f));
                DrawRectangleLines(sx+3,sy+3,GAME_TILE-6,GAME_TILE-6,Fade(INK,.55f));
                DrawLine(sx+5,sy+4,sx+GAME_TILE-6,sy+4,Fade(sc->wall_hi,.35f));
                DrawLine(sx+5,sy+GAME_TILE-4,sx+GAME_TILE-6,sy+GAME_TILE-4,
                         Fade(INK,.65f));
            }
        } else {
            scene_floor(sc,sx,sy,x,y);
            /* No time-driven animation: an unchanged scene stays pixel-identical. */
            if(tile==TOWER_TILE_NPC && is_gpt(s,x,y)) gpt_robot(sx,sy,1);
            else tile_sprite(tile,sx,sy,0,sc->accent);
            if(tile==TOWER_TILE_EVENT) event_sprite(s,x,y,sx,sy);
        }
    }
    if(s->tower.cleared && s->tower.floor+1>=TOWER_FLOORS) {
        if(view_has(s,5,1)) beacon(GAME_MAP_X+(5-(int)s->camera_x)*GAME_TILE,
                                   GAME_MAP_Y+(1-(int)s->camera_y)*GAME_TILE,0);
        if(view_has(s,6,1)) princess(GAME_MAP_X+(6-(int)s->camera_x)*GAME_TILE,
                                     GAME_MAP_Y+(1-(int)s->camera_y)*GAME_TILE,0);
    }
    const int hx=GAME_MAP_X+((int)s->tower.x-(int)s->camera_x)*GAME_TILE;
    const int hy=GAME_MAP_Y+((int)s->tower.y-(int)s->camera_y)*GAME_TILE;
    DrawRectangleLines(hx,hy,GAME_TILE,GAME_TILE,sc->accent);
    knight(hx+3,hy+3,true);
    camera_edges(s,sc->bg);
    const text_t *msg=NULL;
    char gain_text[64];
    Color gain_color=MINT;
    const bool has_gain=gain_message(s,gain_text,sizeof(gain_text),&gain_color);
    if(s->notice_id<sizeof(NOTICE_TEXT)/sizeof(NOTICE_TEXT[0])) msg=&NOTICE_TEXT[s->notice_id];
    if(!msg||!msg->zh[0]||!msg->en[0]) msg=&STATUS_TEXT[s->tower.status<sizeof(STATUS_TEXT)/sizeof(STATUS_TEXT[0])?s->tower.status:0];
    if(s->tower.cleared&&!s->notice_id) msg=&T_SUMMIT_REACHED;
    if(has_gain) ui_center_fit(gain_text,430,1,gain_color,440);
    else ui_center(tx(s,msg),430,1,s->tower.status==TOWER_STATUS_TOO_STRONG?RED_INK:MINT);
    button(s,&T_TELEPORT,footer_button_x(0),GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H,false);
    button(s,&T_RESTART_GAME,footer_button_x(1),GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H,false);
}
static const text_t *enemy_text(tower_tile_t tile)
{
    static const text_t slime={"史莱姆","SLIME"},bat={"蝙蝠","BAT"},knight_t={"骑士","KNIGHT"};
    static const text_t sentinel={"哨兵","SENTINEL"},mage_t={"法师","MAGE"},golem_t={"石魔","GOLEM"};
    static const text_t wraith_t={"怨灵","WRAITH"},demon_t={"魔王","DEMON KING"};
    switch(tile){
    case TOWER_TILE_SLIME: return &slime;
    case TOWER_TILE_BAT: return &bat;
    case TOWER_TILE_KNIGHT: return &knight_t;
    case TOWER_TILE_SENTINEL: return &sentinel;
    case TOWER_TILE_MAGE: return &mage_t;
    case TOWER_TILE_GOLEM: return &golem_t;
    case TOWER_TILE_WRAITH: return &wraith_t;
    case TOWER_TILE_DEMON: return &demon_t;
    default: return &slime;
    }
}
static void modal(const game_snapshot_t *s)
{
    char text[80];
    DrawRectangle(0,0,480,480,Fade(INK,.80f));
    DrawRectangle(111,157,258,177,PANEL);
    DrawRectangleLines(111,157,258,177,BRASS);
    if(s->screen==GAME_NEW_CONFIRM) {
        ui_center_fit(tx(s,&T_RESTART),183,2,BRASS,246);
        ui_center_fit(tx(s,&T_UNSAVED),227,1,MUTED,246);
        button(s,&T_BACK,135,265,100,36,false); button(s,&T_NEW_GAME,245,265,100,36,true);
    } else {
        ui_center(tx(s,enemy_text(s->tower.pending.tile)),172,2,BRASS);
        combat_stat("HP",(unsigned long)s->tower.pending.hp,165);
        combat_stat("ATK",(unsigned long)s->tower.pending.attack,240);
        combat_stat("DEF",(unsigned long)s->tower.pending.defence,315);
        snprintf(text,sizeof(text),"%s %lu HP",tx(s,&T_DAMAGE),(unsigned long)s->tower.pending.damage);
        ui_center(text,238,1,s->tower.pending.damage?RED_INK:MINT);
        button(s,&T_BACK,145,281,90,36,false); button(s,&T_FIGHT,245,281,90,36,true);
    }
}
static void shop_modal(const game_snapshot_t *s)
{
    char text[96];
    DrawRectangle(0,0,480,480,Fade(INK,.80f));
    DrawRectangle(111,140,258,220,PANEL);
    DrawRectangleLines(111,140,258,220,BRASS);
    ui_center(tx(s,&T_SHOP),156,2,BRASS);
    snprintf(text,sizeof(text),"%s %lu",tx(s,&T_GOLD),(unsigned long)s->tower.gold);
    ui_center(text,198,1,WHITE_INK);
    snprintf(text,sizeof(text),"%s  %lu%s",tx(s,&T_SHOP_ATK),
             (unsigned long)tower_shop_price(&s->tower),s->lang==GAME_LANG_EN?"G":"金");
    button(s,NULL,130,222,220,36,true);
    ui_draw(text,130+(220-ui_width(text,1))/2,222+(36-16)/2,1,INK);
    snprintf(text,sizeof(text),"%s  %lu%s",tx(s,&T_SHOP_DEF),
             (unsigned long)tower_shop_price(&s->tower),s->lang==GAME_LANG_EN?"G":"金");
    button(s,NULL,130,264,220,36,true);
    ui_draw(text,130+(220-ui_width(text,1))/2,264+(36-16)/2,1,INK);
    if(s->tower.status==TOWER_STATUS_ITEM) ui_center(tx(s,&T_BOUGHT),302,1,MINT);
    else if(s->tower.status==TOWER_STATUS_NEED_GOLD) ui_center(tx(s,&T_NO_GOLD),302,1,RED_INK);
    button(s,&T_LEAVE,130,326,220,28,false);
}
/* The teleport panel lists exactly the six chapter anchors. A locked anchor is
 * dim and says so; there is no page control and no other floor is listed. */
static void teleport_cell(const game_snapshot_t *s,unsigned anchor,int x,int y)
{
    char label[24];
    const unsigned floor=tower_anchor_floor(anchor);
    const bool unlocked=tower_anchor_unlocked(&s->tower,anchor);
    const bool current=unlocked&&floor==s->tower.floor;
    const bool selected=anchor==s->anchor_cursor;
    const Color border=selected?BRASS:(unlocked?EDGE:STONE);
    const Color label_ink=unlocked?(current?BRASS:WHITE_INK):MUTED;
    const Color sub_ink=unlocked?MUTED:(Color){72,80,94,255};
    DrawRectangle(x,y,GAME_TELEPORT_CELL_W,GAME_TELEPORT_CELL_H,
                  current?Fade(BRASS,.16f):PANEL);
    if(selected) DrawRectangleLines(x-2,y-2,
                                    GAME_TELEPORT_CELL_W+4,GAME_TELEPORT_CELL_H+4,BRASS);
    DrawRectangleLines(x,y,GAME_TELEPORT_CELL_W,GAME_TELEPORT_CELL_H,border);
    snprintf(label,sizeof(label),s->lang==GAME_LANG_EN?"%02uF":"%u层",floor+1u);
    ui_draw(label,x+12,y+8,1,label_ink);
    ui_draw(tx(s,unlocked?&REGION_NAME[anchor?anchor-1u:0u]:&T_ANCHOR_LOCKED),
            x+12,y+32,1,sub_ink);
    if(current) {
        const char *here=tx(s,&T_ANCHOR_HERE);
        ui_draw(here,x+GAME_TELEPORT_CELL_W-ui_width(here,1)-12,y+8,1,BRASS);
    }
}
static void teleport_modal(const game_snapshot_t *s)
{
    DrawRectangle(0,0,480,480,Fade(INK,.82f));
    DrawRectangle(GAME_TELEPORT_PANEL_X,GAME_TELEPORT_PANEL_Y,
                  GAME_TELEPORT_PANEL_W,GAME_TELEPORT_PANEL_H,PANEL);
    DrawRectangleLines(GAME_TELEPORT_PANEL_X,GAME_TELEPORT_PANEL_Y,
                       GAME_TELEPORT_PANEL_W,GAME_TELEPORT_PANEL_H,BRASS);
    ui_draw(tx(s,&T_TELEPORT_TITLE),52,112,1,BRASS);
    DrawRectangle(GAME_TELEPORT_CLOSE_X,GAME_TELEPORT_CLOSE_Y,
                  GAME_TELEPORT_CLOSE_W,GAME_TELEPORT_CLOSE_H,INK);
    DrawRectangleLines(GAME_TELEPORT_CLOSE_X,GAME_TELEPORT_CLOSE_Y,
                       GAME_TELEPORT_CLOSE_W,GAME_TELEPORT_CLOSE_H,EDGE);
    {
        const int cx=GAME_TELEPORT_CLOSE_X+GAME_TELEPORT_CLOSE_W/2;
        const int cy=GAME_TELEPORT_CLOSE_Y+GAME_TELEPORT_CLOSE_H/2;
        DrawLine(cx-5,cy-5,cx+5,cy+5,WHITE_INK);
        DrawLine(cx-5,cy+5,cx+5,cy-5,WHITE_INK);
    }
    for(int row=0;row<3;++row) for(int col=0;col<2;++col) {
        const unsigned anchor=(unsigned)(row*2+col);
        if(anchor>=TOWER_ANCHORS) continue;
        teleport_cell(s,anchor,
                      GAME_TELEPORT_CELL_X+col*(GAME_TELEPORT_CELL_W+GAME_TELEPORT_GAP_X),
                      GAME_TELEPORT_CELL_Y+row*(GAME_TELEPORT_CELL_H+GAME_TELEPORT_GAP_Y));
    }
    ui_draw(tx(s,&T_ANCHOR_HINT),52,362,1,MUTED);
}
static void over_modal(const game_snapshot_t *s)
{
    char text[96];
    DrawRectangle(0,0,480,480,Fade(INK,.84f));
    DrawRectangle(GAME_END_PANEL_X,GAME_END_PANEL_Y,
                  GAME_END_PANEL_W,GAME_END_PANEL_H,PANEL);
    DrawRectangleLines(GAME_END_PANEL_X,GAME_END_PANEL_Y,
                       GAME_END_PANEL_W,GAME_END_PANEL_H,RED_INK);
    ui_center_fit(tx(s,&T_DEAD),126,2,RED_INK,296);
    snprintf(text,sizeof(text),tx(s,&T_FLOOR),s->tower.floor+1);
    ui_center(text,172,1,MUTED);
    snprintf(text,sizeof(text),"HP 0   ATK %lu   DEF %lu",
             (unsigned long)s->tower.attack,(unsigned long)s->tower.defence);
    ascii_center(text,200,16,WHITE_INK);
    snprintf(text,sizeof(text),"%s %lu    %s %lu",tx(s,&T_GOLD),
             (unsigned long)s->tower.gold,tx(s,&T_TURNS),(unsigned long)s->tower.turns);
    ui_center(text,228,1,BRASS);
    if(s->has_checkpoint) {
        button(s,&T_LOAD_CHECKPOINT,GAME_END_BUTTON_X,GAME_END_BUTTON1_Y,
               GAME_END_BUTTON_W,GAME_END_BUTTON_H,true);
        button(s,&T_NEW_GAME,GAME_END_BUTTON_X,GAME_END_BUTTON2_Y,
               GAME_END_BUTTON_W,GAME_END_BUTTON_H,false);
    } else {
        button(s,&T_NEW_GAME,GAME_END_BUTTON_X,GAME_END_BUTTON_Y,
               GAME_END_BUTTON_W,GAME_END_BUTTON_H,true);
    }
}
static void restart_modal(const game_snapshot_t *s)
{
    DrawRectangle(0,0,480,480,Fade(INK,.84f));
    DrawRectangle(GAME_RESTART_PANEL_X,GAME_RESTART_PANEL_Y,
                  GAME_RESTART_PANEL_W,GAME_RESTART_PANEL_H,PANEL);
    DrawRectangleLines(GAME_RESTART_PANEL_X,GAME_RESTART_PANEL_Y,
                       GAME_RESTART_PANEL_W,GAME_RESTART_PANEL_H,BRASS);
    ui_center(tx(s,&T_RESTART_GAME),126,2,BRASS);
    ui_center_fit(tx(s,&T_RESTART_HINT),160,1,MUTED,296);
    if(s->has_checkpoint)
        button(s,&T_LOAD_CHECKPOINT,GAME_RESTART_BUTTON_X,GAME_RESTART_RETRY_Y,
               GAME_RESTART_BUTTON_W,GAME_RESTART_BUTTON_H,true);
    else {
        DrawRectangleLines(GAME_RESTART_BUTTON_X,GAME_RESTART_RETRY_Y,
                           GAME_RESTART_BUTTON_W,GAME_RESTART_BUTTON_H,STONE);
        ui_center(tx(s,&T_NO_SAVE),GAME_RESTART_RETRY_Y+13,1,MUTED);
    }
    button(s,&T_NEW_GAME,GAME_RESTART_BUTTON_X,GAME_RESTART_NEW_Y,
           GAME_RESTART_BUTTON_W,GAME_RESTART_BUTTON_H,false);
    button(s,&T_CANCEL,GAME_RESTART_BUTTON_X,GAME_RESTART_CANCEL_Y,
           GAME_RESTART_BUTTON_W,GAME_RESTART_BUTTON_H,false);
    if(s->notice_id==GAME_NOTICE_NO_SAVE || s->notice_id==GAME_NOTICE_DAMAGED)
        ui_center_fit(tx(s,&T_RETRY_FAILED),366,1,RED_INK,296);
}
static void victory_modal(const game_snapshot_t *s)
{
    char text[96];
    DrawRectangle(0,0,480,480,Fade(INK,.84f));
    DrawRectangle(GAME_END_PANEL_X,GAME_END_PANEL_Y,
                  GAME_END_PANEL_W,GAME_END_PANEL_H,PANEL);
    DrawRectangleLines(GAME_END_PANEL_X,GAME_END_PANEL_Y,
                       GAME_END_PANEL_W,GAME_END_PANEL_H,BRASS);
    ui_center_fit(tx(s,&T_VICTORY),126,2,MINT,296);
    snprintf(text,sizeof(text),"HP %lu   ATK %lu   DEF %lu",
             (unsigned long)s->tower.hp,(unsigned long)s->tower.attack,
             (unsigned long)s->tower.defence);
    ascii_center(text,172,16,WHITE_INK);
    snprintf(text,sizeof(text),"%s %lu / %lu / %lu",tx(s,&T_KEY),
             (unsigned long)s->tower.keys[0],(unsigned long)s->tower.keys[1],
             (unsigned long)s->tower.keys[2]);
    ui_center(text,204,1,WHITE_INK);
    snprintf(text,sizeof(text),"%s %lu    %s %lu",tx(s,&T_GOLD),
             (unsigned long)s->tower.gold,tx(s,&T_TURNS),(unsigned long)s->tower.turns);
    ui_center(text,232,1,BRASS);
    button(s,&T_EXPLORE,GAME_END_BUTTON_X,GAME_END_BUTTON_Y,
           GAME_END_BUTTON_W,GAME_END_BUTTON_H,true);
}
bool game_render(game_handle_t game)
{
    const game_snapshot_t *s=game_read(game);
    if(!s) return false;
    BeginDrawing();
    if(!MosaicoFastFrameAvailable()) { EndDrawing(); return false; }
    /* A single full-canvas clear is sufficient. */
    ClearBackground(BG);
    DrawRectangleLines(3,3,GAME_WIDTH-6,GAME_HEIGHT-6,STONE);
    if(s->screen==GAME_TITLE) title(s);
    else dungeon(game,s);
    if(s->screen==GAME_NEW_CONFIRM || (s->screen==GAME_DUNGEON && s->tower.phase==TOWER_PHASE_COMBAT)) modal(s);
    if(s->screen==GAME_SHOP) shop_modal(s);
    if(s->screen==GAME_DIALOG) story_modal(s);
    if(s->screen==GAME_TELEPORT) teleport_modal(s);
    if(s->screen==GAME_RESTART_MENU) restart_modal(s);
    if(s->screen==GAME_OVER) over_modal(s);
    if(s->screen==GAME_VICTORY) victory_modal(s);
    if(s->paused) {
        DrawRectangle(110,204,260,60,PANEL); ui_center(tx(s,&T_PAUSED),226,1,BRASS);
    }
    if(s->settings_open) settings_modal(s);
    else if(s->preferences_save_failed) {
        DrawRectangle(56,426,368,19,PANEL);
        ui_center(tx(s,&T_PREFERENCES_FAILED),428,1,RED_INK);
    }
    settings_button(s);
    EndDrawing();
    game_render_complete(game);
    return true;
}

/* Device scheduling uses this path. An explicit Host screenshot uses
 * game_render() so that a new caller-owned target is always fully populated. */
bool game_render_pending(game_handle_t game)
{
    if (!game) return false;
    return !game_needs_redraw(game) || game_render(game);
}
