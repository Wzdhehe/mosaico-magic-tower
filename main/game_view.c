// SPDX-License-Identifier: MIT
// Original geometric pixel art and a generated 16x16 CJK bitmap subset.
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
#include "mosaico_raylib_fast.h"
#include "ui_font.h"

static const Color BG={14,19,28,255}, INK={9,13,20,255}, PANEL={25,32,44,255};
static const Color STONE={52,62,77,255}, EDGE={77,87,99,255};
static const Color BRASS={226,188,112,255}, WHITE_INK={234,230,215,255};
static const Color MUTED={139,158,167,255}, MINT={127,216,180,255};
static const Color RED_INK={225,110,111,255};
static const Color KEY_BLUE={107,179,232,255};
static const Color ROBE={156,126,196,255}, GOLEM_STONE={154,148,136,255};

typedef struct { const char *zh, *en; } text_t;
static const text_t T_TITLE={"魔塔","MAGIC TOWER"};
static const text_t T_SUBTITLE={"十层试炼","TEN FLOORS"};
static const text_t T_NEW={"新的旅程","NEW JOURNEY"};
static const text_t T_CONTINUE={"继续旅程","CONTINUE"};
static const text_t T_NO_SAVE={"暂无存档","NO SAVE YET"};
static const text_t T_HINT1={"点击相邻格子或滑动移动","Tap a neighbour or swipe to step"};
static const text_t T_HINT2={"查看伤害，选择战斗","Check the cost. Choose your battles"};
static const text_t T_FLOOR={"第 %u 层","FLOOR %02u"};
static const text_t T_SUMMIT={"塔顶","SUMMIT"};
static const text_t T_KEY={"钥匙","KEY"};
static const text_t T_GOLD={"金币","GOLD"};
static const text_t T_SAVE={"存档","SAVE"};
static const text_t T_LOAD={"读取","LOAD"};
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
static const text_t T_UNSAVED={"未保存的进度将丢失","Progress will be lost"};
static const text_t T_PAUSED={"暂停","PAUSED"};
static const text_t T_SUMMIT_REACHED={"魔王陨落，公主获救","DEMON KING FALLS - PRINCESS SAVED"};
static const text_t T_SHOP={"商店","SHOP"};
static const text_t T_SHOP_ATK={"攻击 +3","ATK +3"};
static const text_t T_SHOP_DEF={"防御 +3","DEF +3"};
static const text_t T_LEAVE={"离开","LEAVE"};
static const text_t T_BOUGHT={"已购买","BOUGHT"};
static const text_t T_NO_GOLD={"金币不足","NOT ENOUGH GOLD"};
static const text_t T_TALK_CONTINUE={"继续","CONTINUE"};

typedef struct { const char *zh[2], *en[2]; } story_t;
/* One short two-line conversation per floor. These carry the original
 * Magic Tower premise: the princess is held at the summit, and the tower's
 * coloured keys are the only way through its seals. */
static const story_t STORY[TOWER_FLOORS]={
    {{"魔王掳走了公主，囚禁在塔顶。","钥匙颜色对应门。别硬闯。"},
     {"The Demon King holds the princess at the summit.","Keys match doors. Do not rush in."}},
    {{"购买攻击或防御。","金币不足就离开。"},
     {"Buying power or defence costs gold.","Each purchase raises the next price."}},
    {{"西侧蓝门后藏着红钥匙。","四层有第二把蓝钥匙。"},
     {"The west vault hides a red key.","Find another blue key on floor four."}},
    {{"先开右侧蓝门继续前进。","左边红门藏着备用资源。"},
     {"The right blue gate is the way onward.","The left red vault holds spare supplies."}},
    {{"旧书库只认红钥匙。","没有就回三层的密室找。"},
     {"The archive seal needs a red key.","Search the sealed room on floor three."}},
    {{"法师守着防御宝石。","石魔守着攻击宝石。"},
     {"The mage guards defence.","The golem guards attack. Choose a path."}},
    {{"东侧怨灵守着红钥匙。","八层的封印也在等它。"},
     {"A wraith guards the eastern red key.","Floor eight has a matching seal."}},
    {{"牢房上方的红门封住出口。","资源不足就返回下层。"},
     {"A red lock seals the upper exit.","Go below if you need more supplies."}},
    {{"石魔守防御，怨灵守攻击。","先选择，再看伤害。"},
     {"Golem guards defence; wraith guards attack.","Each choice changes the final battle."}},
    {{"魔王就挡在最后的门前。","算清伤害，再决定是否开战。"},
     {"The Demon King blocks the final way.","Check the damage before you commit."}},
};

static const text_t NPC_NAME[TOWER_FLOORS]={
    {"守夜人","WATCHMAN"},
    {"吟游诗人","BARD"},
    {"侍女幽魂","GHOST MAID"},
    {"守卫亡魂","FALLEN GUARD"},
    {"老学者","SCHOLAR"},
    {"伤兵","WOUNDED SOLDIER"},
    {"黑祭司","DARK PRIESTESS"},
    {"骑士团长","KNIGHT COMMANDER"},
    {"公主回声","PRINCESS ECHO"},
    {"公主","PRINCESS"},
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
    {"有人想和你说话","Someone wants to talk"}
};
static const text_t NOTICE_TEXT[]={
    {"",""},
    {"向上攀登","Climb the tower"},
    {"旅程已保存","Journey saved."},
    {"暂无存档","No saved journey."},
    {"存档已损坏，旅程未变","Save damaged. Journey unchanged."},
    {"旅程已恢复","Journey restored."},
    {"保存失败，请重试","Save failed. Try again."},
    {"塔顶到达！可返回或探索","SUMMIT REACHED! Return or explore."}
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
static void ui_draw_cjk16(const char *text,int x,int y,Color color)
{ ui_draw(text,x,y,1,color); }
static int cjk_line_count(const char *text,int max_chars)
{
    int count=0; uint32_t cp;
    if(!text||!max_chars) return 0;
    while(*text){ utf8_next(&text,&cp); ++count; }
    return (count+max_chars-1)/max_chars;
}
static bool closing_punctuation(uint32_t cp)
{
    switch(cp){
    case 0x3002: case 0xFF0C: case 0xFF01: case 0xFF1F:
    case 0xFF1B: case 0xFF1A: case 0x3001: case 0xFF09:
    case 0x3011: case 0x300B: case 0x300D: case 0x300F:
    case 0x2026:
        return true;
    default:
        return false;
    }
}
static int ui_draw_wrap_cjk(const char *text,int x,int y,int max_chars,
                            int line_height,Color color,int max_lines,bool large)
{
    const char *start=text,*p=text;
    char line[128];
    int count=0,lines=0;
    while(*p&&lines<max_lines){
        if(count>=max_chars){
            const char *peek=p; uint32_t cp=0;
            utf8_next(&peek,&cp);
            if(!closing_punctuation(cp)){
                size_t n=(size_t)(p-start);
                if(n>=sizeof(line)) n=sizeof(line)-1;
                memcpy(line,start,n); line[n]=0;
                large?ui_draw_cjk15(line,x,y,color):ui_draw_cjk16(line,x,y,color);
                y+=line_height; ++lines; start=p; count=0;
                if(lines>=max_lines) break;
            }
        }
        const char *next=p; uint32_t cp;
        utf8_next(&next,&cp);
        p=next; ++count;
    }
    if(start<p&&lines<max_lines){
        size_t n=(size_t)(p-start);
        if(n>=sizeof(line)) n=sizeof(line)-1;
        memcpy(line,start,n); line[n]=0;
        large?ui_draw_cjk15(line,x,y,color):ui_draw_cjk16(line,x,y,color);
        ++lines;
    }
    return lines;
}
static void button(const game_snapshot_t *s,const text_t *label,int x,int y,int w,int h,bool active)
{
    const char *text=label?tx(s,label):"";
    DrawRectangle(x,y,w,h,active?BRASS:PANEL);
    DrawRectangleLines(x,y,w,h,active?BRASS:EDGE);
    if(text[0]) ui_draw(text,x+(w-ui_width(text,1))/2,y+(h-16)/2,1,active?INK:WHITE_INK);
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
static void tile_sprite(tower_tile_t tile,int x,int y,uint32_t tick)
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
            DrawRectangle(x+4+i*3,y+5+row*4,19-i*3,3,MUTED);
        }
    } else if(tile==TOWER_TILE_GOAL) {
        DrawRectangle(x+8,y+11,11,11,BRASS); DrawRectangle(x+5,y+6,4,8,BRASS);
        DrawRectangle(x+12,y+4,4,12,BRASS); DrawRectangle(x+19,y+6,4,8,BRASS);
        DrawRectangle(x+9,y+14,10,3,RED_INK);
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
static void story_modal(const game_snapshot_t *s)
{
    const story_t *story=&STORY[s->tower.floor<TOWER_FLOORS?s->tower.floor:0];
    const text_t *name=&NPC_NAME[s->tower.floor<TOWER_FLOORS?s->tower.floor:0];
    DrawRectangle(0,0,480,480,Fade(INK,.82f));
    DrawRectangle(30,120,420,260,PANEL);
    DrawRectangleLines(30,120,420,260,BRASS);
    ui_draw_cjk15(tx(s,name),48,138,BRASS);
    DrawRectangle(38,160,96,150,INK);
    DrawRectangleLines(38,160,96,150,EDGE);
    npc_portrait(38,160,s->tick);
    DrawLine(142,160,142,316,EDGE);
    if(s->lang==GAME_LANG_EN){
        char paragraph[220];
        snprintf(paragraph,sizeof(paragraph),"%s %s",story->en[0],story->en[1]);
        const char *p=paragraph;
        int y=172;
        while(*p&&y<284){
            int take=0,last_space=0;
            while(p[take]&&take<24){
                if(p[take]==' ') last_space=take;
                ++take;
            }
            if(p[take]&&last_space) take=last_space;
            char line[40];
            int n=take<39?take:39;
            memcpy(line,p,(size_t)n); line[n]=0;
            MosaicoFastDrawText(line,150+(290-MosaicoFastMeasureText(line,16))/2,y,16,WHITE_INK);
            p+=take;
            while(*p==' ') ++p;
            y+=22;
        }
    } else {
        const int l0=cjk_line_count(story->zh[0],11);
        const int l1=cjk_line_count(story->zh[1],11);
        const int s0=cjk_line_count(story->zh[0],17);
        const int s1=cjk_line_count(story->zh[1],17);
        if(l0+l1<=5){
            const int first=ui_draw_wrap_cjk(story->zh[0],150,174,11,28,WHITE_INK,l0,true);
            ui_draw_wrap_cjk(story->zh[1],150,174+first*28,11,28,WHITE_INK,l1,true);
        } else if(s0+s1<=7){
            const int first=ui_draw_wrap_cjk(story->zh[0],150,174,17,20,WHITE_INK,s0,false);
            ui_draw_wrap_cjk(story->zh[1],150,174+first*20,17,20,WHITE_INK,s1,false);
        } else {
            /* Far too long to rescue: keep the compact font and let the
             * authored-text layout test fail instead of inventing a tiny UI. */
            const int first=ui_draw_wrap_cjk(story->zh[0],150,174,17,20,WHITE_INK,3,false);
            ui_draw_wrap_cjk(story->zh[1],150,174+first*20,17,20,WHITE_INK,3,false);
        }
    }
    button(s,&T_TALK_CONTINUE,180,330,120,32,false);
}
static void dungeon(game_handle_t game,const game_snapshot_t *s)
{
    char text[96];
    if(s->tower.floor+1>=TOWER_FLOORS)
        ui_center(tx(s,&T_SUMMIT),10,1,BRASS);
    else {
        snprintf(text,sizeof(text),tx(s,&T_FLOOR),s->tower.floor+1);
        ui_center(text,10,1,BRASS);
    }
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
        ui_draw(key_label,x,58,1,MUTED); x+=ui_width(key_label,1)+label_gap;
        ui_draw(k0,x,58,1,BRASS); x+=ui_width(k0,1)+sep;
        ui_draw("/",x,58,1,MUTED); x+=slash_w+sep;
        ui_draw(k1,x,58,1,KEY_BLUE); x+=ui_width(k1,1)+sep;
        ui_draw("/",x,58,1,MUTED); x+=slash_w+sep;
        ui_draw(k2,x,58,1,RED_INK); x+=ui_width(k2,1)+group_gap;
        ui_draw(gold_label,x,58,1,MUTED); x+=ui_width(gold_label,1)+label_gap;
        ui_draw(gold,x,58,1,BRASS);
    }
    DrawRectangle(GAME_MAP_X-3,GAME_MAP_Y-3,GAME_TILE*TOWER_WIDTH+6,GAME_TILE*TOWER_HEIGHT+6,EDGE);
    for(int y=0;y<TOWER_HEIGHT;++y) for(int x=0;x<TOWER_WIDTH;++x) {
        const int sx=GAME_MAP_X+x*GAME_TILE, sy=GAME_MAP_Y+y*GAME_TILE;
        DrawRectangle(sx,sy,GAME_TILE,GAME_TILE,((x+y)&1)?PANEL:(Color){30,38,50,255});
        DrawPixel(sx+4,sy+20,STONE);
        /* No time-driven animation: an unchanged scene stays pixel-identical. */
        tile_sprite(game_tile(game,x,y),sx,sy,0);
    }
    if(s->tower.cleared && s->tower.floor+1>=TOWER_FLOORS) {
        beacon(GAME_MAP_X+5*GAME_TILE,GAME_MAP_Y+1*GAME_TILE,0);
        princess(GAME_MAP_X+6*GAME_TILE,GAME_MAP_Y+1*GAME_TILE,0);
    }
    const int hx=GAME_MAP_X+(int)s->tower.x*GAME_TILE, hy=GAME_MAP_Y+(int)s->tower.y*GAME_TILE;
    DrawRectangleLines(hx,hy,GAME_TILE,GAME_TILE,BRASS);
    knight(hx+3,hy+3,true);
    const text_t *msg=NULL;
    char gain_text[64];
    Color gain_color=MINT;
    const bool has_gain=gain_message(s,gain_text,sizeof(gain_text),&gain_color);
    if(s->notice_id<sizeof(NOTICE_TEXT)/sizeof(NOTICE_TEXT[0])) msg=&NOTICE_TEXT[s->notice_id];
    if(!msg||!msg->zh[0]||!msg->en[0]) msg=&STATUS_TEXT[s->tower.status<sizeof(STATUS_TEXT)/sizeof(STATUS_TEXT[0])?s->tower.status:0];
    if(s->tower.cleared&&!s->notice_id) msg=&T_SUMMIT_REACHED;
    if(has_gain) ui_center_fit(gain_text,430,1,gain_color,440);
    else ui_center(tx(s,msg),430,1,s->tower.status==TOWER_STATUS_TOO_STRONG?RED_INK:MINT);
    button(s,&T_SAVE,GAME_BUTTON_X,GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H,false);
    button(s,&T_LOAD,GAME_BUTTON_X+105,GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H,false);
    button(s,&T_NEW_GAME,GAME_BUTTON_X+210,GAME_FOOTER_Y,GAME_BUTTON_W,GAME_FOOTER_H,false);
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
