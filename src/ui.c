#include "ui.h"
#include "config.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "config_import.h"

// ── Layout ────────────────────────────────────────────────────────────────────

#define SCREEN_W    1920
#define SCREEN_H    1080
#define CARD_X      280
#define CARD_Y      160
#define CARD_W      1360
#define CARD_H      760
#define CARD_PAD    60
#define CORNER_R    18

// ── Colours (R,G,B,A) ─────────────────────────────────────────────────────────

#define COL_BG          0x0d, 0x0d, 0x0d, 0xff
#define COL_CARD        0x1a, 0x1a, 0x2e, 0xff
#define COL_ACCENT      0x00, 0x37, 0x91, 0xff   // PlayStation blue
#define COL_BORDER      0x22, 0x22, 0x3a, 0xff
#define COL_TEXT        0xff, 0xff, 0xff, 0xff
#define COL_TEXT_DIM    0x99, 0x99, 0xbb, 0xff
#define COL_TEXT_FAINT  0x55, 0x55, 0x77, 0xff
#define COL_CODE_BG     0x11, 0x11, 0x22, 0xff
#define COL_CODE_TEXT   0x7e, 0xc8, 0xe3, 0xff   // light blue for code

// ── Minimal 5×7 pixel font (printable ASCII 0x20–0x7e) ───────────────────────

static const uint8_t FONT_DATA[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0x20 space
    {0x00,0x00,0x5f,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7f,0x14,0x7f,0x14}, // #
    {0x24,0x2a,0x7f,0x2a,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1c,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1c,0x00}, // )
    {0x08,0x2a,0x1c,0x2a,0x08}, // *
    {0x08,0x08,0x3e,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3e,0x51,0x49,0x45,0x3e}, // 0
    {0x00,0x42,0x7f,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4b,0x31}, // 3
    {0x18,0x14,0x12,0x7f,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3c,0x4a,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1e}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x08,0x14,0x22,0x41,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x00,0x41,0x22,0x14,0x08}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3e}, // @
    {0x7e,0x11,0x11,0x11,0x7e}, // A
    {0x7f,0x49,0x49,0x49,0x36}, // B
    {0x3e,0x41,0x41,0x41,0x22}, // C
    {0x7f,0x41,0x41,0x22,0x1c}, // D
    {0x7f,0x49,0x49,0x49,0x41}, // E
    {0x7f,0x09,0x09,0x09,0x01}, // F
    {0x3e,0x41,0x49,0x49,0x7a}, // G
    {0x7f,0x08,0x08,0x08,0x7f}, // H
    {0x00,0x41,0x7f,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3f,0x01}, // J
    {0x7f,0x08,0x14,0x22,0x41}, // K
    {0x7f,0x40,0x40,0x40,0x40}, // L
    {0x7f,0x02,0x0c,0x02,0x7f}, // M
    {0x7f,0x04,0x08,0x10,0x7f}, // N
    {0x3e,0x41,0x41,0x41,0x3e}, // O
    {0x7f,0x09,0x09,0x09,0x06}, // P
    {0x3e,0x41,0x51,0x21,0x5e}, // Q
    {0x7f,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7f,0x01,0x01}, // T
    {0x3f,0x40,0x40,0x40,0x3f}, // U
    {0x1f,0x20,0x40,0x20,0x1f}, // V
    {0x3f,0x40,0x38,0x40,0x3f}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x7f,0x41,0x41,0x00}, // [
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7f,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04}, // ^
    {0x40,0x40,0x40,0x40,0x40}, // _
    {0x00,0x01,0x02,0x04,0x00}, // `
    {0x20,0x54,0x54,0x54,0x78}, // a
    {0x7f,0x48,0x44,0x44,0x38}, // b
    {0x38,0x44,0x44,0x44,0x20}, // c
    {0x38,0x44,0x44,0x48,0x7f}, // d
    {0x38,0x54,0x54,0x54,0x18}, // e
    {0x08,0x7e,0x09,0x01,0x02}, // f
    {0x0c,0x52,0x52,0x52,0x3e}, // g
    {0x7f,0x08,0x04,0x04,0x78}, // h
    {0x00,0x44,0x7d,0x40,0x00}, // i
    {0x20,0x40,0x44,0x3d,0x00}, // j
    {0x7f,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7f,0x40,0x00}, // l
    {0x7c,0x04,0x18,0x04,0x78}, // m
    {0x7c,0x08,0x04,0x04,0x78}, // n
    {0x38,0x44,0x44,0x44,0x38}, // o
    {0x7c,0x14,0x14,0x14,0x08}, // p
    {0x08,0x14,0x14,0x18,0x7c}, // q
    {0x7c,0x08,0x04,0x04,0x08}, // r
    {0x48,0x54,0x54,0x54,0x20}, // s
    {0x04,0x3f,0x44,0x40,0x20}, // t
    {0x3c,0x40,0x40,0x20,0x7c}, // u
    {0x1c,0x20,0x40,0x20,0x1c}, // v
    {0x3c,0x40,0x30,0x40,0x3c}, // w
    {0x44,0x28,0x10,0x28,0x44}, // x
    {0x0c,0x50,0x50,0x50,0x3c}, // y
    {0x44,0x64,0x54,0x4c,0x44}, // z
    {0x00,0x08,0x36,0x41,0x00}, // {
    {0x00,0x00,0x7f,0x00,0x00}, // |
    {0x00,0x41,0x36,0x08,0x00}, // }
    {0x10,0x08,0x08,0x10,0x08}, // ~
};

// ── Drawing helpers ───────────────────────────────────────────────────────────

static void set_color(SDL_Renderer *r, int R, int G, int B, int A)
{
    SDL_SetRenderDrawColor(r, R, G, B, A);
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h)
{
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

static void draw_glyph(SDL_Renderer *r, int x, int y, uint8_t ch, int scale)
{
    if (ch < 0x20 || ch > 0x7e) return;
    const uint8_t *col = FONT_DATA[ch - 0x20];
    for (int cx = 0; cx < 5; cx++)
        for (int cy = 0; cy < 7; cy++)
            if (col[cx] & (1 << cy))
                fill_rect(r, x + cx * scale, y + cy * scale, scale, scale);
}

static void draw_text(SDL_Renderer *r, int x, int y, const char *text, int scale)
{
    int ox = x;
    for (const char *p = text; *p; p++)
    {
        if (*p == '\n') { x = ox; y += 9 * scale; continue; }
        draw_glyph(r, x, y, (uint8_t)*p, scale);
        x += 6 * scale;
    }
}

static int text_width(const char *text, int scale)
{
    int w = 0;
    for (const char *p = text; *p && *p != '\n'; p++)
        w += 6 * scale;
    return w;
}

static void draw_text_centered(SDL_Renderer *r, int cx, int y,
                                const char *text, int scale)
{
    draw_text(r, cx - text_width(text, scale) / 2, y, text, scale);
}

static void fill_rounded_rect(SDL_Renderer *r, int x, int y, int w, int h, int radius)
{
    fill_rect(r, x + radius, y,          w - radius * 2, h);
    fill_rect(r, x,          y + radius, w,              h - radius * 2);
}

static void draw_code_block(SDL_Renderer *r, int x, int y, int w,
                             const char *text, int scale)
{
    int lines = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') lines++;
    int h = lines * 9 * scale + 20;

    set_color(r, COL_CODE_BG);
    fill_rounded_rect(r, x, y, w, h, 8);

    set_color(r, COL_ACCENT);
    fill_rect(r, x, y, 4, h);

    set_color(r, COL_CODE_TEXT);
    draw_text(r, x + 18, y + 10, text, scale);
}

// ── Main screen renderer ──────────────────────────────────────────────────────

static void draw_button(SDL_Renderer *r, int x, int y, int w, int h,
                        const char *label, bool focused, bool enabled)
{
    if (!enabled) {
        set_color(r, COL_BORDER);
        fill_rounded_rect(r, x, y, w, h, 10);
        set_color(r, COL_TEXT_FAINT);
        draw_text_centered(r, x + w/2, y + (h/2) - 9, label, 2);
        return;
    }

    if (focused) {
        set_color(r, COL_ACCENT);
        fill_rounded_rect(r, x, y, w, h, 10);
        set_color(r, COL_TEXT);
        draw_text_centered(r, x + w/2, y + (h/2) - 9, label, 2);
    } else {
        set_color(r, COL_CODE_BG);
        fill_rounded_rect(r, x, y, w, h, 10);
        set_color(r, COL_BORDER);
        SDL_Rect rr = {x, y, w, h};
        SDL_RenderDrawRect(r, &rr);
        set_color(r, COL_TEXT);
        draw_text_centered(r, x + w/2, y + (h/2) - 9, label, 2);
    }
}

static void draw_input_box(SDL_Renderer *r, int x, int y, int w, int h,
                           const char *text, int cursor, bool focused)
{
    set_color(r, COL_CODE_BG);
    fill_rounded_rect(r, x, y, w, h, 10);

    if (focused) set_color(r, COL_ACCENT);
    else         set_color(r, COL_BORDER);
    SDL_Rect rr = {x, y, w, h};
    SDL_RenderDrawRect(r, &rr);

    int scale = 3;
    int tx = x + 18;
    int ty = y + (h - (7 * scale)) / 2;

    if (text && text[0]) {
        set_color(r, COL_CODE_TEXT);
        draw_text(r, tx, ty, text, scale);
    } else {
        set_color(r, COL_TEXT_FAINT);
        draw_text(r, tx, ty, "192.168.1.50", scale);
    }

    if (focused && ((SDL_GetTicks() / 500) % 2) == 0) {
        int cx = tx + cursor * (6 * scale);
        set_color(r, COL_TEXT);
        draw_text(r, cx, ty, "|", scale);
    }
}

static void render_setup_screen(SDL_Renderer *r, const char *status,
                                const char *ip, int focus,
                                const char *msg,
                                bool can_import,
                                bool can_connect,
                                bool show_keypad)
{
    int cx = SCREEN_W / 2;

    // Background
    set_color(r, COL_BG);
    fill_rect(r, 0, 0, SCREEN_W, SCREEN_H);

    // Card
    set_color(r, COL_CARD);
    fill_rounded_rect(r, CARD_X, CARD_Y, CARD_W, CARD_H, CORNER_R);

    // Accent bar at top of card
    set_color(r, COL_ACCENT);
    fill_rect(r, CARD_X, CARD_Y, CARD_W, 6);

    // Title
    set_color(r, COL_TEXT);
    draw_text_centered(r, cx, CARD_Y + 36, "Chiaki - Connect", 3);

    // Divider
    set_color(r, COL_BORDER);
    fill_rect(r, CARD_X + CARD_PAD, CARD_Y + 80, CARD_W - CARD_PAD * 2, 1);

    int fx = CARD_X + CARD_PAD;
    int fy = CARD_Y + 104;

    set_color(r, COL_TEXT_DIM);
    draw_text(r, fx, fy, "Import chiaki-ng config + connect", 2);

    set_color(r, COL_ACCENT);
    draw_text(r, fx, fy + 34, "1. Copy chiaki-ng-Default.ini to the TV", 2);
    set_color(r, COL_TEXT_DIM);
    draw_text(r, fx, fy + 56, "Export/copy it from chiaki-ng on your PC, then push it to the TV using ADB or WebOS Dev Manager", 2);
    draw_code_block(r, fx, fy + 78, CARD_W - CARD_PAD * 2,
        "/media/developer/apps/usr/palm/applications/org.homebrew.chiaki/chiaki-ng-Default.ini",
        2);

    set_color(r, COL_ACCENT);
    draw_text(r, fx, fy + 178, "2. Enter your PS5 LAN IP address", 2);
    set_color(r, COL_TEXT_DIM);
    draw_text(r, fx, fy + 200, "PS5: Settings > Network > Connection Status > View Connection Status", 2);

    // IP input
    int ip_y = fy + 242;
    set_color(r, COL_TEXT);
    draw_text(r, fx, ip_y, "PS5 IP Address:", 2);
    int cursor = (int)(ip ? strlen(ip) : 0);
    bool ip_focused = (focus == 0) || show_keypad;
    draw_input_box(r, fx + 320, ip_y - 14, 520, 64, ip, cursor, ip_focused);

    // When the IP field is "entered", show a remote-friendly numeric keypad.
    // Otherwise, keep the page clean.
    const int btn_w = 360;
    const int btn_h = 72;
    int btn_y = ip_y + 92;

    if (show_keypad)
    {
        // On-screen numeric keypad (for TV remote)
        // Focus mapping (when keypad shown): 1..12=keys, 15=Done
        static const char *keys[12] = {
            "1","2","3",
            "4","5","6",
            "7","8","9",
            ".","0","DEL"
        };
        const int key_w = 110;
        const int key_h = 56;
        const int key_gap = 16;
        const int cols = 3;
        const int rows = 4;
        int keypad_x = fx + 320;
        int keypad_y = ip_y + 70;

        set_color(r, COL_TEXT_DIM);
        draw_text(r, keypad_x, keypad_y - 30, "Use keypad to enter IP (remote-friendly)", 2);

        for (int i = 0; i < 12; i++)
        {
            int row = i / cols;
            int col = i % cols;
            int x = keypad_x + col * (key_w + key_gap);
            int y = keypad_y + row * (key_h + key_gap);
            bool kfocus = (focus == (1 + i));
            draw_button(r, x, y, key_w, key_h, keys[i], kfocus, true);
        }

        int keypad_h = rows * key_h + (rows - 1) * key_gap;
        btn_y = keypad_y + keypad_h + 26;

        // Done (closes keypad and returns to launcher)
        int done_w = cols * key_w + (cols - 1) * key_gap;
        draw_button(r, fx + 320, btn_y, done_w, btn_h, "Done", focus == 15, true);

        // Message (still show status/errors while keypad is open)
        if (msg && msg[0]) {
            set_color(r, COL_TEXT_DIM);
            draw_text(r, fx, btn_y + btn_h + 18, msg, 2);
        }
    }
    else
    {
        // Main buttons
        draw_button(r, fx, btn_y, btn_w, btn_h, "Import Config", focus == 13, can_import);
        draw_button(r, fx + btn_w + 40, btn_y, btn_w, btn_h, "Connect", focus == 14, can_connect);

        // Settings button (non-connection options)
        int settings_y = btn_y + btn_h + 18;
        draw_button(r, fx, settings_y, btn_w * 2 + 40, btn_h, "Settings", focus == 16, true);

        // Message
        if (msg && msg[0]) {
            set_color(r, COL_TEXT_DIM);
            draw_text(r, fx, settings_y + btn_h + 18, msg, 2);
        }
    }

    // Hint bar
    set_color(r, COL_BORDER);
    fill_rect(r, 0, SCREEN_H - 56, SCREEN_W, 56);
    set_color(r, COL_TEXT_FAINT);
    if (show_keypad) {
        draw_text_centered(r, cx, SCREEN_H - 38,
            "ARROWS: Keypad   OK: Input   BACK: Done", 2);
    } else {
        draw_text_centered(r, cx, SCREEN_H - 38,
            "ARROWS: Navigate   OK: Select   BACK: Exit", 2);
    }

    // Key status debug line
    set_color(r, COL_ACCENT);
    draw_text(r, CARD_X + CARD_PAD, SCREEN_H - 80, status, 1);
}

// ── Settings screen ─────────────────────────────────────────────────────────

typedef enum {
    SET_RESOLUTION = 0,
    SET_FPS,
    SET_BITRATE,
    SET_CODEC,
    SET_PS5,
    SET_HW_DECODER,
    SET_WAKE_ON_START,
    SET_SLEEP_ON_EXIT,
    SET_LOG_LEVEL,
    SET_RETURN,
    SET_COUNT
} SettingsFocus;

typedef struct {
    int w;
    int h;
    const char *label;
} ResOption;

static const ResOption RES_OPTIONS[] = {
    {1280, 720,  "720p"},
    {1920, 1080, "1080p"},
    {2560, 1440, "1440p"},
    {3840, 2160, "2160p"},
};

static const int FPS_OPTIONS[] = {30, 60};
static const int BITRATE_OPTIONS[] = {5000, 10000, 15000, 25000, 35000, 45000};
static const char *CODEC_OPTIONS[] = {"h264", "h265", "h265_hdr"};
static const char *CODEC_LABELS[]  = {"H.264 (AVC)", "H.265 (HEVC)", "H.265 HDR (HEVC)"};
static const char *LOG_LEVEL_STRS[] = {"off","error","warning","info","verbose","debug"};
static const char *LOG_LEVEL_LABELS[] = {"Off","Error","Warning","Info","Verbose","Debug"};

static const char *ui_log_level_to_string(int mask)
{
    if (mask == 0) return "off";
    if (mask == 16) return "error";
    if ((mask & (16|8|4|2|1)) == (16|8|4|2|1)) return "debug";
    if ((mask & (16|8|4|2)) == (16|8|4|2)) return "verbose";
    if ((mask & (16|8|4)) == (16|8|4)) return "info";
    if ((mask & (16|8)) == (16|8)) return "warning";
    return "warning";
}

static int ui_log_level_from_string(const char *s)
{
    if (!s || !s[0] || !strcmp(s, "off")) return 0;
    if (!strcmp(s, "error")) return 16;
    if (!strcmp(s, "warning")) return 16|8;
    if (!strcmp(s, "info")) return 16|8|4;
    if (!strcmp(s, "verbose")) return 16|8|4|2;
    if (!strcmp(s, "debug")) return 16|8|4|2|1;
    return 16|8;
}

static int ui_find_resolution_idx(const AppConfig *cfg)
{
    int w = cfg ? cfg->video_width : 1920;
    int h = cfg ? cfg->video_height : 1080;
    for (int i = 0; i < (int)(sizeof(RES_OPTIONS)/sizeof(RES_OPTIONS[0])); i++)
        if (RES_OPTIONS[i].w == w && RES_OPTIONS[i].h == h)
            return i;
    return 1; // default 1080p
}

static int ui_find_fps_idx(const AppConfig *cfg)
{
    int fps = cfg ? cfg->video_fps : 60;
    for (int i = 0; i < (int)(sizeof(FPS_OPTIONS)/sizeof(FPS_OPTIONS[0])); i++)
        if (FPS_OPTIONS[i] == fps)
            return i;
    return 1; // default 60
}

static int ui_find_bitrate_idx(const AppConfig *cfg)
{
    int br = cfg ? cfg->video_bitrate : 15000;
    for (int i = 0; i < (int)(sizeof(BITRATE_OPTIONS)/sizeof(BITRATE_OPTIONS[0])); i++)
        if (BITRATE_OPTIONS[i] == br)
            return i;
    // pick nearest
    int best = 2;
    int best_diff = 1000000;
    for (int i = 0; i < (int)(sizeof(BITRATE_OPTIONS)/sizeof(BITRATE_OPTIONS[0])); i++) {
        int d = BITRATE_OPTIONS[i] - br;
        if (d < 0) d = -d;
        if (d < best_diff) { best_diff = d; best = i; }
    }
    return best;
}

static int ui_find_codec_idx(const AppConfig *cfg)
{
    const char *c = (cfg && cfg->video_codec) ? cfg->video_codec : "h265";
    for (int i = 0; i < 3; i++)
        if (!strcmp(CODEC_OPTIONS[i], c))
            return i;
    return 1;
}

static int ui_find_log_level_idx(const AppConfig *cfg)
{
    const char *s = ui_log_level_to_string(cfg ? cfg->log_level : (16|8));
    for (int i = 0; i < 6; i++)
        if (!strcmp(LOG_LEVEL_STRS[i], s))
            return i;
    return 2; // warning
}

static void ui_draw_option_row(SDL_Renderer *r, int x, int y, int w, int h,
                               const char *label, const char *value, bool focused)
{
    set_color(r, COL_CODE_BG);
    fill_rounded_rect(r, x, y, w, h, 10);
    if (focused) set_color(r, COL_ACCENT);
    else         set_color(r, COL_BORDER);
    SDL_Rect rr = {x, y, w, h};
    SDL_RenderDrawRect(r, &rr);

    int scale = 2;
    int ty = y + (h - (7 * scale)) / 2;
    set_color(r, COL_TEXT);
    draw_text(r, x + 18, ty, label, scale);

    if (value) {
        set_color(r, focused ? COL_TEXT : COL_TEXT_DIM);
        int vw = text_width(value, scale);
        draw_text(r, x + w - 18 - vw, ty, value, scale);
    }
}

static void ui_render_settings_screen(SDL_Renderer *r,
                                      const AppConfig *cfg,
                                      int focus,
                                      const char *msg,
                                      int res_idx,
                                      int fps_idx,
                                      int br_idx,
                                      int codec_idx,
                                      int log_idx)
{
    int cx = SCREEN_W / 2;
    set_color(r, COL_BG);
    fill_rect(r, 0, 0, SCREEN_W, SCREEN_H);

    set_color(r, COL_CARD);
    fill_rounded_rect(r, CARD_X, CARD_Y, CARD_W, CARD_H, CORNER_R);
    set_color(r, COL_ACCENT);
    fill_rect(r, CARD_X, CARD_Y, CARD_W, 6);

    // Header
    set_color(r, COL_TEXT);
    draw_text_centered(r, cx, CARD_Y + 34, "Settings", 3);
    set_color(r, COL_BORDER);
    fill_rect(r, CARD_X + CARD_PAD, CARD_Y + 74, CARD_W - CARD_PAD * 2, 1);

    // Compact layout: keep all rows + Return comfortably within the card
    int fx = CARD_X + CARD_PAD;
    int y  = CARD_Y + 92;
    set_color(r, COL_TEXT_DIM);
    draw_text(r, fx, y, "Streaming options (saved to config.json)", 2);

    int row_x = fx;
    int row_w = CARD_W - CARD_PAD * 2;
    int row_h = 54;
    int row_gap = 10;
    y += 26;

    char vbuf[128];

    // Resolution
    snprintf(vbuf, sizeof(vbuf), "%s (%dx%d)", RES_OPTIONS[res_idx].label, RES_OPTIONS[res_idx].w, RES_OPTIONS[res_idx].h);
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Resolution", vbuf, focus == SET_RESOLUTION);
    y += row_h + row_gap;

    // FPS
    snprintf(vbuf, sizeof(vbuf), "%d", FPS_OPTIONS[fps_idx]);
    ui_draw_option_row(r, row_x, y, row_w, row_h, "FPS", vbuf, focus == SET_FPS);
    y += row_h + row_gap;

    // Bitrate
    snprintf(vbuf, sizeof(vbuf), "%d kbps", BITRATE_OPTIONS[br_idx]);
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Bitrate", vbuf, focus == SET_BITRATE);
    y += row_h + row_gap;

    // Codec
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Codec", CODEC_LABELS[codec_idx], focus == SET_CODEC);
    y += row_h + row_gap;

    // PS5
    ui_draw_option_row(r, row_x, y, row_w, row_h, "PS5", (cfg && cfg->ps5) ? "On" : "Off", focus == SET_PS5);
    y += row_h + row_gap;

    // HW decoder
    ui_draw_option_row(r, row_x, y, row_w, row_h, "HW Decoder (NOT USED)", (cfg && cfg->hw_decode) ? "On" : "Off", focus == SET_HW_DECODER);
    y += row_h + row_gap;

    // Wake on start
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Wake on Start", (cfg && cfg->wakeup) ? "On" : "Off", focus == SET_WAKE_ON_START);
    y += row_h + row_gap;

    // Sleep on exit
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Sleep on Exit", (cfg && cfg->sleep_on_exit) ? "On" : "Off", focus == SET_SLEEP_ON_EXIT);
    y += row_h + row_gap;

    // Log level
    ui_draw_option_row(r, row_x, y, row_w, row_h, "Log Level", LOG_LEVEL_LABELS[log_idx], focus == SET_LOG_LEVEL);
    y += row_h + row_gap + 6;

    // Return button (keep within the card and away from the bottom hint bar)
    draw_button(r, row_x, y, row_w, 56, "Return", focus == SET_RETURN, true);

    // Message (outside the card, above the bottom hint bar)
    if (msg && msg[0]) {
        set_color(r, COL_TEXT_DIM);
        draw_text(r, row_x, CARD_Y + CARD_H + 10, msg, 2);
    }

    // Hint bar
    set_color(r, COL_BORDER);
    fill_rect(r, 0, SCREEN_H - 56, SCREEN_W, 56);
    set_color(r, COL_TEXT_FAINT);
    draw_text_centered(r, cx, SCREEN_H - 38,
        "UP/DOWN: Navigate   LEFT/RIGHT: Change   OK: Select   BACK: Return", 2);
}

// ── Minimal loading screen ──────────────────────────────────────────────────

void ui_render_loading(SDL_Renderer *r, const char *base_text)
{
    int w = SCREEN_W, h = SCREEN_H;
    (void)SDL_GetRendererOutputSize(r, &w, &h);

    // Animated dots (0..3)
    int dots = (int)((SDL_GetTicks() / 350) % 4);
    char msg[256];
    if (!base_text) base_text = "Loading";
    snprintf(msg, sizeof(msg), "%s%.*s", base_text, dots, "...");

    // Opaque background so it is visible before the NDL plane starts.
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    // Simple centered text
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    int scale = 4;
    draw_text_centered(r, w / 2, (h / 2) - (9 * scale), msg, scale);

    SDL_SetRenderDrawColor(r, 160, 160, 160, 255);
    draw_text_centered(r, w / 2, (h / 2) + (9 * scale), "Please wait", 2);
}


// ── Stats overlay (drawn on top of video plane) ─────────────────────────────

void ui_render_stats_overlay(SDL_Renderer *r, const char *text)
{
    if(!r || !text || !text[0])
        return;

    int w = SCREEN_W, h = SCREEN_H;
    (void)SDL_GetRendererOutputSize(r, &w, &h);

    const int scale = 2;
    const int pad = 18;
    const int line_h = 9 * scale;

    // Compute max line width and number of lines
    int lines = 1;
    int maxw = 0;
    int curw = 0;
    for(const char *p = text; *p; p++)
    {
        if(*p == '\n')
        {
            if(curw > maxw) maxw = curw;
            curw = 0;
            lines++;
            continue;
        }
        curw += 6 * scale;
    }
    if(curw > maxw) maxw = curw;

    int box_w = maxw + pad * 2;
    int box_h = lines * line_h + pad * 2;

    // Clamp box to screen
    if(box_w > w - 40) box_w = w - 40;
    if(box_h > h - 40) box_h = h - 40;

    int x = 20;
    int y = 20;

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_color(r, 0, 0, 0, 170);
    fill_rounded_rect(r, x, y, box_w, box_h, 10);

    // Accent bar
    set_color(r, COL_ACCENT);
    fill_rect(r, x, y, 4, box_h);

    // Text
    set_color(r, COL_TEXT);
    draw_text(r, x + pad, y + pad, text, scale);
}

// ── Public entry point ────────────────────────────────────────────────────────

static bool ui_has_keys(const AppConfig *cfg)
{
    return cfg
        && cfg->psn_account_id_b64 && cfg->psn_account_id_b64[0]
        && cfg->registered_key_b64 && cfg->registered_key_b64[0]
        && cfg->rp_key_b64 && cfg->rp_key_b64[0]
        && strlen(cfg->registered_key_b64) >= 8;
}

static bool ui_valid_ipv4(const char *s)
{
    if (!s || !s[0]) return false;
    struct in_addr a;
    return inet_pton(AF_INET, s, &a) == 1;
}

static bool ui_write_host(const char *config_path, const char *host)
{
    if (!config_path || !host || !host[0]) return false;

    struct json_object *root = json_object_from_file(config_path);
    if (!root)
        root = json_object_new_object();

    // overwrite host
    json_object_object_del(root, "host");
    json_object_object_add(root, "host", json_object_new_string(host));

    int rc = json_object_to_file_ext(config_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return rc == 0;
}

static bool ui_write_settings_json(const char *config_path, const AppConfig *cfg)
{
    if (!config_path || !cfg)
        return false;

    struct json_object *root = json_object_from_file(config_path);
    if (!root)
        root = json_object_new_object();

    // ints
    json_object_object_del(root, "video_width");
    json_object_object_add(root, "video_width", json_object_new_int(cfg->video_width));
    json_object_object_del(root, "video_height");
    json_object_object_add(root, "video_height", json_object_new_int(cfg->video_height));
    json_object_object_del(root, "video_fps");
    json_object_object_add(root, "video_fps", json_object_new_int(cfg->video_fps));
    json_object_object_del(root, "video_bitrate");
    json_object_object_add(root, "video_bitrate", json_object_new_int(cfg->video_bitrate));

    // bools
    json_object_object_del(root, "ps5");
    json_object_object_add(root, "ps5", json_object_new_boolean(cfg->ps5));
    json_object_object_del(root, "hw_decode");
    json_object_object_add(root, "hw_decode", json_object_new_boolean(cfg->hw_decode));
    json_object_object_del(root, "wakeup");
    json_object_object_add(root, "wakeup", json_object_new_boolean(cfg->wakeup));
    json_object_object_del(root, "sleep_on_exit");
    json_object_object_add(root, "sleep_on_exit", json_object_new_boolean(cfg->sleep_on_exit));

    // codec
    const char *codec = (cfg->video_codec && cfg->video_codec[0]) ? cfg->video_codec : "h265";
    json_object_object_del(root, "video_codec");
    json_object_object_add(root, "video_codec", json_object_new_string(codec));

    // log level (write as string)
    const char *ll = ui_log_level_to_string(cfg->log_level);
    json_object_object_del(root, "log_level");
    json_object_object_add(root, "log_level", json_object_new_string(ll));

    int rc = json_object_to_file_ext(config_path, root, JSON_C_TO_STRING_PRETTY);
    json_object_put(root);
    return rc == 0;
}

static void ui_reload_cfg(AppConfig *cfg, const char *config_path)
{
    if (!cfg) return;
    config_free(cfg);
    (void)config_load(cfg, config_path);
}

static void ui_build_ini_path(const char *config_path, char *out, size_t outlen)
{
    if (!out || outlen == 0) return;
    out[0] = '\0';

    if (!config_path) {
        snprintf(out, outlen, "chiaki-ng-Default.ini");
        return;
    }

    const char *slash = strrchr(config_path, '/');
    if (!slash) {
        snprintf(out, outlen, "chiaki-ng-Default.ini");
        return;
    }

    size_t dirlen = (size_t)(slash - config_path);
    if (dirlen + 1 >= outlen) dirlen = outlen - 2;
    memcpy(out, config_path, dirlen);
    out[dirlen] = '\0';
    snprintf(out + dirlen, outlen - dirlen, "/chiaki-ng-Default.ini");
}

static void ui_set_codec(AppConfig *cfg, const char *codec)
{
    if (!cfg) return;
    if (!codec) codec = "h265";
    if (cfg->video_codec && !strcmp(cfg->video_codec, codec))
        return;
    free(cfg->video_codec);
    cfg->video_codec = strdup(codec);
}

static void ui_apply_settings_indices(AppConfig *cfg,
                                      int res_idx,
                                      int fps_idx,
                                      int br_idx,
                                      int codec_idx,
                                      int log_idx)
{
    if (!cfg) return;
    if (res_idx < 0) res_idx = 1;
    if (res_idx >= (int)(sizeof(RES_OPTIONS)/sizeof(RES_OPTIONS[0]))) res_idx = 1;
    if (fps_idx < 0) fps_idx = 1;
    if (fps_idx >= (int)(sizeof(FPS_OPTIONS)/sizeof(FPS_OPTIONS[0]))) fps_idx = 1;
    if (br_idx < 0) br_idx = 2;
    if (br_idx >= (int)(sizeof(BITRATE_OPTIONS)/sizeof(BITRATE_OPTIONS[0]))) br_idx = 2;
    if (codec_idx < 0) codec_idx = 1;
    if (codec_idx >= 3) codec_idx = 1;
    if (log_idx < 0) log_idx = 2;
    if (log_idx >= 6) log_idx = 2;

    cfg->video_width   = RES_OPTIONS[res_idx].w;
    cfg->video_height  = RES_OPTIONS[res_idx].h;
    cfg->video_fps     = FPS_OPTIONS[fps_idx];
    cfg->video_bitrate = BITRATE_OPTIONS[br_idx];
    ui_set_codec(cfg, CODEC_OPTIONS[codec_idx]);
    cfg->log_level = ui_log_level_from_string(LOG_LEVEL_STRS[log_idx]);
}

static void ui_run_settings(SDL_Renderer *renderer, AppConfig *cfg, const char *config_path)
{
    if (!renderer || !cfg) return;

    int res_idx   = ui_find_resolution_idx(cfg);
    int fps_idx   = ui_find_fps_idx(cfg);
    int br_idx    = ui_find_bitrate_idx(cfg);
    int codec_idx = ui_find_codec_idx(cfg);
    int log_idx   = ui_find_log_level_idx(cfg);
    int focus     = SET_RESOLUTION;

    char msg[256] = {0};

    while (1)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                return;

            if (ev.type == SDL_KEYDOWN)
            {
                SDL_Keycode sym = ev.key.keysym.sym;

                // Back returns to launcher
                if (sym == SDLK_ESCAPE || sym == 1073742094 /*WEBOS_KEY_BACK*/)
                    return;

                if (sym == SDLK_UP)
                {
                    if (focus > 0) focus--;
                }
                else if (sym == SDLK_DOWN)
                {
                    if (focus < SET_RETURN) focus++;
                }
                else if (sym == SDLK_LEFT || sym == SDLK_RIGHT || sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
                {
                    bool forward = (sym == SDLK_RIGHT) || (sym == SDLK_RETURN) || (sym == SDLK_KP_ENTER) || (sym == SDLK_SPACE);
                    bool changed = false;

                    if (focus == SET_RESOLUTION) {
                        int n = (int)(sizeof(RES_OPTIONS)/sizeof(RES_OPTIONS[0]));
                        res_idx = (res_idx + (forward ? 1 : -1) + n) % n;
                        changed = true;
                    } else if (focus == SET_FPS) {
                        int n = (int)(sizeof(FPS_OPTIONS)/sizeof(FPS_OPTIONS[0]));
                        fps_idx = (fps_idx + (forward ? 1 : -1) + n) % n;
                        changed = true;
                    } else if (focus == SET_BITRATE) {
                        int n = (int)(sizeof(BITRATE_OPTIONS)/sizeof(BITRATE_OPTIONS[0]));
                        br_idx = (br_idx + (forward ? 1 : -1) + n) % n;
                        changed = true;
                    } else if (focus == SET_CODEC) {
                        int n = 3;
                        codec_idx = (codec_idx + (forward ? 1 : -1) + n) % n;
                        changed = true;
                    } else if (focus == SET_LOG_LEVEL) {
                        int n = 6;
                        log_idx = (log_idx + (forward ? 1 : -1) + n) % n;
                        changed = true;
                    } else if (focus == SET_PS5) {
                        cfg->ps5 = !cfg->ps5;
                        changed = true;
                    } else if (focus == SET_HW_DECODER) {
                        cfg->hw_decode = !cfg->hw_decode;
                        changed = true;
                    } else if (focus == SET_WAKE_ON_START) {
                        cfg->wakeup = !cfg->wakeup;
                        changed = true;
                    } else if (focus == SET_SLEEP_ON_EXIT) {
                        cfg->sleep_on_exit = !cfg->sleep_on_exit;
                        changed = true;
                    } else if (focus == SET_RETURN) {
                        return;
                    }

                    if (changed)
                    {
                        ui_apply_settings_indices(cfg, res_idx, fps_idx, br_idx, codec_idx, log_idx);
                        if (!ui_write_settings_json(config_path, cfg))
                            snprintf(msg, sizeof(msg), "Failed to write config.json");
                        else
                            snprintf(msg, sizeof(msg), "Saved.");
                    }
                }
            }
            else if (ev.type == SDL_CONTROLLERBUTTONDOWN)
            {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                    ev.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                    return;

                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    SDL_Event fake; memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN; fake.key.keysym.sym = SDLK_UP;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    SDL_Event fake; memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN; fake.key.keysym.sym = SDLK_DOWN;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    SDL_Event fake; memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN; fake.key.keysym.sym = SDLK_LEFT;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    SDL_Event fake; memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN; fake.key.keysym.sym = SDLK_RIGHT;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    SDL_Event fake; memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN; fake.key.keysym.sym = SDLK_RETURN;
                    SDL_PushEvent(&fake);
                }
            }
        }

        ui_render_settings_screen(renderer, cfg, focus, msg, res_idx, fps_idx, br_idx, codec_idx, log_idx);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

UIResult ui_run_registration(SDL_Renderer *renderer, AppConfig *cfg,
                             const char *config_path, ChiakiLog *log)
{
    (void)log;

    // IP input buffer (pre-filled from config)
    char ip[64] = {0};
    if (cfg && cfg->host && cfg->host[0])
        snprintf(ip, sizeof(ip), "%s", cfg->host);

    // UI state
    // Focus mapping:
    //   Main page:  0 = IP field, 13 = Import, 14 = Connect, 16 = Settings
    //   Keypad:     1..12 = keypad keys ("1".."9", ".", "0", "DEL"), 15 = Done
    int focus = 0;
    bool editing = false;
    char msg[256] = {0};

    // Choose an initial focus
    if (ui_valid_ipv4(ip) && ui_has_keys(cfg))
        focus = 14;

    // Keep text input enabled for physical keyboards, but don't rely on it.
    SDL_StartTextInput();

    while (1)
    {
        // Build a status string showing what keys are present/missing
        char status[256];
        snprintf(status, sizeof(status),
            "host: %s  ps5: %d  acct: %s  regkey: %s  rpkey: %s  refresh: %s",
            (cfg && cfg->host) ? cfg->host : "(null)",
            cfg ? cfg->ps5 : 1,
            (cfg && cfg->psn_account_id_b64) ? "OK" : "MISSING",
            (cfg && cfg->registered_key_b64) ? (strlen(cfg->registered_key_b64) >= 8 ? "OK" : "TOO SHORT") : "MISSING",
            (cfg && cfg->rp_key_b64) ? "OK" : "MISSING",
            (cfg && cfg->psn_refresh_token) ? "OK" : "(none)");

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                return UI_RESULT_QUIT;

            if (ev.type == SDL_KEYDOWN)
            {
                SDL_Keycode sym = ev.key.keysym.sym;

                // Quit / Back keys
                if (sym == SDLK_AC_HOME)
                    return UI_RESULT_QUIT;
                if (sym == SDLK_ESCAPE || sym == 1073742094 /*WEBOS_KEY_BACK*/)
                {
                    if (editing) {
                        editing = false;
                        focus = 0;
                        continue;
                    }
                    return UI_RESULT_QUIT;
                }

                // Navigation
                if (sym == SDLK_UP)
                {
                    if (editing)
                    {
                        if (focus >= 1 && focus <= 12)
                        {
                            int idx = focus - 1;
                            int row = idx / 3;
                            if (row > 0) focus -= 3;
                        }
                        else if (focus == 15)
                        {
                            focus = 11; // "0" key
                        }
                    }
                    else
                    {
                        if (focus == 13 || focus == 14)
                            focus = 0;
                        else if (focus == 16)
                            focus = 13;
                    }
                }
                else if (sym == SDLK_DOWN)
                {
                    if (editing)
                    {
                        if (focus >= 1 && focus <= 12)
                        {
                            int idx = focus - 1;
                            int row = idx / 3;
                            if (row == 3) focus = 15; // Done
                            else focus += 3;
                        }
                        // Done stays
                    }
                    else
                    {
                        if (focus == 0)
                            focus = 13;
                        else if (focus == 13 || focus == 14)
                            focus = 16;
                    }
                }
                else if (sym == SDLK_LEFT)
                {
                    if (editing)
                    {
                        if (focus >= 1 && focus <= 12)
                        {
                            int idx = focus - 1;
                            int col = idx % 3;
                            if (col > 0) focus -= 1;
                        }
                    }
                    else
                    {
                        if (focus == 14) focus = 13;
                    }
                }
                else if (sym == SDLK_RIGHT)
                {
                    if (editing)
                    {
                        if (focus >= 1 && focus <= 12)
                        {
                            int idx = focus - 1;
                            int col = idx % 3;
                            if (col < 2) focus += 1;
                        }
                    }
                    else
                    {
                        if (focus == 13) focus = 14;
                    }
                }

                // Physical keyboard editing (when IP field or keypad is focused)
                if (focus == 0 || editing)
                {
                    size_t len = strlen(ip);
                    if (sym == SDLK_BACKSPACE)
                    {
                        if (len > 0) ip[len - 1] = '\0';
                    }
                    else if ((sym >= SDLK_0 && sym <= SDLK_9) ||
                             (sym >= SDLK_KP_0 && sym <= SDLK_KP_9) ||
                             sym == SDLK_PERIOD || sym == SDLK_KP_PERIOD)
                    {
                        if (len + 1 < sizeof(ip))
                        {
                            char c;
                            if (sym == SDLK_PERIOD || sym == SDLK_KP_PERIOD) c = '.';
                            else if (sym >= SDLK_0 && sym <= SDLK_9) c = (char)('0' + (sym - SDLK_0));
                            else c = (char)('0' + (sym - SDLK_KP_0));

                            ip[len] = c;
                            ip[len + 1] = '\0';
                        }
                    }
                }

                // Activate
                if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
                {
                    if (editing && focus >= 1 && focus <= 12)
                    {
                        // Keypad key
                        int idx = focus - 1;
                        static const char *keys2[12] = {
                            "1","2","3","4","5","6","7","8","9",".","0","DEL"
                        };
                        const char *k = keys2[idx];

                        size_t len = strlen(ip);
                        if (!strcmp(k, "DEL"))
                        {
                            if (len > 0) ip[len - 1] = '\0';
                        }
                        else
                        {
                            if (len + 1 < sizeof(ip))
                            {
                                char c = k[0];
                                if (c == '.' || (c >= '0' && c <= '9'))
                                {
                                    ip[len] = c;
                                    ip[len + 1] = '\0';
                                }
                            }
                        }
                    }
                    else if (editing && focus == 15)
                    {
                        // Done
                        editing = false;
                        focus = 0;
                    }
                    else if (!editing && focus == 0)
                    {
                        // Enter keypad
                        editing = true;
                        focus = 1;
                    }
                    else if (!editing && focus == 13)
                    {
                        // Import
                        if (!ui_valid_ipv4(ip)) {
                            snprintf(msg, sizeof(msg), "Enter a valid PS5 IPv4 address first.");
                            focus = 0;
                            continue;
                        }

                        if (!ui_write_host(config_path, ip)) {
                            snprintf(msg, sizeof(msg), "Failed to write host into config.json");
                            continue;
                        }

                        char ini_path[512];
                        ui_build_ini_path(config_path, ini_path, sizeof(ini_path));

                        ChiakiImportResult r = config_try_import_chiaki_ini(ini_path, config_path);
                        switch (r)
                        {
                        case CI_FILE_NOT_FOUND:
                            snprintf(msg, sizeof(msg), "chiaki-ng-Default.ini not found on TV. Copy it then try again.");
                            break;
                        case CI_PARSE_ERROR:
                            snprintf(msg, sizeof(msg), "Import failed: could not parse chiaki-ng INI");
                            break;
                        case CI_WRITE_ERROR:
                            snprintf(msg, sizeof(msg), "Import failed: could not write config.json");
                            break;
                        case CI_SUCCESS:
                        case CI_SUCCESS_NEEDS_HOST:
                            snprintf(msg, sizeof(msg), "Import OK. Press Connect.");
                            ui_reload_cfg(cfg, config_path);
                            if (cfg && cfg->host && cfg->host[0])
                                snprintf(ip, sizeof(ip), "%s", cfg->host);
                            if (ui_has_keys(cfg) && ui_valid_ipv4(ip))
                                focus = 14;
                            break;
                        default:
                            snprintf(msg, sizeof(msg), "Import finished.");
                            ui_reload_cfg(cfg, config_path);
                            break;
                        }
                    }
                    else if (!editing && focus == 16)
                    {
                        // Settings
                        ui_run_settings(renderer, cfg, config_path);
                        // Reload to reflect any changes + keep memory ownership consistent
                        ui_reload_cfg(cfg, config_path);
                        if (cfg && cfg->host && cfg->host[0])
                            snprintf(ip, sizeof(ip), "%s", cfg->host);
                        snprintf(msg, sizeof(msg), "Settings updated.");
                        if (ui_has_keys(cfg) && ui_valid_ipv4(ip))
                            focus = 14;
                        else
                            focus = 0;
                    }
                    else if (!editing && focus == 14)
                    {
                        // Connect
                        if (!ui_valid_ipv4(ip)) {
                            snprintf(msg, sizeof(msg), "Enter a valid PS5 IPv4 address.");
                            focus = 0;
                            continue;
                        }

                        if (!ui_write_host(config_path, ip)) {
                            snprintf(msg, sizeof(msg), "Failed to save host into config.json");
                            continue;
                        }
                        ui_reload_cfg(cfg, config_path);

                        if (!ui_has_keys(cfg)) {
                            snprintf(msg, sizeof(msg), "Missing registration keys. Copy chiaki-ng-Default.ini and press Import Config.");
                            focus = 13;
                            continue;
                        }

                        return UI_RESULT_CONNECT;
                    }
                }
            }
            else if (ev.type == SDL_TEXTINPUT)
            {
                if (focus == 0 || editing)
                {
                    const char *t = ev.text.text;
                    for (const char *p = t; *p; p++)
                    {
                        char c = *p;
                        if (!(isdigit((unsigned char)c) || c == '.'))
                            continue;
                        size_t len = strlen(ip);
                        if (len + 1 < sizeof(ip)) {
                            ip[len] = c;
                            ip[len + 1] = '\0';
                        }
                    }
                }
            }
            else if (ev.type == SDL_CONTROLLERBUTTONDOWN)
            {
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B ||
                    ev.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                {
                    if (editing) {
                        editing = false;
                        focus = 0;
                        continue;
                    }
                    return UI_RESULT_QUIT;
                }

                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    SDL_Event fake;
                    memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN;
                    fake.key.keysym.sym = SDLK_UP;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    SDL_Event fake;
                    memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN;
                    fake.key.keysym.sym = SDLK_DOWN;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                           ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    SDL_Event fake;
                    memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN;
                    fake.key.keysym.sym = (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? SDLK_LEFT : SDLK_RIGHT;
                    SDL_PushEvent(&fake);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    // Treat like Enter
                    SDL_Event fake;
                    memset(&fake, 0, sizeof(fake));
                    fake.type = SDL_KEYDOWN;
                    fake.key.keysym.sym = SDLK_RETURN;
                    SDL_PushEvent(&fake);
                }
            }
        }

        bool can_import = ui_valid_ipv4(ip);
        bool can_connect = ui_valid_ipv4(ip) && ui_has_keys(cfg);

        render_setup_screen(renderer, status, ip, focus, msg, can_import, can_connect, editing);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}
