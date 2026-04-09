#include "stats.h"

#include <stdio.h>
#include <string.h>

#include <chiaki/session.h> // for CHIAKI_CODEC_* constants

#ifdef HAVE_NDL_DIRECTMEDIA
#include "NDL_directmedia.h"
#endif

StreamStatsCounters g_stream_stats;

void stats_reset(StreamStatsCounters *c)
{
    if (!c)
        return;
    atomic_store_explicit(&c->video_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&c->video_frames, 0, memory_order_relaxed);
    atomic_store_explicit(&c->video_feed_fail, 0, memory_order_relaxed);

    atomic_store_explicit(&c->audio_bytes, 0, memory_order_relaxed);
    atomic_store_explicit(&c->audio_packets, 0, memory_order_relaxed);

    atomic_store_explicit(&c->video_latency_ms, -1, memory_order_relaxed);

    atomic_store_explicit(&c->jitter_avg_us, 0, memory_order_relaxed);
    atomic_store_explicit(&c->jitter_max_us, 0, memory_order_relaxed);
    atomic_store_explicit(&c->frames_dropped, 0, memory_order_relaxed);
    atomic_store_explicit(&c->ndl_buf_depth, 0, memory_order_relaxed);
}

void stats_set_video_format(StreamStatsCounters *c, int w, int h, int fps, int codec)
{
    if (!c)
        return;
    atomic_store_explicit(&c->video_w, w, memory_order_relaxed);
    atomic_store_explicit(&c->video_h, h, memory_order_relaxed);
    atomic_store_explicit(&c->video_fps, fps, memory_order_relaxed);
    atomic_store_explicit(&c->video_codec, codec, memory_order_relaxed);
}

void stats_overlay_init(StatsOverlay *o)
{
    memset(o, 0, sizeof(*o));
    o->enabled = false;
}

void stats_overlay_toggle(StatsOverlay *o)
{
    if (!o)
        return;
    o->enabled = !o->enabled;
    // force immediate refresh
    o->last_sample_ms = 0;
}

static const char *codec_to_str(int codec)
{
    switch (codec)
    {
    case CHIAKI_CODEC_H265:
        return "H.265";
    case CHIAKI_CODEC_H265_HDR:
        return "H.265 HDR";
    case CHIAKI_CODEC_H264:
        return "H.264";
    default:
        return "Unknown";
    }
}

static int query_video_render_buf_frames(void)
{
#ifdef HAVE_NDL_DIRECTMEDIA
    int len = -1;
    int rc = NDL_DirectVideoGetRenderBufferLength(&len);
    if (rc != 0 || len < 0)
        return -1;
    return len;
#else
    return -1;
#endif
}

void stats_overlay_update(StatsOverlay *o, const StreamStatsCounters *c, uint32_t now_ms)
{
    if (!o || !c || !o->enabled)
        return;

    const uint32_t sample_period_ms = 500;
    if (o->last_sample_ms != 0 && (uint32_t)(now_ms - o->last_sample_ms) < sample_period_ms)
        return;

    uint64_t vbytes = atomic_load_explicit(&c->video_bytes, memory_order_relaxed);
    uint64_t abytes = atomic_load_explicit(&c->audio_bytes, memory_order_relaxed);
    uint64_t vframes = atomic_load_explicit(&c->video_frames, memory_order_relaxed);
    uint64_t vfail = atomic_load_explicit(&c->video_feed_fail, memory_order_relaxed);

    int vw = atomic_load_explicit(&c->video_w, memory_order_relaxed);
    int vh = atomic_load_explicit(&c->video_h, memory_order_relaxed);
    int vfps_cfg = atomic_load_explicit(&c->video_fps, memory_order_relaxed);
    int vcodec = atomic_load_explicit(&c->video_codec, memory_order_relaxed);
    int vlat_ms_reported = atomic_load_explicit(&c->video_latency_ms, memory_order_relaxed);

    int64_t jitter_avg = atomic_load_explicit(&c->jitter_avg_us, memory_order_relaxed);
    int64_t jitter_max = atomic_load_explicit(&c->jitter_max_us, memory_order_relaxed);
    uint64_t dropped = atomic_load_explicit(&c->frames_dropped, memory_order_relaxed);
    int buf_depth = atomic_load_explicit(&c->ndl_buf_depth, memory_order_relaxed);

    float dt = 0.0f;
    if (o->last_sample_ms != 0)
        dt = (float)((uint32_t)(now_ms - o->last_sample_ms)) / 1000.0f;

    if (dt > 0.0f)
    {
        double dv = (double)(vbytes - o->last_video_bytes);
        double da = (double)(abytes - o->last_audio_bytes);
        double df = (double)(vframes - o->last_video_frames);

        o->mbps_video = (float)((dv * 8.0) / (dt * 1000.0 * 1000.0));
        o->mbps_audio = (float)((da * 8.0) / (dt * 1000.0 * 1000.0));
        o->mbps_total = o->mbps_video + o->mbps_audio;
        o->fps = (float)(df / dt);
    }

    o->feed_fail_total = vfail;
    o->jitter_ms = (float)jitter_avg / 1000.0f;
    o->jitter_max_ms = (float)jitter_max / 1000.0f;
    o->frames_dropped = dropped;

    // Estimated pipeline buffer latency (video render queue). This is only a proxy for
    // "how much video is buffered", not a full end-to-end network latency.
    o->video_buf_frames = buf_depth;
    o->latency_ms = -1;
    if (o->video_buf_frames >= 0)
    {
        float fps_for_lat = (o->fps > 1.0f) ? o->fps : (float)vfps_cfg;
        if (fps_for_lat > 1.0f)
        {
            float buf_frames = (o->video_buf_frames > 0) ? (float)o->video_buf_frames : 0.5f;
            o->latency_ms = (int)((buf_frames * 1000.0f / fps_for_lat) + 0.5f);
        }
    }

    o->last_sample_ms = now_ms;
    o->last_video_bytes = vbytes;
    o->last_audio_bytes = abytes;
    o->last_video_frames = vframes;
    o->last_video_fail = vfail;

    // Build display string
    char latency_line[96];
    if (vlat_ms_reported >= 0)
        snprintf(latency_line, sizeof(latency_line), "Latency: ~%d ms (SS4S avg)\n", vlat_ms_reported);
    else if (o->latency_ms >= 0)
        snprintf(latency_line, sizeof(latency_line), "Latency: ~%d ms (buf %d f)\n", o->latency_ms, o->video_buf_frames);
    else
        snprintf(latency_line, sizeof(latency_line), "Latency: n/a\n");

    snprintf(o->text, sizeof(o->text),
             "Stream Stats (UP to toggle)\n"
             "Video: %dx%d  %s\n"
             "Bitrate: %.2f Mbps  (V %.2f / A %.2f)\n"
             "FPS: %.1f\n"
             "%s"
             "Jitter: %.1f ms avg / %.1f ms max\n"
             "NDL buf: %d frames  Dropped: %llu\n"
             "Feed errors: %llu\n",
             vw, vh, codec_to_str(vcodec),
             o->mbps_total, o->mbps_video, o->mbps_audio,
             o->fps,
             latency_line,
             o->jitter_ms, o->jitter_max_ms,
             buf_depth, (unsigned long long)o->frames_dropped,
             (unsigned long long)o->feed_fail_total);
}
