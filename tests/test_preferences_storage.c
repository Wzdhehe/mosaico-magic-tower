// SPDX-License-Identifier: MIT
#include "game_preferences.h"
#include "host_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    assert(argc==3);
    const game_preferences_t chosen={GAME_LANG_EN,52,61};
    const uint8_t expected[GAME_PREFERENCES_BYTES]={1,1,52,61};
    uint8_t wire[GAME_PREFERENCES_BYTES],other[GAME_PREFERENCES_BYTES];
    game_preferences_t restored={0};
    assert(game_preferences_encode(&chosen,wire));
    assert(!memcmp(wire,expected,sizeof wire));
    assert(game_host_save(argv[1],wire,sizeof wire));
    assert(game_host_load(argv[1],other,sizeof other));
    assert(game_preferences_decode(other,&restored));
    assert(restored.lang==1 && restored.volume==52 && restored.brightness==61);
    for(unsigned field=0;field<GAME_PREFERENCES_BYTES;++field) {
        memcpy(other,wire,sizeof wire); other[field]=255;
        const game_preferences_t before=restored;
        assert(!game_preferences_decode(other,&restored));
        assert(!memcmp(&before,&restored,sizeof before));
    }
    const game_preferences_t invalid={0,101,61};
    memset(other,42,sizeof other);
    assert(!game_preferences_encode(&invalid,other));
    for(unsigned i=0;i<sizeof other;++i) assert(other[i]==42);
    assert(!game_preferences_encode(NULL,wire));
    assert(!game_preferences_encode(&chosen,NULL));
    assert(!game_preferences_decode(NULL,&restored));
    assert(!game_preferences_decode(wire,NULL));
    /* Runner pre-seeds this preference and blocks its .tmp staging path. */
    assert(!game_host_save(argv[2],wire,sizeof wire));
    assert(game_host_load(argv[2],other,sizeof other));
    const uint8_t previous[GAME_PREFERENCES_BYTES]={1,0,45,40};
    assert(!memcmp(other,previous,sizeof other));
    FILE *file=fopen(argv[1],"wb"); assert(file);
    assert(fwrite(wire,1,3,file)==3); assert(fclose(file)==0);
    assert(!game_host_load(argv[1],other,sizeof other));
    puts("PASS: validated preferences, transactional replacement, invalid/truncated data and failed staging");
    return 0;
}
