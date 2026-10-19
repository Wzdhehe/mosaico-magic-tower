// SPDX-License-Identifier: MIT
#pragma once
void game_audio_init(void);
void game_audio_set_volume(int volume);
/* Track IDs follow game_music.h. UI task controls wake a persistent owner;
 * volume zero closes playback and parks it until a changed control arrives. */
void game_audio_set_track(int track);
