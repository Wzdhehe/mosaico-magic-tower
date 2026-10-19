// SPDX-License-Identifier: MIT
#include "game_music.h"
#include <stdlib.h>
#include <string.h>
#include "game_music_score.h"
#define VOICES 4U
#define ECHO_FRAMES 3072U
#define GAIN_ONE 32768

typedef struct {
    uint32_t phase, increment, age, duration;
    int32_t gain;
} voice_t;
struct game_music_t {
    uint32_t remaining, unit, echo_index;
    uint64_t position;
    int32_t gain, target_gain, fade;
    uint16_t step;
    uint8_t track, requested_track, volume;
    voice_t voice[VOICES];
    int16_t echo[ECHO_FRAMES];
};

static void reset_track(game_music_handle_t m, unsigned track) {
    m->track=(uint8_t)track;
    m->unit=unit_frames[track];
    m->remaining=0;
    m->step=0;
    m->position=0;
    m->echo_index=0;
    memset(m->voice,0,sizeof(m->voice));
    memset(m->echo,0,sizeof(m->echo));
}
bool game_music_create(const game_music_config_t *config, game_music_handle_t *ret_handle) {
    if (!ret_handle) return false;
    *ret_handle=NULL;
    if (config && config->reserved) return false;
    game_music_handle_t m=calloc(1,sizeof(*m));
    if (!m) return false;
    m->volume=45;
    m->target_gain=45*GAIN_ONE/100;
    reset_track(m,0);
    *ret_handle=m;
    return true;
}
void game_music_delete(game_music_handle_t m) { free(m); }
size_t game_music_memory_bytes(void) { return sizeof(struct game_music_t); }
uint32_t game_music_loop_samples(unsigned track) {
    return track<GAME_MUSIC_TRACKS ? (uint32_t)unit_frames[track]*128U : 0;
}
uint64_t game_music_position(game_music_handle_t m) { return m?m->position:0; }
const char *game_music_track_name(unsigned track) {
    return track<GAME_MUSIC_TRACKS?track_names[track]:NULL;
}
bool game_music_set_volume(game_music_handle_t m, unsigned volume) {
    if (!m || volume>100) return false;
    if (m->volume==volume) return true;
    m->volume=(uint8_t)volume;
    m->target_gain=(int32_t)volume*GAIN_ONE/100;
    if (!volume) {
        m->gain=0;
        /* Mute means exact silence immediately; discard the short echo tail.
         * Oscillators/score continue in the inexpensive silent advance path. */
        memset(m->echo,0,sizeof(m->echo));
        if (m->requested_track!=m->track) reset_track(m,m->requested_track);
    }
    return true;
}
bool game_music_set_track(game_music_handle_t m, unsigned track) {
    if (!m || track>=GAME_MUSIC_TRACKS) return false;
    m->requested_track=(uint8_t)track;
    if (!m->volume && m->track!=track) { reset_track(m,track); m->fade=0; }
    return true;
}
static void note_on(game_music_handle_t m, unsigned index, unsigned note, uint32_t frames) {
    voice_t *v=&m->voice[index];
    v->phase=0; v->age=0; v->duration=frames;
    v->increment=note?note_increment[0][note]:0;
    v->gain=note?voice_gain[m->track][index]:0;
    if (m->track==2 && index==0 && v->duration>4000) v->duration=4000;
}
static void begin_unit(game_music_handle_t m) {
    unsigned cell=m->step, span=chord_span[m->track], chord=cell/span;
    uint8_t note=melody[m->track][cell];
    if (note!=255) {
        unsigned run=1;
        while(cell+run<128 && melody[m->track][cell+run]==255) ++run;
        note_on(m,0,note,run*m->unit-320);
    }
    if (!(cell%span)) {
        note_on(m,1,chords[m->track][chord][1],span*m->unit-1800);
        note_on(m,2,chords[m->track][chord][2],span*m->unit-1800);
    }
    uint8_t bass=bass_notes[m->track][cell];
    if (bass!=255) {
        unsigned run=1;
        while(cell+run<128 && bass_notes[m->track][cell+run]==255) ++run;
        note_on(m,3,bass,run*m->unit-450);
    }
    m->remaining=m->unit;
    m->step=(uint16_t)((cell+1)&127);
}
static void advance_clock(game_music_handle_t m, uint32_t frames) {
    m->remaining-=frames;
    m->position+=frames;
    if (m->position==game_music_loop_samples(m->track)) m->position=0;
}
static void silent_advance(game_music_handle_t m, size_t frames) {
    while(frames) {
        if (!m->remaining) begin_unit(m);
        uint32_t n=frames<m->remaining?(uint32_t)frames:m->remaining;
        for(unsigned i=0;i<VOICES;++i) {
            voice_t *v=&m->voice[i];
            v->phase+=v->increment*n;
            v->age+=n;
        }
        advance_clock(m,n); frames-=n;
    }
}
static int32_t voice_sample(voice_t *v, unsigned wave) {
    if (!v->gain || v->age>=v->duration) return 0;
    uint32_t age=v->age++, remaining=v->duration-age;
    int32_t envelope=GAIN_ONE;
    if (age<128) envelope=(int32_t)age<<8;
    if (remaining<1024) envelope=(int32_t)remaining<<5;
    v->phase+=v->increment;
    return (((int32_t)waveform[wave][v->phase>>24]*v->gain)>>15)*envelope>>15;
}
bool game_music_render(game_music_handle_t m, int16_t *output, size_t frames) {
    if (!m || (!output && frames)) return false;
    if (!frames) return true;
    if (!m->volume) {
        memset(output,0,frames*sizeof(*output));
        silent_advance(m,frames);
        return true;
    }
    for(size_t i=0;i<frames;++i) {
        if (m->track!=m->requested_track) {
            if (m->fade>0) m->fade-=128;
            if (m->fade<=0) { reset_track(m,m->requested_track); m->fade=0; }
        } else if (m->fade<GAIN_ONE) m->fade+=128;
        if (!m->remaining) begin_unit(m);
        if (m->gain<m->target_gain) {
            m->gain+=64; if(m->gain>m->target_gain) m->gain=m->target_gain;
        } else if (m->gain>m->target_gain) {
            m->gain-=64; if(m->gain<m->target_gain) m->gain=m->target_gain;
        }
        int32_t dry=0;
        for(unsigned v=0;v<VOICES;++v) dry+=voice_sample(&m->voice[v],v==0?lead_wave[m->track]:0);
        int32_t mixed=dry+(m->echo[m->echo_index]>>2);
        m->echo[m->echo_index]=(int16_t)dry;
        if (++m->echo_index==ECHO_FRAMES) m->echo_index=0;
        /* 2.25x makeup: full volume matches the comfortable level that was
         * previously heard at 45%, leaving ample headroom for the speaker. */
        mixed=(((mixed*m->gain)>>15)*m->fade>>15)*9/4;
        if(mixed>32767) mixed=32767;
        if(mixed<-32768) mixed=-32768;
        output[i]=(int16_t)mixed;
        advance_clock(m,1);
    }
    return true;
}
