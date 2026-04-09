#pragma once
#include <stdbool.h>

// ── Performance mode presets ────────────────────────────────────────────────
typedef enum
{
    PERF_MODE_FAST,     // lowest latency, aggressive frame dropping
    PERF_MODE_BALANCED, // default — one-frame cushion, adaptive tuner
    PERF_MODE_SAFE,     // smooth output, tolerates bad networks
} PerfMode;

typedef struct
{
    PerfMode mode;
    int ndl_drop_threshold;  // NDL SetFrameDropThreshold value
    int max_buf_depth;       // initial max NDL render buffer depth (frames)
    double jitter_ema_alpha; // EMA smoothing factor (higher = faster reaction)
    int auto_tune_min;       // auto-tuner lower bound for buf depth
    int auto_tune_max;       // auto-tuner upper bound for buf depth
    int chord_window_ms;     // Select+Start chord detection window
    int loop_interval_ns;    // main loop pacer interval (nanoseconds)
} PerfProfile;

PerfProfile perf_profile_from_name(const char *name);

typedef struct
{
    char *host;
    char *psn_account_id_b64;
    char *registered_key_b64;
    char *rp_key_b64;
    int rp_key_type;
    int video_width;
    int video_height;
    int video_fps;
    int video_bitrate;
    bool ps5;
    bool hw_decode;
    char *video_codec; // "h265" (default), "h265_hdr" (PS5 HDR/HEVC), or "h264"
    int audio_volume;
    bool wakeup;
    char *ps5_mac;
    int wakeup_delay_ms;
    bool sleep_on_exit;
    char *ss4s_module;
    int log_level; // chiaki bitmask: DEBUG=1 VERBOSE=2 INFO=4 WARNING=8 ERROR=16
    // PSN cloud wakeup — needed when PS5 ignores local UDP wakeup (port 987).
    // Copy from: %APPDATA%\Roaming\Chiaki\Chiaki.conf  →  psn_refresh_token = v3.xxxxx
    char *psn_refresh_token; // long-lived OAuth2 refresh token (optional)
    char *perf_mode;         // "fast", "balanced" (default), or "safe"
} AppConfig;

int config_load(AppConfig *cfg, const char *path);
void config_free(AppConfig *cfg);