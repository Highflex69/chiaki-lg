#pragma once

#include <SDL2/SDL.h>
#include <chiaki/session.h>
#include <chiaki/controller.h>

typedef struct InputContext InputContext;

InputContext *input_init(int chord_window_ms);
void input_fini(InputContext *ctx);

// Set the active chiaki session so the evdev reader thread can push
// controller state updates directly (bypassing SDL's joystick subsystem).
// Call this after chiaki_session_start() succeeds.
void input_set_session(InputContext *ctx, ChiakiSession *session);

// Handle SDL keyboard events from the TV remote only.
// Gamepad buttons/axes are handled internally via direct evdev + EVIOCGRAB.
void input_handle_event(SDL_Event *ev, InputContext *ctx, ChiakiSession *session);
