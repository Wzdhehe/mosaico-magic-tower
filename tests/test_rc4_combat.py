"""Regression for king stat ink crossing adjacent columns; real C RGB565."""
import ctypes as C
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import test_campaign_dialog_layout as qa
from capture_campaign_design import rgb565_image

qa.OUT=ROOT/'artifacts/rc4-combat'
qa.FACADE+=r'''
int combat_bounds(const char *label,unsigned value,int center) {
 BeginDrawing();
 ClearBackground(BLACK);
 king_combat_stat(label,value,center);
 EndDrawing();
 unsigned ink=0;
 for(int y=0;y<480;++y)for(int x=0;x<480;++x)
  if(dialog_pixels[y*480+x]) {++ink;if(x<center-36 || x>center+36 || y<211 || y>=249)return x+1;}
 return ink?0:-1;
}
void combat_capture(unsigned lang,unsigned tile,uint16_t *out) {
 memset(&dialog_snapshot,0,sizeof(dialog_snapshot));
 tower_read(dialog_model,&dialog_snapshot.tower);
 dialog_snapshot.screen=GAME_DUNGEON;dialog_snapshot.lang=(uint8_t)lang;
 dialog_snapshot.tower.phase=TOWER_PHASE_COMBAT;
 dialog_snapshot.tower.pending=(tower_enemy_preview_t){.tile=(tower_tile_t)tile,
   .hp=1200,.attack=170,.defence=110,.damage=1234};
 dialog_snapshot.floor_w=dialog_snapshot.floor_h=11;
 dialog_snapshot.tower.floor=49;dialog_snapshot.tower.x=5;dialog_snapshot.tower.y=4;
 dialog_dirty=true;(void)game_render((game_handle_t)&dialog_snapshot);
 memcpy(out,dialog_pixels,sizeof(dialog_pixels));
}
int combat_legacy_unchanged(void) {
 static uint16_t reference[480*480];
 const char *labels[]={"HP","ATK","DEF"};
 const unsigned values[]={560,151,95};
 for(unsigned i=0;i<3;++i) {
  const int center=165+(int)i*75;
  BeginDrawing();ClearBackground(BLACK);combat_stat(labels[i],values[i],center);EndDrawing();
  memcpy(reference,dialog_pixels,sizeof(reference));
  char text[24];snprintf(text,sizeof(text),"%s %u",labels[i],values[i]);
  BeginDrawing();ClearBackground(BLACK);
  MosaicoFastDrawText(text,center-MosaicoFastMeasureText(text,16)/2,207,16,WHITE_INK);
  EndDrawing();
  if(memcmp(reference,dialog_pixels,sizeof(reference)))return 1+(int)i;
 }
 return 0;
}
'''

def main():
    lib,_=qa.compile_dialog()
    assert lib.dialog_init()==0
    lib.combat_bounds.argtypes=[C.c_char_p,C.c_uint,C.c_int]
    lib.combat_capture.argtypes=[C.c_uint,C.c_uint,C.POINTER(C.c_uint16)]
    try:
        assert lib.combat_legacy_unchanged()==0,'Normal monster stat rendering changed'
        for label,value,cx in [(b'HP',1200,165),(b'ATK',170,240),(b'DEF',110,315),
                               (b'HP',99999,165),(b'ATK',99999,240),(b'DEF',99999,315)]:
            overflow=lib.combat_bounds(label,value,cx)
            assert overflow==0,('stat ink crosses its column',label,value,'x',overflow-1)
        buf=(C.c_uint16*(480*480))()
        for lang in range(2):
            for tile in range(12,20):
                lib.combat_capture(lang,tile,buf)
                picture=rgb565_image(buf)
                picture.save(qa.OUT/f'enemy-{tile}-combat-{lang}.png')
                if tile==19:picture.save(qa.OUT/f'king-combat-{lang}.png')
        print('PASS: king stat ink stays below name/in three columns; normal stat pixels unchanged; all eight enemies in both languages')
    finally:lib.dialog_close()
if __name__=='__main__':main()
