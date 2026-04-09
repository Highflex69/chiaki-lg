#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ss4s.h>
#include "config.h"

typedef struct VideoContext VideoContext;

VideoContext *video_init(SS4S_Player *player, int width, int height, int fps, int chiaki_codec,
                         const PerfProfile *profile);
void video_fini(VideoContext *ctx);

/* Actual ChiakiVideoSampleCallback typedef (confirmed from build error output):
 *   bool (*)(uint8_t *buf, unsigned int buf_size, int codec, bool is_keyframe, void *user)
 * chiaki passes codec type and keyframe flag — no need to parse NAL headers ourselves.
 * Returns true on success; chiaki logs a warning if false is returned. */
bool video_sample_cb(uint8_t *buf, unsigned int buf_size, int codec, bool is_keyframe, void *user);
