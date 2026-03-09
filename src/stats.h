#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

// Thread-safe counters updated from Chiaki callback threads (video/audio)
// and sampled from the SDL/main thread for display.
typedef struct StreamStatsCounters
{
    atomic_uint_fast64_t video_bytes;
    atomic_uint_fast64_t video_frames;
    atomic_uint_fast64_t video_feed_fail;

    atomic_uint_fast64_t audio_bytes;
    atomic_uint_fast64_t audio_packets;

    atomic_int video_w;
    atomic_int video_h;
    atomic_int video_fps;   // configured/nominal fps (used for buffer-latency estimate)
    atomic_int video_codec; // CHIAKI_CODEC_* if available

    // Optional latency reported by SS4S video backend (ms). Updated from the main thread.
    // -1 means unknown/unavailable.
    atomic_int video_latency_ms;
} StreamStatsCounters;

extern StreamStatsCounters g_stream_stats;

void stats_reset(StreamStatsCounters *c);
void stats_set_video_format(StreamStatsCounters *c, int w, int h, int fps, int codec);

// Overlay state (main thread only)
typedef struct StatsOverlay
{
    bool enabled;

    uint32_t last_sample_ms;
    uint64_t last_video_bytes;
    uint64_t last_audio_bytes;
    uint64_t last_video_frames;
    uint64_t last_video_fail;

    // computed
    float mbps_total;
    float mbps_video;
    float mbps_audio;
    float fps;
    int   video_buf_frames;
    int   latency_ms;       // estimated pipeline buffer latency (video render queue)
    uint64_t feed_fail_total;

    char text[512];
} StatsOverlay;

void stats_overlay_init(StatsOverlay *o);
void stats_overlay_toggle(StatsOverlay *o);
void stats_overlay_update(StatsOverlay *o, const StreamStatsCounters *c, uint32_t now_ms);
