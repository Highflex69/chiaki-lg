#pragma once
/* Shared logging helpers — defined in main.c, used by audio.c, video.c, etc.
 *
 * app_log()        — INFO-gated: suppressed if log_level doesn't include INFO.
 *                    Use for high-volume per-frame/session chatter.
 * app_log_always() — Unconditional: always writes regardless of log_level.
 *                    Use for wakeup/probe status, session lifecycle events,
 *                    and anything critical to diagnosing failures.
 */
void app_log(const char *fmt, ...);
void app_log_always(const char *fmt, ...);
