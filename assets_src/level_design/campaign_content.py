"""Designer-owned dialogue, identities, hidden-room presentation and score motifs."""
def page(zh,en):return dict(zh=zh,en=en)
NPCS=[]
def npc(floor,position,name,en,zh,english,**extra):
    NPCS.append(dict(floor=floor,position=list(position),name_zh=name,name_en=en,
                     pages=[page(zh,english)],**extra))

npc(3,(3,9),'旧库管事','ARCHIVIST',['四层北端有蓝钥匙。','九层红封门的钥匙在这里。'],['A blue key waits on floor four.','The ninth seal needs our red key.'])
npc(4,(1,2),'哨兵','SENTRY',['这把蓝钥匙先开哪一库？','旧库给盾，军械库给剑。'],['Which vault gets the first blue key?','Archive: defence. Armory: attack.'])
npc(6,(5,7),'军需官','QUARTERMASTER',['别只看能不能打赢。','升级以后，能少挨几轮？'],['Do more than ask if you can win.','How many hits will an upgrade save?'])
npc(8,(3,6),'伤兵','WOUNDED GUARD',['左脚边有块较暗的墙砖。','走进去，房间才会显现。'],['A darker wall lies to your left.','Step in to reveal the hidden room.'],
    extra_pages=[page(['有些暗墙藏在别的地方。','找到以后，离开也不会消失。'],['More dark walls wait elsewhere.','Revealed rooms stay discovered.'])])
npc(10,(8,8),'名册书记','ROLL KEEPER',['名册停在城破那一天。','队长身后还有很长的阶梯。'],['The roll ends when the gate fell.','A long stair lies past the captain.'])
npc(11,(1,9),'听水老人','WATER LISTENER',['我的铜耳朵落在回流渠了。','十八层旧泵旁，水声最怪。'],['I lost my copper ear in the channel.','The old pump on 18 sounds strange.'],quest=0,
    carrying=page(['你真的听见它了！','我把旧堤入口的淤泥清掉。'],['You really heard it calling!','I will clear the old levee entrance.']),
    done=page(['沿内岸向东找暗墙。','铜耳朵说，谢谢。'],['Find a dark wall east of the bank.','The copper ear says thank you.']))
npc(12,(5,3),'泵房学徒','PUMP KEEPER',['北刻线是盾，南刻线是剑。','十四层阀门能反复调整。'],['North mark: shield. South: sword.','The valve on 14 can be turned back.'])
npc(14,(3,3),'阀厅学徒','VALVE KEEPER',['北库先露出，南库还在水下。','拨动脚边阀门，两库会交换。'],['North is dry; south is underwater.','The valve swaps the two vaults.'],extra_pages=[page(['十五层外岸始终可以通过。','拿完一边，可回来再换。'],['The outer bank always stays open.','Return and turn it for the other.'])])
npc(15,(3,11),'淤泥中的字迹','SILT NOTE',['我们把塔当成了牢房。','可公主是在里面守住什么？'],['We thought the tower was a prison.','What has the princess been holding?'])
npc(17,(3,7),'旧宿舍看守','DORM KEEPER',['东侧骑士身后有红钥匙。','先看看代价，也能回来再拿。'],['The east knight guards a red key.','Check the cost. You can return.'])
npc(18,(3,5),'听水人的纸条','WATER NOTE',['右边那块墙砖没有积水。','铜耳朵，大概还躺在里面。'],['The wall to the right stayed dry.','The copper ear may still be inside.'])
npc(19,(3,1),'渡守遗言','FERRY NOTE',['渡守只拦住向上的人。','他说，水下还有没回家的名字。'],['The ferryman stops only climbers.','Below lie names that never went home.'])
npc(20,(5,9),'摆渡人','FERRYMAN',['石印亮后，这里就能传送。','回来时，不必再交一次代价。'],['The lit seal becomes a warp anchor.','Your return will not cost a fight.'])
npc(21,(3,10),'无影炉匠','SHADOWLESS SMITH',['炉子还有火，却丢了影子。','三十四层有人拿它当旗子。'],['My furnace lost its own shadow.','Someone flies it as a flag on 34.'],quest=1,
    carrying=page(['是它，还是一股焦味。','西边旧库，我替你揭开入口。'],['That is it. Still smells of ash.','I will uncover the west store.']),
    done=page(['影子总算肯跟着火走了。','旧库的暗墙就在我北边。'],['The shadow follows the flame again.','The dark wall lies north of me.']))
npc(24,(5,3),'精铸笔记','CASTING NOTE',['有时只差一点攻击。','多敲一轮，就多挨一次反击。'],['One more point can change a fight.','One extra round means one more hit.'])
npc(25,(5,7),'炉边商人','FORGE MERCHANT',['所有商店共用一份账。','先买什么，要看下一道守卫。'],['Every shop shares the same ledger.','Buy for the guard you face next.'])
npc(27,(5,5),'账房','BOOKKEEPER',['东炉守卫身后有块暗墙。','炉中星记，记得带上。'],['A dark wall waits behind the guard.','Take the furnace mark with you.'])
npc(28,(3,1),'封炉告示','FURNACE NOTICE',['封的是余烬，不是宝石。','账房知道旧仓的入口。'],['The seal holds embers, not jewels.','The ledger names the hidden mouth.'])
npc(29,(1,5),'最后一页账','LAST LEDGER',['钥匙用在门上。','金币要在还用得着时花掉。'],['Keys are meant to open doors.','Spend gold while it can still help.'])
npc(30,(7,3),'炉心回声','HEART ECHO',['公主借走了炉心的光。','她说，要让塔再撑一夜。'],['The princess borrowed its light.','She needed the tower to last a night.'])
npc(31,(8,9),'王城门吏','COURT USHER',['两翼守卫，习惯各不相同。','也有人一直替王城交钥匙。'],['The two wings fight differently.','Some visitors pay the key toll.'])
npc(33,(9,5),'补月裁缝','MOON MENDER',['月亮背面破了一个洞。','四十三层镜子里有那块补丁。'],['The back of the moon has a hole.','Its patch is in the mirrors on 43.'],quest=2,
    carrying=page(['这块补丁，摸起来像夜色。','祭坛东南的旧室就送给你。'],['This patch feels like night itself.','The southeast room is yours.']),
    done=page(['以后满月就不会漏风了。','暗墙在祭坛下面，向东走。'],['Full moons will no longer leak air.','The dark wall is below the altar, east.']))
npc(35,(11,7),'禁书抄写员','SCRIBE',['水纹、炉火、王冠。','三枚星记开四十七层旧室。'],['Water, furnace, crown. Three marks.','They open the old room on 47.'],extra_pages=[page(['水纹在十二层泵房。','炉火在二十七层账房。'],['Water: the pump room on 12.','Furnace: the ledger room on 27.'])])
npc(37,(5,4),'床头遗书','BEDSIDE NOTE',['王冠缺口总朝南。','床脚正中的墙砖颜色较暗。'],['The broken crown points south.','A darker wall lies below the bed.'])
npc(39,(3,5),'守誓书','OATH',['守誓者没有背叛公主。','他只是不肯让封印被误开。'],['The oathkeeper never betrayed her.','He will not risk opening the seal.'])
npc(40,(11,3),'公主留声','HER MESSAGE',['如果你听见这里，封印快尽了。','别救一个影子，来救整座城。'],['If you hear this, the seal is weak.','Come save the city, not a shadow.'])
npc(41,(1,11),'封印碑','SEAL PLAQUE',['高处卫兵的盔甲难以击穿。','先看伤害，别向空血量许愿。'],['The seal guard wears heavy armour.','Read the damage before you strike.'])
npc(42,(7,7),'观测日志','OBSERVATORY LOG',['水纹不是钥匙，是归路。','星记没拿齐，也能抵达王座。'],['The water mark remembers a way home.','You need no marks to reach the king.'])
npc(43,(13,10),'镜中的裁缝','MIRROR TAILOR',['镜子照不见月亮的背面。','但脚边那块布，总在发光。'],['No mirror sees the back of the moon.','That patch by my feet keeps glowing.'])
npc(45,(5,9),'最后的商人','LAST MERCHANT',['王座上的影子不会追你下楼。','四十四层蓝库还能换大药。'],['The king will not chase you below.','The blue vault on 44 holds medicine.'])
npc(46,(13,5),'回收清单','RECOVERY LIST',['旧营、东炉、旧王寝。','还有那些一直等你回去的人。'],['Old camp. East furnace. Royal bed.','And everyone waiting for your return.'])
npc(47,(1,5),'归还之碑','RETURN STONE',['水纹、炉火、王冠都记下了吗？','此室不挡通往王座的道路。'],['Have you kept water, fire and crown?','This room never bars the throne.'])
npc(47,(12,14),'gpt6.1sol','gpt6.1sol',['我是这个游戏的真正开发者，','gpt6.1sol。'],['I am the true developer of this game,','gpt6.1sol.'],easter_egg=True,
    extra_pages=[page(['这座塔的代码，','都是我一行一行写出来的。'],['The code of this tower:','I wrote it, one line at a time.']),page(['不过隐藏房间里的宝物，','你得自己找。'],['The hidden treasures, though?','You will have to find those yourself.'])])
npc(49,(11,2),'最后的封印','LAST SEAL',['再向上，就是王座。','传送回去准备，门不会重关。'],['The next stair leads to the throne.','Return to prepare. Doors stay open.'])
npc(50,(5,7),'王座前的声音','VOICE AT THE THRONE',['我在魔王身后维持最后的封印。','看清伤害，不够就再回去准备。'],['I hold the last seal behind the king.','Check the cost. Return if needed.'])

REGIONS=[
 dict(name_zh='断门城塞',name_en='BROKEN GATE',style='SCENE_GATE',base='warm grey stone',accent='#d7bd8d',music='gate'),
 dict(name_zh='沉水回廊',name_en='SUNKEN HALL',style='SCENE_CISTERN',base='desaturated teal masonry',accent='#83c9ce',music='water'),
 dict(name_zh='灰烬工坊',name_en='ASH WORKS',style='SCENE_ARMORY',base='charcoal and copper',accent='#dfaa73',music='forge'),
 dict(name_zh='静默王城',name_en='SILENT COURT',style='SCENE_ARCHIVE',base='cool ivory and faded violet',accent='#c5b6dc',music='court'),
 dict(name_zh='星冠高塔',name_en='STAR CROWN',style='SCENE_ASTRAL',base='deep indigo and old gold',accent='#dfc28b',music='crown'),
]
# Pitch names plus rhythmic durations in sixteenth notes. Each phrase is two
# bars in 4/4; R is silence. DS may encode the score, never improvise a new tune.
# Four integer voices maximum; no sample streaming or time-driven UI redraw.
SCORES={
 'gate':dict(bpm=84,key='D minor',lead='D5:4 A4:2 R:2 F5:4 E5:4 D5:6 C5:2 A4:4 R:4',bass='D3:8 R:8 C3:8 A2:8',chords=['Dm','Bb','C','Am'],wave='soft triangle'),
 'water':dict(bpm=72,key='D dorian',lead='A4:2 R:2 D5:4 E5:2 R:2 F5:4 E5:4 D5:4 B4:4 R:4',bass='D3:8 R:8 G2:8 A2:8',chords=['Dm','G','C','Dm'],wave='short triangle pluck'),
 'forge':dict(bpm=96,key='A minor',lead='E5:3 C5:1 R:2 A4:2 C5:4 E5:4 G5:3 E5:1 D5:4 C5:4 R:4',bass='A2:6 E3:2 A2:8 F2:6 C3:2 E2:8',chords=['Am','F','Dm','E'],wave='soft pulse 25 percent'),
 'court':dict(bpm=68,key='F major with borrowed minor',lead='A4:6 G4:2 F4:8 C5:6 A4:2 G4:4 R:4',bass='F2:16 Bb2:8 C3:8',chords=['F','Bb','Bbm','C'],wave='triangle and quiet sustained fifths'),
 'crown':dict(bpm=88,key='D minor',lead='D5:4 A4:2 C5:2 F5:4 E5:4 D5:2 E5:2 F5:4 A5:4 R:4',bass='D3:8 A2:8 Bb2:8 C3:8',chords=['Dm','Bb','Gm','A'],wave='triangle lead and soft pulse echo'),
 'king':dict(bpm=104,key='D harmonic minor',lead='D5:2 E5:2 F5:4 A5:2 G5:2 E5:4 F5:2 E5:2 D5:4 Cs5:4 R:4',bass='D3:4 A2:4 D3:8 Bb2:8 A2:8',chords=['Dm','Bb','Gm','A'],wave='restrained pulse, sparse noise'),
 'home':dict(bpm=72,key='D major',lead='D5:4 A4:2 R:2 Fs5:4 E5:4 D5:6 E5:2 Fs5:4 R:4',bass='D3:16 G2:8 A2:8',chords=['D','G','Bm','A'],wave='triangle, no percussion'),
}
# B is an authored two-bar answer, never a transposition invented by a generator.
for key,answer in {
 'gate':'F5:4 G5:2 A5:2 G5:4 E5:4 F5:6 E5:2 D5:4 R:4',
 'water':'G5:2 R:2 E5:4 D5:2 R:2 B4:4 C5:4 E5:4 D5:4 R:4',
 'forge':'A5:3 G5:1 E5:4 C5:2 R:2 D5:4 E5:3 C5:1 B4:4 A4:4 R:4',
 'court':'C5:6 D5:2 A4:8 Bb4:6 A4:2 G4:4 R:4',
 'crown':'F5:4 G5:2 A5:2 C6:4 A5:4 G5:2 E5:2 D5:4 Cs5:4 R:4',
 'king':'A5:2 Bb5:2 A5:4 F5:2 E5:2 D5:4 E5:2 F5:2 E5:4 Cs5:4 R:4',
 'home':'Fs5:4 G5:2 A5:2 B5:4 A5:4 G5:6 Fs5:2 E5:4 R:4',
}.items():
    SCORES[key]['answer']=answer
    SCORES[key]['form']=['lead','lead','answer','lead']
    SCORES[key]['chord_span_sixteenths']=32

def data():
    return dict(revision='tower50-authored-v8',coordinate_system='zero based x/y; human floor numbers',
                author='Ponkan',npcs=NPCS,regions=REGIONS,scores=SCORES,
                ending=dict(princess=[6,1],beacon=[5,1],floor=50,
                            zh=['封印散去，公主终于离开王座。','那些没有回家的名字，有了归路。'],
                            en=['The seal fades. She leaves the throne.','The lost names have a road home.']))
