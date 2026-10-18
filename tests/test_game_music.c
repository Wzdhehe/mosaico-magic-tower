// SPDX-License-Identifier: MIT
#include "game_music.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static game_music_handle_t a, b;
static int fail_allocation;
void *game_music_test_calloc(size_t count, size_t size) {
    if (fail_allocation) return NULL;
    void *p=malloc(count*size); if(p) memset(p,0,count*size); return p;
}
static int16_t output[24000], other[24000];
static unsigned hashes[3];
static unsigned hash(unsigned h, const int16_t *p, size_t n) {
    for (size_t i=0;i<n;++i) h=(h^(unsigned)(uint16_t)p[i])*16777619U;
    return h;
}
int main(void) {
    game_music_delete(NULL);
    CHECK(!game_music_create(NULL,NULL));
    fail_allocation=1; CHECK(!game_music_create(NULL,&a)); CHECK(a==NULL); fail_allocation=0;
    CHECK(!game_music_render(NULL,output,1));
    CHECK(!game_music_set_track(NULL,0));
    game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a);
    CHECK(!game_music_render(a,NULL,1));
    CHECK(game_music_render(a,NULL,0));
    CHECK(!game_music_set_volume(a,101));
    CHECK(!game_music_set_track(a,3));
    CHECK(game_music_loop_samples(3)==0);
    CHECK(game_music_track_name(3)==NULL);
    CHECK(game_music_position(NULL)==0);
    game_music_config_t bad_config={1};
    game_music_handle_t failed=NULL;
    CHECK(!game_music_create(&bad_config,&failed)); CHECK(!failed);
    puts("PASS: invalid handles/arguments and allocation failure and initialization");
    game_music_delete(b); CHECK(game_music_create(NULL,&b)); CHECK(b);
    CHECK(game_music_render(a,output,24000));
    for(size_t offset=0;offset<24000;) {
        size_t n=(offset%317)+1;
        if(n>24000-offset) n=24000-offset;
        CHECK(game_music_render(b,other+offset,n)); offset+=n;
    }
    CHECK(memcmp(output,other,sizeof(output))==0);
    CHECK(game_music_position(a)==24000);
    puts("PASS: independent instances, determinism and arbitrary chunk boundaries");
    /* Changing/deleting one live instance cannot change another instance's PCM. */
    CHECK(game_music_set_track(a,2));
    CHECK(game_music_set_volume(a,0));
    CHECK(game_music_render(b,other,24000));
    game_music_delete(a); CHECK(game_music_create(NULL,&a));
    CHECK(game_music_render(a,output,24000));
    CHECK(game_music_render(a,output,24000));
    CHECK(memcmp(output,other,sizeof(output))==0);
    puts("PASS: one instance's control changes/deletion do not affect another");
    CHECK(game_music_set_volume(a,0));
    CHECK(game_music_render(a,output,24000));
    for(size_t i=0;i<24000;++i) CHECK(output[i]==0);
    CHECK(game_music_position(a)==72000);
    CHECK(game_music_set_volume(a,45));
    CHECK(game_music_render(a,output,24000));
    unsigned resumed=hash(0,output,24000);
    CHECK(resumed!=0);
    puts("PASS: immediate exact silence, advancing muted timeline, unmute");
    game_music_delete(a); CHECK(game_music_create(NULL,&a));
    game_music_delete(b); CHECK(game_music_create(NULL,&b));
    CHECK(game_music_set_volume(a,0)); CHECK(game_music_set_volume(b,100));
    CHECK(game_music_render(a,output,24000)); CHECK(game_music_render(b,other,24000));
    CHECK(game_music_set_volume(a,100));
    CHECK(game_music_render(a,output,24000)); CHECK(game_music_render(b,other,24000));
    CHECK(memcmp(output+4096,other+4096,(24000-4096)*sizeof(*output))==0);
    puts("PASS: muted phase/score timeline matches audible rendering after gain/echo settle");
    game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a);
    CHECK(game_music_set_volume(a,100));
    clock_t start=clock();
    for(unsigned track=0;track<3;++track) {
        game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a);
        CHECK(game_music_set_volume(a,0));
        CHECK(game_music_set_track(a,track));
        CHECK(game_music_set_volume(a,100));
        uint32_t frames=game_music_loop_samples(track);
        CHECK(frames>=35U*24000U && frames<=45U*24000U);
        long long sum=0,energy=0; int peak=0,max_jump=0,last=0;
        hashes[track]=2166136261U;
        for(uint32_t offset=0;offset<frames;) {
            size_t n=frames-offset>24000?24000:frames-offset;
            CHECK(game_music_render(a,output,n));
            for(size_t i=0;i<n;++i) {
                int v=output[i]; int magnitude=v<0?-v:v;
                int jump=v-last; if(jump<0) jump=-jump;
                if(jump>max_jump) max_jump=jump;
                if(magnitude>peak) peak=magnitude;
                sum+=v; energy+=(long long)v*v; last=v;
                CHECK(v>-30000 && v<30000);
            }
            hashes[track]=hash(hashes[track],output,n); offset+=(uint32_t)n;
        }
        CHECK(peak>1000); CHECK(llabs(sum)<(long long)frames*40);
        CHECK(energy>(long long)frames*10000);
        /* The makeup gain scales peak and slew together; keep the relative
         * bound so a click would still be caught. */
        CHECK(max_jump < peak/3 + 200);
        CHECK(game_music_position(a)==0);
        printf("PASS: track %u %s %.3fs peak=%d mean=%.3f mean_square=%.0f max_jump=%d hash=%08x\n",
            track,game_music_track_name(track),(double)frames/24000,peak,(double)sum/frames,
            (double)energy/frames,max_jump,hashes[track]);
        /* The loop seam has a fresh note envelope and continuing bounded echo. */
        CHECK(game_music_render(a,output,1));
        int seam=output[0]-last; if(seam<0) seam=-seam; CHECK(seam < peak/3 + 200);
    }
    CHECK(hashes[0]!=hashes[1] && hashes[1]!=hashes[2] && hashes[0]!=hashes[2]);
    printf("PASS: distinct track PCM; context=%zu bytes; host full-volume render CPU=%.3fs\n",
           game_music_memory_bytes(),(double)(clock()-start)/CLOCKS_PER_SEC);
    game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a); game_music_delete(b); CHECK(game_music_create(NULL,&b)); CHECK(b);
    CHECK(game_music_render(a,output,24000)); CHECK(game_music_render(b,other,24000));
    CHECK(game_music_set_track(a,2)); CHECK(game_music_set_track(b,2));
    CHECK(game_music_set_volume(a,10)); CHECK(game_music_set_volume(b,10));
    CHECK(game_music_render(a,output,24000));
    for(size_t i=0;i<24000;++i) CHECK(game_music_render(b,other+i,1));
    CHECK(memcmp(output,other,sizeof(output))==0);
    int previous=other[23999];
    CHECK(game_music_set_volume(a,100));
    CHECK(game_music_set_track(a,1));
    CHECK(game_music_render(a,output,24000));
    int jump=output[0]-previous; if(jump<0) jump=-jump; CHECK(jump<2000);
    for(size_t i=1;i<24000;++i) {
        jump=output[i]-output[i-1]; if(jump<0) jump=-jump; CHECK(jump<2000);
    }
    clock_t mute_start=clock();
    CHECK(game_music_set_volume(a,0));
    for(unsigned i=0;i<120;++i) CHECK(game_music_render(a,output,24000));
    printf("PASS: 120s muted timeline host CPU=%.6fs (not board measurement)\n",
           (double)(clock()-mute_start)/CLOCKS_PER_SEC);
    puts("PASS: track/volume transition is chunk independent");
    game_music_delete(a); game_music_delete(b);
    return 0;
}
