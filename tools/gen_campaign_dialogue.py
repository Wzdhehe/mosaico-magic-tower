"""Encode approved authored copy as a compact, coordinate-dispatched C table."""
import argparse
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assets_src/level_design'))
from campaign_content import data

EVENTS=[
 (12,4,3,'水纹星记','WATER MARK',['记下了泵房的水纹。','四十七层旧室还在等待。'],['You record the water mark.','An old room waits on floor 47.']),
 (14,3,4,'水闸','SLUICE',['北库露出，南库还在水下。','再来拨动，可交换两座库房。'],['North is dry; south is underwater.','Turn again to swap the two vaults.']),
 (18,7,7,'铜耳朵','COPPER EAR',['找到了听水用的铜耳朵。','带回十一层，老人还在等。'],['You found the copper ear.','Return it to the old man on 11.']),
 (27,5,6,'炉火星记','FURNACE MARK',['记下了账房的炉火星记。','水纹、炉火、王冠缺一不可。'],['You record the furnace mark.','Water, furnace and crown: all three.']),
 (34,3,5,'炉子的影子','FURNACE SHADOW',['找到了一块带焦味的影子。','二十一层的炉匠会认识它。'],['You find a shadow smelling of ash.','The smith on 21 will recognise it.']),
 (35,11,9,'王冠星记','CROWN MARK',['记下了禁书室的王冠星记。','星记齐全后，去四十七层。'],['You record the crown mark.','Take all three to floor 47.']),
 (43,13,11,'月亮的补丁','MOON PATCH',['这块布像月亮背面的夜色。','带回三十三层，交给裁缝。'],['This patch feels like the moon.','Bring it to the tailor on 33.']),
]
def render():
    records=[]
    for n in data()['npcs']:
        records.append(dict(n,kind=0))
    for f,x,y,zh,en,lines,english in EVENTS:
        n=dict(floor=f,position=[x,y],name_zh=zh,name_en=en,kind=1,pages=[dict(zh=lines,en=english)])
        if f==14:n['carrying']=dict(zh=['南库露出，北库还在水下。','再来拨动，可交换两座库房。'],en=['South is dry; north is underwater.','Turn again to swap the two vaults.'])
        records.append(n)
    quote=lambda s:json.dumps(s,ensure_ascii=False)
    page=lambda p:'{{'+','.join(quote(t) for t in p['zh'])+'},{'+','.join(quote(t) for t in p['en'])+'}}'
    lines=['/* Generated from approved campaign_content.py. Do not improvise copy. */','#pragma once','#include <stdint.h>','#include <stddef.h>',
           'typedef struct { const char *zh[2], *en[2]; } campaign_page_t;',
           'typedef struct { uint8_t floor,x,y,kind,page_count; int8_t quest; const char *name_zh,*name_en; const campaign_page_t *pages,*carrying,*done; } campaign_dialogue_t;',
           '#define CAMPAIGN_DIALOGUE_NONE UINT16_MAX','']
    for i,n in enumerate(records):
        pages=n['pages']+n.get('extra_pages',[])
        lines += [f'static const campaign_page_t campaign_pages_{i}[] = {{',*['    '+page(p)+',' for p in pages],'};']
        for v in ('carrying','done'):
            if v in n:lines.append(f'static const campaign_page_t campaign_{v}_{i} = '+page(n[v])+';')
    lines+=['static const campaign_dialogue_t campaign_dialogues[] = {']
    for i,n in enumerate(records):
        pages=n['pages']+n.get('extra_pages',[])
        fields=[str(n['floor']-1),*map(str,n['position']),str(n['kind']),str(len(pages)),str(n.get('quest',-1)),quote(n['name_zh']),quote(n['name_en']),f'campaign_pages_{i}',f'&campaign_carrying_{i}' if 'carrying' in n else'NULL',f'&campaign_done_{i}' if 'done' in n else'NULL']
        lines.append('    {'+','.join(fields)+'},')
    lines+=['};','#define CAMPAIGN_DIALOGUE_COUNT (sizeof(campaign_dialogues)/sizeof(campaign_dialogues[0]))',
'''static inline uint16_t campaign_dialogue_at(unsigned floor,unsigned x,unsigned y) {
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
''']
    return '\n'.join(lines)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');a=parser.parse_args()
    target=ROOT/'main/campaign_dialogue.h';source=render()
    if a.check:assert target.read_text(encoding='utf-8')==source,'Generated dialogue table is stale'
    else:target.write_text(source,encoding='utf-8')
    print('PASS: approved coordinate-based NPC/event dialogue table')
if __name__=='__main__':main()
