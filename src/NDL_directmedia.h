/*
 * NDL_directmedia.h — combined stub header for webOS NDL DirectMedia API
 *
 * Covers both the v1 API (webOS 4 and older, NDL_DirectMediaInit/Fini) and
 * the v2 API (webOS 5+, NDL_DirectMedia_DL_Initialize dynamic loading).
 *
 * The real shared library (libndl-directmedia.so or libndl-media.so) lives
 * on the TV itself. At build time we link against the stub .so in the webOS
 * sysroot, or use dlopen at runtime via the v2 DL API.
 *
 * Reconstructed from:
 *   https://www.webosbrew.org/webos-userland/  (webosbrew/webos-userland)
 *   mariotaku/moonlight-tv and mariotaku/ss4s source references
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#include <stdint.h>

/* ── Video codec types ─────────────────────────────────────────────────────── */

typedef enum {
    NDL_DIRECTMEDIA_VIDEO_TYPE_NONE  = -1,
    NDL_DIRECTMEDIA_VIDEO_TYPE_H264  = 0,
    NDL_DIRECTMEDIA_VIDEO_TYPE_H265  = 1,  /* HEVC — PS5 default */
    NDL_DIRECTMEDIA_VIDEO_TYPE_VP9   = 2,
    NDL_DIRECTMEDIA_VIDEO_TYPE_AV1   = 3,
} NDL_DIRECTMEDIA_VIDEO_TYPE;

/* ── Audio codec types ─────────────────────────────────────────────────────── */

typedef enum {
    NDL_DIRECTMEDIA_AUDIO_TYPE_NONE  = -1,
    NDL_DIRECTMEDIA_AUDIO_TYPE_PCM   = 0,  /* raw PCM — chiaki decodes Opus for us */
    NDL_DIRECTMEDIA_AUDIO_TYPE_AC3   = 1,
    NDL_DIRECTMEDIA_AUDIO_TYPE_EAC3  = 2,
    NDL_DIRECTMEDIA_AUDIO_TYPE_AAC   = 3,
    NDL_DIRECTMEDIA_AUDIO_TYPE_OPUS  = 5,
} NDL_DIRECTMEDIA_AUDIO_TYPE;

/* ── Pipeline load/unload info ─────────────────────────────────────────────── */

typedef struct {
    /* Video */
    NDL_DIRECTMEDIA_VIDEO_TYPE video_type;
    unsigned int               width;
    unsigned int               height;

    /* Audio */
    NDL_DIRECTMEDIA_AUDIO_TYPE audio_type;
    unsigned int               sample_rate;  /* e.g. 48000 */
    unsigned int               channels;     /* e.g. 2 */
    unsigned int               bit_depth;    /* e.g. 16 */

    /* Reserved / platform-specific fields */
    int                        framerate;    /* optional hint, 0 = unknown */
    int                        reserved[8];
} NDL_DIRECTMEDIA_DATA_INFO_T;

/* ── Pipeline status ───────────────────────────────────────────────────────── */

typedef enum {
    NDL_DIRECTMEDIA_STATUS_LOAD_COMPLETED   =  0,
    NDL_DIRECTMEDIA_STATUS_UNLOAD_COMPLETED =  1,
    NDL_DIRECTMEDIA_STATUS_PLAYING          =  2,
    NDL_DIRECTMEDIA_STATUS_PAUSED           =  3,
    NDL_DIRECTMEDIA_STATUS_EOS              =  4,
    NDL_DIRECTMEDIA_STATUS_ERROR            = -1,
} NDL_DIRECTMEDIA_STATUS;

typedef void (*NDLMediaLoadCallback)(NDL_DIRECTMEDIA_STATUS status);

/* ── PCM effect info (for NDL_DirectEffectLoad) ────────────────────────────── */

typedef struct {
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int bit_depth;
} NDL_DIRECTAUDIO_PCM_INFO_T;

/* ══════════════════════════════════════════════════════════════════════════════
 * v1 API — webOS 4 and older
 * (NDL_DirectMediaInit / NDL_DirectMediaFini)
 * ══════════════════════════════════════════════════════════════════════════════ */

/* Initialise the media pipeline; callback is invoked on status changes.
 * appId must match your appinfo.json "id" field. */
int  NDL_DirectMediaInit(const char *appId, NDLMediaLoadCallback callback);

/* ══════════════════════════════════════════════════════════════════════════════
 * v1/v2 shared pipeline API
 * ══════════════════════════════════════════════════════════════════════════════ */

/* Configure and start the A/V pipeline (async — wait for LOAD_COMPLETED). */
int  NDL_DirectMediaLoad(NDL_DIRECTMEDIA_DATA_INFO_T *info);

/* Stop the pipeline. */
int  NDL_DirectMediaUnload(void);

/* ── Video ──────────────────────────────────────────────────────────────────── */

/* Feed one compressed video buffer (NAL unit / access unit) to the hardware
 * decoder.  pts is in microseconds; pass 0 for low-latency streaming. */
int  NDL_DirectVideoPlay(void *buffer, unsigned int size, long long pts);

/* Flush the render buffer. */
int  NDL_DirectVideoFlushRenderBuffer(void);

/* Returns the current render buffer depth (frames). */
int  NDL_DirectVideoGetRenderBufferLength(int *length);

/* Drop threshold hint (frames). */
int  NDL_DirectVideoSetFrameDropThreshold(int threshold);

/* ── Audio ──────────────────────────────────────────────────────────────────── */

/* Feed decoded PCM (or other audio) to the hardware audio output.
 * pts is in microseconds; pass 0 for low-latency streaming. */
int  NDL_DirectAudioPlay(void *buffer, unsigned int size, long long pts);

/* Returns available space in NDL's audio buffer (bytes). */
int  NDL_DirectAudioGetAvailableBufferSize(int *available);

/* Returns total NDL audio buffer size (bytes). */
int  NDL_DirectAudioGetTotalBufferSize(int *total);

/* Returns 1 if multi-channel (5.1+) audio is supported. */
int  NDL_DirectAudioSupportMultiChannel(int *isSupported);

/* ── PCM effect (secondary audio path, optional) ───────────────────────────── */

int  NDL_DirectEffectLoad(NDL_DIRECTAUDIO_PCM_INFO_T *info, unsigned int *preferredSize);
int  NDL_DirectEffectPlay(void *buffer, unsigned int size);
int  NDL_DirectEffectUnload(void);
int  NDL_DirectEffectGetAvailableBufferSize(unsigned int *avail);

/* ══════════════════════════════════════════════════════════════════════════════
 * v2 API — webOS 5+ dynamic-load wrapper
 * Use NDL_DirectMedia_DL_Initialize() instead of NDL_DirectMediaInit() when
 * you want the library to be loaded via dlopen at runtime (recommended for
 * cross-version compatibility).
 * ══════════════════════════════════════════════════════════════════════════════ */

/* Dynamically loads libndl-directmedia.so (or libndl-media.so).
 * Returns true on success.  Must be called before any other NDL function. */
bool NDL_DirectMedia_DL_Initialize(void);

/* Unloads the dynamic library.  Call after NDL_DirectMediaUnload(). */
void NDL_DirectMedia_DL_Finalize(void);

/* Returns true if NDL_DirectMedia_DL_Initialize() has been called
 * successfully and the library is still loaded. */
bool NDL_DirectMedia_DL_IsInitialized(void);

#ifdef __cplusplus
}
#endif
