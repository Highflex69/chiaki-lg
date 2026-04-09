#include "video.h"
#include "stats.h"
#include <chiaki/session.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declaration — defined in main.c */
extern void app_log(const char *fmt, ...);
extern void app_log_always(const char *fmt, ...);
// Set to true once we've received the first video sample.
extern volatile bool g_have_video_frame;

/* NDL direct calls for A/V sync and buffer control */
#ifdef HAVE_NDL_DIRECTMEDIA
#include "NDL_directmedia.h"
#endif

/* ------------------------------------------------------------------------- */
/* SS4S compatibility layer                                                   */
/* ------------------------------------------------------------------------- */

#ifndef SS4S_OK
#ifdef SS4S_RESULT_OK
#define SS4S_OK SS4S_RESULT_OK
#else
#define SS4S_OK 0
#endif
#endif

/* ------------------------------------------------------------------------- */
/* Frame pacing: limit how far ahead we feed NDL to keep latency bounded.    */
/* MAX_NDL_BUF_DEPTH: if the NDL render buffer exceeds this many frames, we  */
/* drop the incoming frame to prevent latency buildup. A depth of 2 provides */
/* one frame of cushion for jitter without adding more than ~16ms latency.    */
/* AUTO_TUNE_INTERVAL: how often (in frames) the auto-tuner recalculates.    */
/* ------------------------------------------------------------------------- */
#define MAX_NDL_BUF_DEPTH_DEFAULT 2
#define JITTER_EMA_ALPHA 0.05  /* smoothing factor for EMA (lower = smoother) */
#define AUTO_TUNE_INTERVAL 120 /* frames between auto-tuner adjustments */

/* ------------------------------------------------------------------------- */

struct VideoContext
{
    SS4S_Player *player;
    int requested_codec;
    int effective_codec;
    bool use_h265;
    bool opened;
    uint64_t frame_count;
    unsigned reported_samples;
    unsigned reported_mask;
    int fps;

    /* ── Jitter estimator ──────────────────────────────────────────────────── */
    struct timespec last_frame_ts; /* monotonic timestamp of previous frame arrival */
    bool have_last_ts;
    int64_t expected_interval_us; /* 1/fps in microseconds */
    double jitter_ema_us;         /* exponential moving average of |deviation| */
    int64_t jitter_window_max_us; /* max jitter in current sample window */

    /* ── Frame buffer limiter ──────────────────────────────────────────────── */
    int max_buf_depth; /* current limit (may be adjusted by auto-tuner) */

    /* ── Auto-tuner ────────────────────────────────────────────────────────── */
    uint64_t tune_frame_counter; /* frames since last auto-tune */
    double tune_avg_jitter_us;   /* average jitter during tune interval */
};

/* ── Clock helper ──────────────────────────────────────────────────────────── */
static inline int64_t ts_diff_us(const struct timespec *a, const struct timespec *b)
{
    return (int64_t)(a->tv_sec - b->tv_sec) * 1000000LL +
           (int64_t)(a->tv_nsec - b->tv_nsec) / 1000LL;
}

/* ── NDL buffer depth query ────────────────────────────────────────────────── */
static int query_ndl_buf_depth(void)
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

VideoContext *video_init(SS4S_Player *player, int width, int height, int fps, int chiaki_codec,
                         const PerfProfile *profile)
{
    VideoContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->player = player;
    ctx->requested_codec = chiaki_codec;
    ctx->effective_codec = chiaki_codec;
    ctx->use_h265 = (chiaki_codec == CHIAKI_CODEC_H265 || chiaki_codec == CHIAKI_CODEC_H265_HDR);
    ctx->fps = fps;
    ctx->expected_interval_us = (fps > 0) ? (1000000LL / fps) : 16667LL;

    /* Apply profile */
    ctx->max_buf_depth = profile->max_buf_depth;
    ctx->jitter_ema_alpha = profile->jitter_ema_alpha;
    ctx->auto_tune_min = profile->auto_tune_min;
    ctx->auto_tune_max = profile->auto_tune_max;

    // Stats overlay format hint (also updated from callback codec)
    stats_set_video_format(&g_stream_stats, width, height, fps, chiaki_codec);

    SS4S_VideoInfo info = {
        .codec = ctx->use_h265 ? SS4S_VIDEO_H265 : SS4S_VIDEO_H264,
        .width = width,
        .height = height,
        .frameRateNumerator = fps,
        .frameRateDenominator = 1,
    };

    int rc = SS4S_PlayerVideoOpen(ctx->player, &info);
    if (rc != SS4S_OK)
    {
        app_log("[VIDEO/SS4S] PlayerVideoOpen FAILED rc=%d  codec=%s %dx%d@%dfps\n",
                rc, ctx->use_h265 ? "H265" : "H264", width, height, fps);
        free(ctx);
        return NULL;
    }

    app_log("[VIDEO/SS4S] PlayerVideoOpen OK  codec=%s %dx%d@%dfps\n",
            ctx->use_h265 ? "H265" : "H264", width, height, fps);

    /* ── NDL low-latency configuration ──────────────────────────────────── */
#ifdef HAVE_NDL_DIRECTMEDIA
    /* Tell NDL to drop frames when the render buffer gets too deep.
     * The threshold is driven by the performance profile:
     *   fast=1 (most aggressive), balanced=2, safe=4 (NDL manages pacing).
     * Combined with feeding PTS=0 (which SS4S does by default for game
     * streaming), a low threshold effectively disables the A/V synchroniser
     * and puts NDL into free-run / low-latency mode. */
    if (NDL_DirectVideoSetFrameDropThreshold(profile->ndl_drop_threshold) != 0)
        app_log("[VIDEO/NDL] SetFrameDropThreshold(%d) failed — non-fatal\n",
                profile->ndl_drop_threshold);
    else
        app_log("[VIDEO/NDL] SetFrameDropThreshold(%d) OK\n",
                profile->ndl_drop_threshold);
#endif

    app_log("[VIDEO] perf profile: buf_depth=%d ndl_thresh=%d alpha=%.2f tune=%d..%d\n",
            ctx->max_buf_depth, profile->ndl_drop_threshold,
            ctx->jitter_ema_alpha, ctx->auto_tune_min, ctx->auto_tune_max);

    ctx->opened = true;
    return ctx;
}

void video_fini(VideoContext *ctx)
{
    if (!ctx)
        return;
    if (ctx->opened)
        SS4S_PlayerVideoClose(ctx->player);
    free(ctx);
}

/* Actual ChiakiVideoSampleCallback signature (confirmed from build error):
 *   bool (uint8_t *buf, unsigned int buf_size, int codec, bool is_keyframe, void *user)
 * chiaki provides codec type and keyframe flag directly, so no NAL parsing needed.
 * Must return true on success; chiaki logs a warning if we return false. */
bool video_sample_cb(uint8_t *buf, unsigned int buf_size, int codec, bool is_keyframe, void *user)
{
    /* codec is provided by Chiaki per-sample; useful to confirm negotiation (H264/H265/H265_HDR). */

    VideoContext *ctx = (VideoContext *)user;

    if (!ctx || !ctx->opened)
        return false;

    /* ── Jitter estimator ──────────────────────────────────────────────── */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (ctx->have_last_ts)
    {
        int64_t delta_us = ts_diff_us(&now, &ctx->last_frame_ts);
        int64_t deviation = delta_us - ctx->expected_interval_us;
        int64_t abs_dev = deviation < 0 ? -deviation : deviation;

        /* EMA update */
        ctx->jitter_ema_us = ctx->jitter_ema_us * (1.0 - ctx->jitter_ema_alpha) + (double)abs_dev * ctx->jitter_ema_alpha;

        /* Window max */
        if (abs_dev > ctx->jitter_window_max_us)
            ctx->jitter_window_max_us = abs_dev;

        /* Publish to stats atomics */
        atomic_store_explicit(&g_stream_stats.jitter_avg_us,
                              (int64_t)ctx->jitter_ema_us, memory_order_relaxed);
        atomic_store_explicit(&g_stream_stats.jitter_max_us,
                              ctx->jitter_window_max_us, memory_order_relaxed);
    }
    ctx->last_frame_ts = now;
    ctx->have_last_ts = true;

    /* ── NDL buffer depth query & frame limiter ────────────────────────── */
    int buf_depth = query_ndl_buf_depth();
    if (buf_depth >= 0)
    {
        atomic_store_explicit(&g_stream_stats.ndl_buf_depth,
                              (int64_t)buf_depth, memory_order_relaxed);

        if (!is_keyframe && buf_depth > ctx->max_buf_depth)
        {
            /* Drop this frame to prevent latency buildup.
             * Never drop keyframes — losing an IDR causes decode errors. */
            atomic_fetch_add_explicit(&g_stream_stats.frames_dropped, 1, memory_order_relaxed);
            if (ctx->frame_count <= 10 || ctx->frame_count % 300 == 0)
                app_log("[VIDEO] DROPPED frame=%llu  buf_depth=%d > max=%d\n",
                        (unsigned long long)ctx->frame_count, buf_depth, ctx->max_buf_depth);
            return true; /* tell chiaki we "consumed" it */
        }
    }

    /* ── Auto-tuner ────────────────────────────────────────────────────── */
    ctx->tune_frame_counter++;
    ctx->tune_avg_jitter_us += (ctx->jitter_ema_us - ctx->tune_avg_jitter_us) / (double)ctx->tune_frame_counter;

    if (ctx->tune_frame_counter >= AUTO_TUNE_INTERVAL)
    {
        /* Adapt max_buf_depth based on observed jitter.
         * High jitter (>3ms) → allow slightly deeper buffer to absorb variance.
         * Low jitter (<1ms) → tighten to minimum for lowest latency. */
        if (ctx->tune_avg_jitter_us > 3000.0 && ctx->max_buf_depth < ctx->auto_tune_max)
            ctx->max_buf_depth++;
        else if (ctx->tune_avg_jitter_us < 1000.0 && ctx->max_buf_depth > ctx->auto_tune_min)
            ctx->max_buf_depth--;

        if (ctx->frame_count > 0 && ctx->frame_count % (AUTO_TUNE_INTERVAL * 10) == 0)
            app_log_always("[VIDEO/TUNE] jitter_avg=%.0fus max_buf=%d\n",
                           ctx->tune_avg_jitter_us, ctx->max_buf_depth);

        /* Reset window max for next interval */
        ctx->jitter_window_max_us = 0;
        ctx->tune_frame_counter = 0;
        ctx->tune_avg_jitter_us = 0.0;
    }

    // Some Chiaki builds/platforms have been observed to pass a "codec" value that doesn't
    // match CHIAKI_CODEC_* for the active stream (e.g. 0/1 behaving like a boolean, where
    // 0 == CHIAKI_CODEC_H264 even for H.265 sessions).
    // Rule: never allow the callback to "downgrade" a session that was negotiated as HEVC
    // (H.265 or H.265 HDR) to H.264. The negotiated codec (requested_codec) is the ground
    // truth; callback values are only trusted when they agree with or upgrade the expectation.
    const bool codec_known = (codec == CHIAKI_CODEC_H264 || codec == CHIAKI_CODEC_H265 || codec == CHIAKI_CODEC_H265_HDR);
    if (codec_known)
    {
        ctx->reported_samples++;
        if (codec >= 0 && codec <= 31)
            ctx->reported_mask |= (1u << (unsigned)codec);

        // Log the first few frames so the negotiated vs reported codec is clearly visible
        // in /tmp/chiaki.log — useful for diagnosing overlay codec mismatches.
        if (ctx->reported_samples <= 3)
            app_log("[VIDEO] sample #%u: negotiated=%d reported=%d keyframe=%d\n",
                    ctx->reported_samples,
                    ctx->requested_codec, codec, is_keyframe ? 1 : 0);

        const bool session_is_hevc = (ctx->requested_codec == CHIAKI_CODEC_H265 ||
                                      ctx->requested_codec == CHIAKI_CODEC_H265_HDR);
        const bool callback_says_h264 = (codec == CHIAKI_CODEC_H264);

        if (session_is_hevc && callback_says_h264)
        {
            // Callback is reporting H.264 for an HEVC-negotiated session — this is the
            // "codec behaves as boolean" bug in some chiaki-ng builds. Ignore it.
            if (ctx->reported_samples == 1)
                app_log("[VIDEO] ignoring spurious H.264 codec report for %s session "
                        "(negotiated=%d callback=%d)\n",
                        ctx->requested_codec == CHIAKI_CODEC_H265_HDR ? "H.265 HDR" : "H.265",
                        ctx->requested_codec, codec);
        }
        else if (codec != ctx->effective_codec)
        {
            app_log("[VIDEO] codec updated: negotiated=%d effective=%d -> reported=%d\n",
                    ctx->requested_codec, ctx->effective_codec, codec);
            ctx->effective_codec = codec;
        }
    }

    // Update stats counters (callback thread)
    atomic_fetch_add_explicit(&g_stream_stats.video_bytes, (uint64_t)buf_size, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_stream_stats.video_frames, 1, memory_order_relaxed);
    // Store what we believe is the real effective codec for the overlay.
    atomic_store_explicit(&g_stream_stats.video_codec, ctx->effective_codec, memory_order_relaxed);

    SS4S_VideoFeedFlags flags =
        SS4S_VIDEO_FEED_DATA_FRAME_START |
        SS4S_VIDEO_FEED_DATA_FRAME_END;

    if (is_keyframe)
    {
        flags |= SS4S_VIDEO_FEED_DATA_KEYFRAME;
        ctx->frame_count = 0;
    }

    ctx->frame_count++;
    if (ctx->frame_count <= 5 || ctx->frame_count % 600 == 0)
    {
        app_log("[VIDEO/SS4S] frame=%llu  size=%u  keyframe=%d\n",
                (unsigned long long)ctx->frame_count, buf_size, is_keyframe);
    }

    int rc = SS4S_PlayerVideoFeed(ctx->player, buf, (size_t)buf_size, flags);
    if (rc != SS4S_OK)
    {
        if (ctx->frame_count <= 5)
            app_log("[VIDEO/SS4S] PlayerVideoFeed FAILED rc=%d\n", rc);
        atomic_fetch_add_explicit(&g_stream_stats.video_feed_fail, 1, memory_order_relaxed);
        return false;
    }

    // First successful feed → treat stream as "started" for UI purposes.
    if (!g_have_video_frame)
        g_have_video_frame = true;
    return true;
}
