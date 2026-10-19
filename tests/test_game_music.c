// SPDX-License-Identifier: MIT
#include "game_music.h"
#include "game_music_score.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static game_music_handle_t a, b;
static int fail_allocation;
static unsigned allocation_calls;
void *game_music_test_calloc(size_t count, size_t size) {
    ++allocation_calls;
    if (fail_allocation) return NULL;
    void *p=malloc(count*size); if(p) memset(p,0,count*size); return p;
}
static int16_t output[24000], other[24000];
static unsigned hashes[GAME_MUSIC_TRACKS];
static unsigned hash(unsigned h, const int16_t *p, size_t n) {
    for (size_t i=0;i<n;++i) h=(h^(unsigned)(uint16_t)p[i])*16777619U;
    return h;
}
static void check_phrase(const uint8_t *actual, const char *score) {
    unsigned cell=0;
    while (*score) {
        char pitch[8]; unsigned length=0;
        size_t n=0;
        while (*score && *score!=':') { CHECK(n<sizeof(pitch)-1); pitch[n++]=*score++; }
        pitch[n]=0; CHECK(*score==':'); ++score;
        while (*score>='0' && *score<='9') length=length*10U+(unsigned)(*score++-'0');
        CHECK(length>0 && cell+length<=32);
        unsigned note=0;
        if (pitch[0]!='R') {
            const char *names="C D EF G A B";
            const char *position=strchr(names,pitch[0]); CHECK(position);
            unsigned index=1;
            int accidental=0;
            if (pitch[index]=='s') { accidental=1; ++index; }
            if (pitch[index]=='b') { accidental=-1; ++index; }
            CHECK(pitch[index]>='0' && pitch[index]<='8');
            note=(unsigned)((pitch[index]-'0'+1)*12+(position-names)+accidental);
        }
        CHECK(actual[cell++]==note);
        for(unsigned i=1;i<length;++i) CHECK(actual[cell++]==255);
        if (*score==' ') ++score;
    }
    CHECK(cell==32);
}
static void check_authored_scores(void) {
    static const char *const lead[9]={
        "D5:4 A4:2 R:2 F5:4 E5:4 D5:6 C5:2 A4:4 R:4",
        "A4:2 R:2 D5:4 E5:2 R:2 F5:4 E5:4 D5:4 B4:4 R:4",
        "E5:3 C5:1 R:2 A4:2 C5:4 E5:4 G5:3 E5:1 D5:4 C5:4 R:4",
        "A4:6 G4:2 F4:8 C5:6 A4:2 G4:4 R:4",
        "D5:4 A4:2 C5:2 F5:4 E5:4 D5:2 E5:2 F5:4 A5:4 R:4",
        "D5:2 E5:2 F5:4 A5:2 G5:2 E5:4 F5:2 E5:2 D5:4 Cs5:4 R:4",
        "D5:4 A4:2 R:2 Fs5:4 E5:4 D5:6 E5:2 Fs5:4 R:4",
        "D5:3 Fs5:1 A5:4 D6:8 Cs6:4 B5:4 A5:4 R:4",
        "D4:8 F4:4 E4:4 D4:8 R:8"};
    static const char *const answer[9]={
        "F5:4 G5:2 A5:2 G5:4 E5:4 F5:6 E5:2 D5:4 R:4",
        "G5:2 R:2 E5:4 D5:2 R:2 B4:4 C5:4 E5:4 D5:4 R:4",
        "A5:3 G5:1 E5:4 C5:2 R:2 D5:4 E5:3 C5:1 B4:4 A4:4 R:4",
        "C5:6 D5:2 A4:8 Bb4:6 A4:2 G4:4 R:4",
        "F5:4 G5:2 A5:2 C6:4 A5:4 G5:2 E5:2 D5:4 Cs5:4 R:4",
        "A5:2 Bb5:2 A5:4 F5:2 E5:2 D5:4 E5:2 F5:2 E5:4 Cs5:4 R:4",
        "Fs5:4 G5:2 A5:2 B5:4 A5:4 G5:6 Fs5:2 E5:4 R:4",
        "G5:4 B5:4 D6:8 Cs6:4 A5:4 D6:4 R:4",
        "C4:8 E4:4 D4:4 Cs4:8 R:8"};
    static const char *const bass[9]={
        "D3:8 R:8 C3:8 A2:8", "D3:8 R:8 G2:8 A2:8",
        "A2:6 E3:2 A2:8 F2:6 C3:2 E2:8", "F2:16 Bb2:8 C3:8",
        "D3:8 A2:8 Bb2:8 C3:8", "D3:4 A2:4 D3:8 Bb2:8 A2:8", "D3:16 G2:8 A2:8",
        "D3:8 A2:8 G2:8 A2:8", "D2:16 R:8 A2:8"};
    static const uint8_t expected_chords[9][4][3]={
        {{50,53,57},{58,62,65},{48,52,55},{57,60,64}},
        {{50,53,57},{55,59,62},{48,52,55},{50,53,57}},
        {{57,60,64},{53,57,60},{50,53,57},{52,56,59}},
        {{53,57,60},{58,62,65},{58,61,65},{48,52,55}},
        {{50,53,57},{58,62,65},{55,58,62},{57,61,64}},
        {{50,53,57},{58,62,65},{55,58,62},{57,61,64}},
        {{50,54,57},{55,59,62},{59,62,66},{57,61,64}},
        {{50,54,57},{55,59,62},{59,62,66},{57,61,64}},
        {{50,53,57},{58,62,65},{55,58,62},{57,61,64}}};
    for(unsigned track=1;track<GAME_MUSIC_TRACKS;++track) {
        for(unsigned section=0;section<4;++section) {
            check_phrase(melody[track]+section*32,section==2?answer[track-1]:lead[track-1]);
            check_phrase(bass_notes[track]+section*32,bass[track-1]);
        }
        CHECK(memcmp(chords[track],expected_chords[track-1],sizeof(expected_chords[0]))==0);
        CHECK(chord_span[track]==32); CHECK(unit_frames[track]>0);
        CHECK(lead_wave[track]<3); CHECK(voice_gain[track][0]<=3000);
    }
    CHECK(note_increment[0][69]==78741067U);
    CHECK(waveform[0][64]==26755);
    puts("PASS: exact authored pitches, sixteenth durations, AABA answers, bass and standard triads");
}
int main(void) {
    CHECK(GAME_MUSIC_TRACKS==10U);
    check_authored_scores();
    game_music_delete(NULL);
    CHECK(!game_music_create(NULL,NULL));
    fail_allocation=1; CHECK(!game_music_create(NULL,&a)); CHECK(a==NULL); fail_allocation=0;
    CHECK(!game_music_render(NULL,output,1));
    CHECK(!game_music_set_track(NULL,0));
    game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a);
    CHECK(!game_music_render(a,NULL,1));
    CHECK(game_music_render(a,NULL,0));
    CHECK(!game_music_set_volume(a,101));
    CHECK(!game_music_set_track(a,GAME_MUSIC_TRACKS));
    CHECK(game_music_loop_samples(GAME_MUSIC_TRACKS)==0);
    CHECK(game_music_track_name(GAME_MUSIC_TRACKS)==NULL);
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
    for(unsigned track=0;track<GAME_MUSIC_TRACKS;++track) {
        game_music_delete(a); CHECK(game_music_create(NULL,&a)); CHECK(a);
        CHECK(game_music_set_volume(a,0));
        CHECK(game_music_set_track(a,track));
        CHECK(game_music_set_volume(a,100));
        uint32_t frames=game_music_loop_samples(track);
        static const unsigned unit_frames[10]={7500,4286,5000,3750,5294,4091,3462,5000,3750,6429};
        CHECK(frames==128U*unit_frames[track]);
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
    for(unsigned left=0;left<GAME_MUSIC_TRACKS;++left)
        for(unsigned right=left+1;right<GAME_MUSIC_TRACKS;++right) CHECK(hashes[left]!=hashes[right]);
    CHECK(game_music_memory_bytes()<=6400);
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
    for(unsigned track=0;track<GAME_MUSIC_TRACKS;++track) {
        CHECK(game_music_set_volume(a,0)); CHECK(game_music_set_track(a,track));
        CHECK(game_music_set_volume(b,0)); CHECK(game_music_set_track(b,track));
        CHECK(game_music_set_volume(a,100)); CHECK(game_music_set_volume(b,100));
        unsigned before=allocation_calls;
        fail_allocation=1;
        CHECK(game_music_render(a,output,24000));
        for(size_t offset=0;offset<24000;) {
            size_t count=(offset%317)+1;
            if(count>24000-offset) count=24000-offset;
            CHECK(game_music_render(b,other+offset,count)); offset+=count;
        }
        CHECK(memcmp(output,other,sizeof(output))==0);
        CHECK(game_music_set_volume(a,0));
        CHECK(game_music_render(a,output,24000)); CHECK(game_music_render(b,other,24000));
        for(size_t i=0;i<24000;++i) CHECK(output[i]==0);
        CHECK(game_music_set_volume(a,100));
        CHECK(game_music_render(a,output,24000)); CHECK(game_music_render(b,other,24000));
        CHECK(memcmp(output+4096,other+4096,(24000-4096)*sizeof(*output))==0);
        CHECK(allocation_calls==before);
        fail_allocation=0;
    }
    puts("PASS: all ten tracks chunk deterministic, exact mute with advancing score, no render/setter allocation");
    game_music_delete(a); game_music_delete(b);
    return 0;
}
