#include "video.h"
#include "stats.h"
#include <chiaki/session.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration — defined in main.c */
extern void app_log(const char *fmt, ...);
// Set to true once we've received the first video sample.
extern volatile bool g_have_video_frame;

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

/* NAL IDR detection removed — chiaki provides is_keyframe directly via callback. */

/* ------------------------------------------------------------------------- */

struct VideoContext {
    SS4S_Player *player;
    int          requested_codec; // CHIAKI_CODEC_* requested in session init
    int          effective_codec; // CHIAKI_CODEC_* used for UI/stats (may be updated if chiaki reports a sane value)
    bool         use_h265;      // true for H.265 or H.265 HDR sessions
    bool         opened;
    uint64_t     frame_count;
    unsigned     reported_samples;
    unsigned     reported_mask;   // bitmask of distinct reported codec values (only for known CHIAKI_CODEC_* values)
};

VideoContext *video_init(SS4S_Player *player, int width, int height, int fps, int chiaki_codec)
{
    VideoContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->player = player;
    ctx->requested_codec = chiaki_codec;
    ctx->effective_codec = chiaki_codec;
    ctx->use_h265 = (chiaki_codec == CHIAKI_CODEC_H265 || chiaki_codec == CHIAKI_CODEC_H265_HDR);

    // Stats overlay format hint (also updated from callback codec)
    stats_set_video_format(&g_stream_stats, width, height, fps, chiaki_codec);

    SS4S_VideoInfo info = {
        .codec  = ctx->use_h265 ? SS4S_VIDEO_H265 : SS4S_VIDEO_H264,
        .width  = width,
        .height = height,
        .frameRateNumerator   = fps,
        .frameRateDenominator = 1,
    };

    int rc = SS4S_PlayerVideoOpen(ctx->player, &info);
    if (rc != SS4S_OK) {
        app_log("[VIDEO/SS4S] PlayerVideoOpen FAILED rc=%d  codec=%s %dx%d@%dfps\n",
                rc, ctx->use_h265 ? "H265" : "H264", width, height, fps);
        free(ctx);
        return NULL;
    }

    app_log("[VIDEO/SS4S] PlayerVideoOpen OK  codec=%s %dx%d@%dfps\n",
            ctx->use_h265 ? "H265" : "H264", width, height, fps);

    ctx->opened = true;
    return ctx;
}

void video_fini(VideoContext *ctx)
{
    if (!ctx) return;
    if (ctx->opened) SS4S_PlayerVideoClose(ctx->player);
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

    // Some Chiaki builds/platforms have been observed to pass a "codec" value that doesn't
    // match CHIAKI_CODEC_* for the active stream (e.g. 0/1 behaving like a boolean, where
    // 0 == CHIAKI_CODEC_H264 even for H.265 sessions).
    // Rule: never allow the callback to "downgrade" a session that was negotiated as HEVC
    // (H.265 or H.265 HDR) to H.264. The negotiated codec (requested_codec) is the ground
    // truth; callback values are only trusted when they agree with or upgrade the expectation.
    const bool codec_known = (codec == CHIAKI_CODEC_H264 || codec == CHIAKI_CODEC_H265 || codec == CHIAKI_CODEC_H265_HDR);
    if(codec_known)
    {
        ctx->reported_samples++;
        if(codec >= 0 && codec <= 31)
            ctx->reported_mask |= (1u << (unsigned)codec);

        // Log the first few frames so the negotiated vs reported codec is clearly visible
        // in /tmp/chiaki.log — useful for diagnosing overlay codec mismatches.
        if(ctx->reported_samples <= 3)
            app_log("[VIDEO] sample #%u: negotiated=%d reported=%d keyframe=%d\n",
                    ctx->reported_samples,
                    ctx->requested_codec, codec, is_keyframe ? 1 : 0);

        const bool session_is_hevc = (ctx->requested_codec == CHIAKI_CODEC_H265 ||
                                      ctx->requested_codec == CHIAKI_CODEC_H265_HDR);
        const bool callback_says_h264 = (codec == CHIAKI_CODEC_H264);

        if(session_is_hevc && callback_says_h264)
        {
            // Callback is reporting H.264 for an HEVC-negotiated session — this is the
            // "codec behaves as boolean" bug in some chiaki-ng builds. Ignore it.
            if(ctx->reported_samples == 1)
                app_log("[VIDEO] ignoring spurious H.264 codec report for %s session "
                        "(negotiated=%d callback=%d)\n",
                        ctx->requested_codec == CHIAKI_CODEC_H265_HDR ? "H.265 HDR" : "H.265",
                        ctx->requested_codec, codec);
        }
        else if(codec != ctx->effective_codec)
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

    if (is_keyframe) {
        flags |= SS4S_VIDEO_FEED_DATA_KEYFRAME;
        ctx->frame_count = 0;
    }

    ctx->frame_count++;
    if (ctx->frame_count <= 5 || ctx->frame_count % 600 == 0) {
        app_log("[VIDEO/SS4S] frame=%llu  size=%u  keyframe=%d\n",
                (unsigned long long)ctx->frame_count, buf_size, is_keyframe);
    }

    int rc = SS4S_PlayerVideoFeed(ctx->player, buf, (size_t)buf_size, flags);
    if (rc != SS4S_OK) {
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
