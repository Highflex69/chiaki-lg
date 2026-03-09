#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ss4s.h>
#include <chiaki/audio.h>
#include <chiaki/session.h>

typedef struct AudioContext AudioContext;

AudioContext    *audio_init(SS4S_Player *player);
void             audio_fini(AudioContext *ctx);
ChiakiAudioSink  audio_make_sink(AudioContext *ctx);
