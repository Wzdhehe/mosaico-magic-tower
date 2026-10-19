/* Generated from approved campaign_content.py. Do not improvise copy. */
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { const char *zh[2], *en[2]; } campaign_page_t;
typedef struct { uint8_t floor,x,y,kind,page_count; int8_t quest; const char *name_zh,*name_en; const campaign_page_t *pages,*carrying,*done; } campaign_dialogue_t;
#define CAMPAIGN_DIALOGUE_NONE UINT16_MAX

static const campaign_page_t campaign_pages_0[] = {
    {{"四层北端有蓝钥匙。","九层红封门的钥匙在这里。"},{"A blue key waits on floor four.","The ninth seal needs our red key."}},
};
static const campaign_page_t campaign_pages_1[] = {
    {{"这把蓝钥匙先开哪一库？","旧库给盾，军械库给剑。"},{"Which vault gets the first blue key?","Archive: defence. Armory: attack."}},
};
static const campaign_page_t campaign_pages_2[] = {
    {{"别只看能不能打赢。","升级以后，能少挨几轮？"},{"Do more than ask if you can win.","How many hits will an upgrade save?"}},
};
static const campaign_page_t campaign_pages_3[] = {
    {{"左脚边有块较暗的墙砖。","走进去，房间才会显现。"},{"A darker wall lies to your left.","Step in to reveal the hidden room."}},
    {{"有些暗墙藏在别的地方。","找到以后，离开也不会消失。"},{"More dark walls wait elsewhere.","Revealed rooms stay discovered."}},
};
static const campaign_page_t campaign_pages_4[] = {
    {{"名册停在城破那一天。","队长身后还有很长的阶梯。"},{"The roll ends when the gate fell.","A long stair lies past the captain."}},
};
static const campaign_page_t campaign_pages_5[] = {
    {{"我的铜耳朵落在回流渠了。","十八层旧泵旁，水声最怪。"},{"I lost my copper ear in the channel.","The old pump on 18 sounds strange."}},
};
static const campaign_page_t campaign_carrying_5 = {{"你真的听见它了！","我把旧堤入口的淤泥清掉。"},{"You really heard it calling!","I will clear the old levee entrance."}};
static const campaign_page_t campaign_done_5 = {{"沿内岸向东找暗墙。","铜耳朵说，谢谢。"},{"Find a dark wall east of the bank.","The copper ear says thank you."}};
static const campaign_page_t campaign_pages_6[] = {
    {{"北刻线是盾，南刻线是剑。","十四层阀门能反复调整。"},{"North mark: shield. South: sword.","The valve on 14 can be turned back."}},
};
static const campaign_page_t campaign_pages_7[] = {
    {{"北库先露出，南库还在水下。","拨动脚边阀门，两库会交换。"},{"North is dry; south is underwater.","The valve swaps the two vaults."}},
    {{"十五层外岸始终可以通过。","拿完一边，可回来再换。"},{"The outer bank always stays open.","Return and turn it for the other."}},
};
static const campaign_page_t campaign_pages_8[] = {
    {{"我们把塔当成了牢房。","可公主是在里面守住什么？"},{"We thought the tower was a prison.","What has the princess been holding?"}},
};
static const campaign_page_t campaign_pages_9[] = {
    {{"东侧骑士身后有红钥匙。","先看看代价，也能回来再拿。"},{"The east knight guards a red key.","Check the cost. You can return."}},
};
static const campaign_page_t campaign_pages_10[] = {
    {{"右边那块墙砖没有积水。","铜耳朵，大概还躺在里面。"},{"The wall to the right stayed dry.","The copper ear may still be inside."}},
};
static const campaign_page_t campaign_pages_11[] = {
    {{"渡守只拦住向上的人。","他说，水下还有没回家的名字。"},{"The ferryman stops only climbers.","Below lie names that never went home."}},
};
static const campaign_page_t campaign_pages_12[] = {
    {{"石印亮后，这里就能传送。","回来时，不必再交一次代价。"},{"The lit seal becomes a warp anchor.","Your return will not cost a fight."}},
};
static const campaign_page_t campaign_pages_13[] = {
    {{"炉子还有火，却丢了影子。","三十四层有人拿它当旗子。"},{"My furnace lost its own shadow.","Someone flies it as a flag on 34."}},
};
static const campaign_page_t campaign_carrying_13 = {{"是它，还是一股焦味。","西边旧库，我替你揭开入口。"},{"That is it. Still smells of ash.","I will uncover the west store."}};
static const campaign_page_t campaign_done_13 = {{"影子总算肯跟着火走了。","旧库的暗墙就在我北边。"},{"The shadow follows the flame again.","The dark wall lies north of me."}};
static const campaign_page_t campaign_pages_14[] = {
    {{"有时只差一点攻击。","多敲一轮，就多挨一次反击。"},{"One more point can change a fight.","One extra round means one more hit."}},
};
static const campaign_page_t campaign_pages_15[] = {
    {{"所有商店共用一份账。","先买什么，要看下一道守卫。"},{"Every shop shares the same ledger.","Buy for the guard you face next."}},
};
static const campaign_page_t campaign_pages_16[] = {
    {{"东炉守卫身后有块暗墙。","炉中星记，记得带上。"},{"A dark wall waits behind the guard.","Take the furnace mark with you."}},
};
static const campaign_page_t campaign_pages_17[] = {
    {{"封的是余烬，不是宝石。","账房知道旧仓的入口。"},{"The seal holds embers, not jewels.","The ledger names the hidden mouth."}},
};
static const campaign_page_t campaign_pages_18[] = {
    {{"钥匙用在门上。","金币要在还用得着时花掉。"},{"Keys are meant to open doors.","Spend gold while it can still help."}},
};
static const campaign_page_t campaign_pages_19[] = {
    {{"公主借走了炉心的光。","她说，要让塔再撑一夜。"},{"The princess borrowed its light.","She needed the tower to last a night."}},
};
static const campaign_page_t campaign_pages_20[] = {
    {{"两翼守卫，习惯各不相同。","也有人一直替王城交钥匙。"},{"The two wings fight differently.","Some visitors pay the key toll."}},
};
static const campaign_page_t campaign_pages_21[] = {
    {{"月亮背面破了一个洞。","四十三层镜子里有那块补丁。"},{"The back of the moon has a hole.","Its patch is in the mirrors on 43."}},
};
static const campaign_page_t campaign_carrying_21 = {{"这块补丁，摸起来像夜色。","祭坛东南的旧室就送给你。"},{"This patch feels like night itself.","The southeast room is yours."}};
static const campaign_page_t campaign_done_21 = {{"以后满月就不会漏风了。","暗墙在祭坛下面，向东走。"},{"Full moons will no longer leak air.","Below the altar, east: a dark wall."}};
static const campaign_page_t campaign_pages_22[] = {
    {{"水纹、炉火、王冠。","三枚星记开四十七层旧室。"},{"Water, furnace, crown. Three marks.","They open the old room on 47."}},
    {{"水纹在十二层泵房。","炉火在二十七层账房。"},{"Water: the pump room on 12.","Furnace: the ledger room on 27."}},
};
static const campaign_page_t campaign_pages_23[] = {
    {{"王冠缺口总朝南。","床脚正中的墙砖颜色较暗。"},{"The broken crown points south.","A darker wall lies below the bed."}},
};
static const campaign_page_t campaign_pages_24[] = {
    {{"守誓者没有背叛公主。","他只是不肯让封印被误开。"},{"The oathkeeper never betrayed her.","He will not risk opening the seal."}},
};
static const campaign_page_t campaign_pages_25[] = {
    {{"如果你听见这里，封印快尽了。","别救一个影子，来救整座城。"},{"If you hear this, the seal is weak.","Come save the city, not a shadow."}},
};
static const campaign_page_t campaign_pages_26[] = {
    {{"高处卫兵的盔甲难以击穿。","先看伤害，别向空血量许愿。"},{"The seal guard wears heavy armour.","Read the damage before you strike."}},
};
static const campaign_page_t campaign_pages_27[] = {
    {{"水纹不是钥匙，是归路。","星记没拿齐，也能抵达王座。"},{"The water mark remembers a way home.","You need no marks to reach the king."}},
};
static const campaign_page_t campaign_pages_28[] = {
    {{"镜子照不见月亮的背面。","但脚边那块布，总在发光。"},{"No mirror sees the back of the moon.","That patch by my feet keeps glowing."}},
};
static const campaign_page_t campaign_pages_29[] = {
    {{"王座上的影子不会追你下楼。","四十四层蓝库还能换大药。"},{"The king will not chase you below.","The blue vault on 44 holds medicine."}},
};
static const campaign_page_t campaign_pages_30[] = {
    {{"旧营、东炉、旧王寝。","还有那些一直等你回去的人。"},{"Old camp. East furnace. Royal bed.","And everyone waiting for your return."}},
};
static const campaign_page_t campaign_pages_31[] = {
    {{"水纹、炉火、王冠都记下了吗？","此室不挡通往王座的道路。"},{"Have you kept water, fire and crown?","This room never bars the throne."}},
};
static const campaign_page_t campaign_pages_32[] = {
    {{"我是这个游戏的真正开发者，","gpt6.1sol。"},{"I am the true developer of this game,","gpt6.1sol."}},
    {{"这座塔的代码，","都是我一行一行写出来的。"},{"The code of this tower:","I wrote it, one line at a time."}},
    {{"不过隐藏房间里的宝物，","你得自己找。"},{"The hidden treasures, though?","You will have to find those yourself."}},
};
static const campaign_page_t campaign_pages_33[] = {
    {{"再向上，就是王座。","传送回去准备，门不会重关。"},{"The next stair leads to the throne.","Return to prepare. Doors stay open."}},
};
static const campaign_page_t campaign_pages_34[] = {
    {{"我在魔王身后维持最后的封印。","看清伤害，不够就再回去准备。"},{"I hold the last seal behind the king.","Check the cost. Return if needed."}},
};
static const campaign_page_t campaign_pages_35[] = {
    {{"你来了。封印终于可以放下了。","我守着的，是城里每个等归人的家。"},{"You came. I can let the seal go.","I held it for everyone waiting below."}},
    {{"谢谢你，也谢谢那些等你回去的人。","走吧。这一次，我们一起下楼。"},{"Thank you. Others are waiting for you.","Come. This time, we go down together."}},
};
static const campaign_page_t campaign_pages_36[] = {
    {{"记下了泵房的水纹。","四十七层旧室还在等待。"},{"You record the water mark.","An old room waits on floor 47."}},
};
static const campaign_page_t campaign_pages_37[] = {
    {{"北库露出，南库还在水下。","再来拨动，可交换两座库房。"},{"North is dry; south is underwater.","Turn again to swap the two vaults."}},
};
static const campaign_page_t campaign_carrying_37 = {{"南库露出，北库还在水下。","再来拨动，可交换两座库房。"},{"South is dry; north is underwater.","Turn again to swap the two vaults."}};
static const campaign_page_t campaign_pages_38[] = {
    {{"找到了听水用的铜耳朵。","带回十一层，老人还在等。"},{"You found the copper ear.","Return it to the old man on 11."}},
};
static const campaign_page_t campaign_pages_39[] = {
    {{"记下了账房的炉火星记。","水纹、炉火、王冠缺一不可。"},{"You record the furnace mark.","Water, furnace and crown: all three."}},
};
static const campaign_page_t campaign_pages_40[] = {
    {{"找到了一块带焦味的影子。","二十一层的炉匠会认识它。"},{"You find a shadow smelling of ash.","The smith on 21 will recognise it."}},
};
static const campaign_page_t campaign_pages_41[] = {
    {{"记下了禁书室的王冠星记。","星记齐全后，去四十七层。"},{"You record the crown mark.","Take all three to floor 47."}},
};
static const campaign_page_t campaign_pages_42[] = {
    {{"这块布像月亮背面的夜色。","带回三十三层，交给裁缝。"},{"This patch feels like the moon.","Bring it to the tailor on 33."}},
};
static const campaign_dialogue_t campaign_dialogues[] = {
    {2,3,9,0,1,-1,"旧库管事","ARCHIVIST",campaign_pages_0,NULL,NULL},
    {3,1,2,0,1,-1,"哨兵","SENTRY",campaign_pages_1,NULL,NULL},
    {5,5,7,0,1,-1,"军需官","QUARTERMASTER",campaign_pages_2,NULL,NULL},
    {7,3,6,0,2,-1,"伤兵","WOUNDED GUARD",campaign_pages_3,NULL,NULL},
    {9,8,8,0,1,-1,"名册书记","ROLL KEEPER",campaign_pages_4,NULL,NULL},
    {10,1,9,0,1,0,"听水老人","WATER LISTENER",campaign_pages_5,&campaign_carrying_5,&campaign_done_5},
    {11,5,3,0,1,-1,"泵房学徒","PUMP KEEPER",campaign_pages_6,NULL,NULL},
    {13,3,3,0,2,-1,"阀厅学徒","VALVE KEEPER",campaign_pages_7,NULL,NULL},
    {14,3,11,0,1,-1,"淤泥中的字迹","SILT NOTE",campaign_pages_8,NULL,NULL},
    {16,3,7,0,1,-1,"旧宿舍看守","DORM KEEPER",campaign_pages_9,NULL,NULL},
    {17,3,5,0,1,-1,"听水人的纸条","WATER NOTE",campaign_pages_10,NULL,NULL},
    {18,3,1,0,1,-1,"渡守遗言","FERRY NOTE",campaign_pages_11,NULL,NULL},
    {19,5,9,0,1,-1,"摆渡人","FERRYMAN",campaign_pages_12,NULL,NULL},
    {20,3,10,0,1,1,"无影炉匠","SHADOWLESS SMITH",campaign_pages_13,&campaign_carrying_13,&campaign_done_13},
    {23,5,3,0,1,-1,"精铸笔记","CASTING NOTE",campaign_pages_14,NULL,NULL},
    {24,5,7,0,1,-1,"炉边商人","FORGE MERCHANT",campaign_pages_15,NULL,NULL},
    {26,5,5,0,1,-1,"账房","BOOKKEEPER",campaign_pages_16,NULL,NULL},
    {27,3,1,0,1,-1,"封炉告示","FURNACE NOTICE",campaign_pages_17,NULL,NULL},
    {28,1,5,0,1,-1,"最后一页账","LAST LEDGER",campaign_pages_18,NULL,NULL},
    {29,7,3,0,1,-1,"炉心回声","HEART ECHO",campaign_pages_19,NULL,NULL},
    {30,8,9,0,1,-1,"王城门吏","COURT USHER",campaign_pages_20,NULL,NULL},
    {32,9,5,0,1,2,"补月裁缝","MOON MENDER",campaign_pages_21,&campaign_carrying_21,&campaign_done_21},
    {34,11,7,0,2,-1,"禁书抄写员","SCRIBE",campaign_pages_22,NULL,NULL},
    {36,5,4,0,1,-1,"床头遗书","BEDSIDE NOTE",campaign_pages_23,NULL,NULL},
    {38,3,5,0,1,-1,"守誓书","OATH",campaign_pages_24,NULL,NULL},
    {39,11,3,0,1,-1,"公主留声","HER MESSAGE",campaign_pages_25,NULL,NULL},
    {40,1,11,0,1,-1,"封印碑","SEAL PLAQUE",campaign_pages_26,NULL,NULL},
    {41,7,7,0,1,-1,"观测日志","OBSERVATORY LOG",campaign_pages_27,NULL,NULL},
    {42,13,10,0,1,-1,"镜中的裁缝","MIRROR TAILOR",campaign_pages_28,NULL,NULL},
    {44,5,9,0,1,-1,"最后的商人","LAST MERCHANT",campaign_pages_29,NULL,NULL},
    {45,13,5,0,1,-1,"回收清单","RECOVERY LIST",campaign_pages_30,NULL,NULL},
    {46,1,5,0,1,-1,"归还之碑","RETURN STONE",campaign_pages_31,NULL,NULL},
    {46,12,14,0,3,-1,"gpt6.1sol","gpt6.1sol",campaign_pages_32,NULL,NULL},
    {48,11,2,0,1,-1,"最后的封印","LAST SEAL",campaign_pages_33,NULL,NULL},
    {49,5,7,0,1,-1,"王座前的声音","VOICE AT THE THRONE",campaign_pages_34,NULL,NULL},
    {49,6,1,0,2,-1,"公主","PRINCESS",campaign_pages_35,NULL,NULL},
    {11,4,3,1,1,-1,"水纹星记","WATER MARK",campaign_pages_36,NULL,NULL},
    {13,3,4,1,1,-1,"水闸","SLUICE",campaign_pages_37,&campaign_carrying_37,NULL},
    {17,7,7,1,1,-1,"铜耳朵","COPPER EAR",campaign_pages_38,NULL,NULL},
    {26,5,6,1,1,-1,"炉火星记","FURNACE MARK",campaign_pages_39,NULL,NULL},
    {33,3,5,1,1,-1,"炉子的影子","FURNACE SHADOW",campaign_pages_40,NULL,NULL},
    {34,11,9,1,1,-1,"王冠星记","CROWN MARK",campaign_pages_41,NULL,NULL},
    {42,13,11,1,1,-1,"月亮的补丁","MOON PATCH",campaign_pages_42,NULL,NULL},
};
#define CAMPAIGN_DIALOGUE_COUNT (sizeof(campaign_dialogues)/sizeof(campaign_dialogues[0]))
static inline uint16_t campaign_dialogue_at(unsigned floor,unsigned x,unsigned y) {
    for(unsigned i=0;i<CAMPAIGN_DIALOGUE_COUNT;++i) {
        const campaign_dialogue_t *d=&campaign_dialogues[i];
        if(d->floor==floor && d->x==x && d->y==y) return (uint16_t)i;
    }
    return CAMPAIGN_DIALOGUE_NONE;
}
static inline unsigned campaign_dialogue_pages(uint16_t id,unsigned variant) {
    if(id>=CAMPAIGN_DIALOGUE_COUNT) return 0;
    const campaign_dialogue_t *d=&campaign_dialogues[id];
    return (variant==1 && d->carrying) || (variant==2 && d->done)?1u:d->page_count;
}
static inline const campaign_page_t *campaign_dialogue_page(uint16_t id,unsigned variant,unsigned page) {
    if(page>=campaign_dialogue_pages(id,variant)) return NULL;
    const campaign_dialogue_t *d=&campaign_dialogues[id];
    if(variant==1 && d->carrying) return d->carrying;
    if(variant==2 && d->done) return d->done;
    return &d->pages[page];
}
