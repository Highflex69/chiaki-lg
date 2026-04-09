#include "audio.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declaration — defined in main.c, shared via app_log.h */
extern void app_log(const char *fmt, ...);

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
/* Audio jitter buffer — ring of raw Opus packets                            */
/* ------------------------------------------------------------------------- */

#define AUDIO_RING_SLOTS 8
#define AUDIO_RING_MAX_PKT 512

typedef struct
{
    uint8_t data[AUDIO_RING_MAX_PKT];
    size_t len;
    struct timespec ts; /* arrival timestamp */
} AudioRingSlot;

/* Clock helper */
static inline int64_t audio_ts_diff_us(const struct timespec *a, const struct timespec *b)
{
    return (int64_t)(a->tv_sec - b->tv_sec) * 1000000LL +
           (int64_t)(a->tv_nsec - b->tv_nsec) / 1000LL;
}

/* ------------------------------------------------------------------------- */

struct AudioContext
{
    SS4S_Player *player;
    bool opened;
    int channels;

    /* ── Jitter estimator ─────────────────────────────────────────────── */
    struct timespec last_pkt_ts;
    bool have_last_ts;
    int64_t expected_interval_us; /* nominally 10000 (10ms at 48kHz/480) */
    double jitter_ema_us;

    /* ── Ring buffer for burst absorption ─────────────────────────────── */
    AudioRingSlot ring[AUDIO_RING_SLOTS];
    int ring_head; /* next write position */
    int ring_tail; /* next read position */
    int ring_count;
    int target_depth; /* packets to hold (0 = passthrough) */
    int64_t hold_us;  /* target_depth * expected_interval_us */
};

/* ── Ring helpers ──────────────────────────────────────────────────────── */

static void ring_enqueue(AudioContext *ctx, const uint8_t *buf, size_t len,
                         const struct timespec *ts)
{
    if (ctx->ring_count >= AUDIO_RING_SLOTS)
    {
        /* Overflow — drop oldest silently */
        ctx->ring_tail = (ctx->ring_tail + 1) % AUDIO_RING_SLOTS;
        ctx->ring_count--;
    }
    AudioRingSlot *s = &ctx->ring[ctx->ring_head];
    size_t copy = len < AUDIO_RING_MAX_PKT ? len : AUDIO_RING_MAX_PKT;
    memcpy(s->data, buf, copy);
    s->len = copy;
    s->ts = *ts;
    ctx->ring_head = (ctx->ring_head + 1) % AUDIO_RING_SLOTS;
    ctx->ring_count++;
}

static AudioRingSlot *ring_peek(AudioContext *ctx)
{
    if (ctx->ring_count <= 0)
        return NULL;
    return &ctx->ring[ctx->ring_tail];
}

static void ring_dequeue(AudioContext *ctx)
{
    if (ctx->ring_count <= 0)
        return;
    ctx->ring_tail = (ctx->ring_tail + 1) % AUDIO_RING_SLOTS;
    ctx->ring_count--;
}

/* ── Callbacks ─────────────────────────────────────────────────────────── */

static void audio_header_cb(ChiakiAudioHeader *header, void *user)
{
    AudioContext *ctx = (AudioContext *)user;

    app_log("[AUDIO/SS4S] Header: rate=%u ch=%u bits=%u frame_size=%u\n",
            header->rate, header->channels, header->bits, header->frame_size);

    ctx->channels = header->channels;

    /* Estimate expected packet interval from frame_size and sample rate.
     * Typically frame_size=480 at 48kHz → 10ms. */
    if (header->rate > 0 && header->frame_size > 0)
        ctx->expected_interval_us = (int64_t)header->frame_size * 1000000LL / (int64_t)header->rate;
    else
        ctx->expected_interval_us = 10000; /* 10ms fallback */

    ctx->hold_us = (int64_t)ctx->target_depth * ctx->expected_interval_us;

    SS4S_AudioInfo info = {
        .codec = SS4S_AUDIO_OPUS,
        .numOfChannels = (int)header->channels,
        .sampleRate = (int)header->rate,
        .appName = "org.homebrew.chiaki",
    };

    int rc = SS4S_PlayerAudioOpen(ctx->player, &info);
    if (rc == SS4S_OK)
    {
        ctx->opened = true;
        app_log("[AUDIO/SS4S] PlayerAudioOpen OK  ch=%d rate=%d interval=%lldus buf_depth=%d\n",
                info.numOfChannels, info.sampleRate,
                (long long)ctx->expected_interval_us, ctx->target_depth);
    }
    else
    {
        app_log("[AUDIO/SS4S] PlayerAudioOpen FAILED rc=%d\n", rc);
    }
}

static void audio_frame_cb(unsigned char *buf, unsigned int samples_count, void *user)
{
    AudioContext *ctx = (AudioContext *)user;

    if (!ctx->opened || !buf || samples_count == 0)
        return;

    /* ── Jitter estimator ──────────────────────────────────────────── */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (ctx->have_last_ts)
    {
        int64_t delta_us = audio_ts_diff_us(&now, &ctx->last_pkt_ts);
        int64_t deviation = delta_us - ctx->expected_interval_us;
        int64_t abs_dev = deviation < 0 ? -deviation : deviation;

        /* EMA with alpha=0.05 (matches video default) */
        ctx->jitter_ema_us = ctx->jitter_ema_us * 0.95 + (double)abs_dev * 0.05;

        /* Publish to stats */
        atomic_store_explicit(&g_stream_stats.audio_jitter_avg_us,
                              (int64_t)ctx->jitter_ema_us, memory_order_relaxed);
    }
    ctx->last_pkt_ts = now;
    ctx->have_last_ts = true;

    /* Stats counters */
    atomic_fetch_add_explicit(&g_stream_stats.audio_bytes, (uint64_t)samples_count, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_stream_stats.audio_packets, 1, memory_order_relaxed);

    /* ── Passthrough mode (fast profile, target_depth=0) ───────── */
    if (ctx->target_depth == 0)
    {
        SS4S_PlayerAudioFeed(ctx->player, buf, (size_t)samples_count);
        return;
    }

    /* ── Buffered mode: enqueue then drain aged packets ────────── */
    ring_enqueue(ctx, buf, (size_t)samples_count, &now);

    /* Drain: feed any packet that has been held long enough */
    AudioRingSlot *oldest;
    while ((oldest = ring_peek(ctx)) != NULL)
    {
        int64_t age_us = audio_ts_diff_us(&now, &oldest->ts);
        if (age_us >= ctx->hold_us || ctx->ring_count > AUDIO_RING_SLOTS - 1)
        {
            SS4S_PlayerAudioFeed(ctx->player, oldest->data, oldest->len);
            ring_dequeue(ctx);
        }
        else
        {
            break;
        }
    }
}

AudioContext *audio_init(SS4S_Player *player, const PerfProfile *profile)
{
    AudioContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->player = player;
    ctx->target_depth = profile->audio_buf_depth;
    ctx->expected_interval_us = 10000; /* updated in header_cb */
    ctx->hold_us = (int64_t)ctx->target_depth * ctx->expected_interval_us;
    app_log("[AUDIO] init: buf_depth=%d hold=%lldus\n",
            ctx->target_depth, (long long)ctx->hold_us);
    return ctx;
}

void audio_fini(AudioContext *ctx)
{
    if (!ctx)
        return;

    /* Flush remaining buffered packets */
    if (ctx->opened)
    {
        AudioRingSlot *s;
        while ((s = ring_peek(ctx)) != NULL)
        {
            SS4S_PlayerAudioFeed(ctx->player, s->data, s->len);
            ring_dequeue(ctx);
        }
        SS4S_PlayerAudioClose(ctx->player);
    }
    free(ctx);
}

ChiakiAudioSink audio_make_sink(AudioContext *ctx)
{
    ChiakiAudioSink sink;
    memset(&sink, 0, sizeof(sink));
    sink.header_cb = audio_header_cb;
    sink.frame_cb = audio_frame_cb;
    sink.user = ctx;
    return sink;
}
