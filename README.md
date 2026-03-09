# chiaki-lg

A native port of [chiaki-ng](https://github.com/streetpea/chiaki-ng) for LG webOS smart TVs.
Streams PS4/PS5 Remote Play directly to your TV.

---

## Features

- **PS4 and PS5 Remote Play** over local network
- **1080p60 streaming** with H.265, H.265 HDR, or H.264 video codecs
- **Hardware video decode** via webOS NDL (direct media pipeline) — video is decoded on a dedicated hardware plane below the app surface, not software-rendered
- **Native Opus audio passthrough** — raw Opus packets fed directly to webOS NDL hardware decoder (no software decode step)
- **Full gamepad support** — DualSense, DualShock 4, Xbox Wireless Controller, and other Bluetooth/USB gamepads via direct evdev with exclusive grab (`EVIOCGRAB`)
- **chiaki-ng settings import** — drop a `chiaki-ng-Default.ini` export file onto the TV to auto-import registration credentials (PS5 IP still required)
- **Wake-on-LAN** — wakes PS5 from rest mode before connecting (UDP broadcast + unicast)
- **PSN cloud wakeup** — optional Sony push-notification wakeup via PSN session API when local UDP wakeup is unreliable (requires PSN refresh token)
- **Sleep-on-exit** — sends PS5 to rest mode when you quit the app
- **Auto-reconnect** — retries on transient network errors; exits cleanly on deliberate disconnects
- **Stats overlay** — real-time bitrate, FPS, codec, and latency display toggled with the TV remote UP button
- **Two-tier logging** — `app_log_always()` for critical diagnostics (always written), `app_log()` for verbose chatter (gated by `log_level`)

---

## Architecture

The app is a single native C binary (`chiaki-webos`) that links against chiaki-ng's C library for the PS Remote Play protocol and uses the [SS4S](https://github.com/aspect-apps/ss4s) abstraction layer (with the `ndl-webos5` backend) for hardware-accelerated video and audio output on webOS.

### Video pipeline

chiaki-lg decodes the Remote Play stream and delivers H.264/H.265 NAL units via a callback. The app feeds these directly to `SS4S_PlayerVideoFeed()` which routes them to webOS NDL's hardware video decoder. NDL renders video on a dedicated hardware plane *below* the app's OpenGL surface. The app's EGL surface is configured with an 8-bit alpha channel and cleared to transparent each frame so the video plane shows through. The SDL/GL surface is only used for UI overlays (loading screen, stats).

### Audio pipeline

chiaki-lg delivers raw Opus-encoded packets via an audio callback. The app feeds these directly to `SS4S_PlayerAudioFeed()` with codec `SS4S_AUDIO_OPUS`. webOS NDL has native hardware Opus decoding — there is no software decode step.

### Input pipeline

Gamepad input uses direct Linux evdev reads with `EVIOCGRAB` for exclusive device access. This prevents webOS's `hidd` daemon from intercepting gamepad buttons (which would otherwise remap B→Back, A→OK, Guide→Home at the system level). The evdev reader runs in a dedicated thread and pushes `ChiakiControllerState` updates directly to the chiaki session, bypassing SDL's joystick subsystem entirely.

The TV remote is **not** mapped to PS5 controller inputs. It serves only three functions during streaming:

| TV Remote Button | Action |
|---|---|
| **Up** | Toggle stats overlay |
| **Back** | Exit the app |
| **Home** | Exit the app (SDL_QUIT) |

Volume is handled by the webOS system layer and works normally.

### PSN cloud wakeup

When a `psn_refresh_token` is configured, the app performs a multi-step PSN API sequence before falling back to UDP wakeup:

1. Refresh the OAuth2 access token via Sony's auth endpoint
2. Query the PSN device list to find the PS5's device UID (duid)
3. Create a Remote Play session on PSN's session manager (triggers a push notification to the PS5)
4. Send a wakeup command via the `cloudAssistedNavigation` commands endpoint (delivers an SQS message to the PS5)
5. Clean up the session

If any step fails, the app falls back to standard UDP wakeup packets. All PSN API calls use direct libcurl with SSL verification disabled (webOS lacks the CA bundle paths that chiaki-ng's internal holepunch library expects).

---

## Source files

| File | Purpose |
|---|---|
| `main.c` | Entry point, session lifecycle, wakeup (UDP + PSN cloud), reconnect logic, SDL event loop, stats overlay integration |
| `config.c` / `config.h` | JSON config loader (`json-c`), default config generation |
| `config_import.c` / `config_import.h` | chiaki-ng Qt INI settings import (parses `@ByteArray()` format) |
| `video.c` / `video.h` | Video callback → SS4S/NDL feed, codec negotiation, stats counters |
| `audio.c` / `audio.h` | Audio callback → SS4S/NDL Opus feed, stats counters |
| `input.c` / `input.h` | Evdev gamepad reader thread with `EVIOCGRAB`, axis normalization |
| `ui.c` / `ui.h` | Registration UI (on-screen keyboard, PIN entry), loading screen, stats overlay renderer |
| `stats.c` / `stats.h` | Thread-safe stream statistics counters and overlay state machine |
| `app_log.h` | Shared logging API (`app_log` / `app_log_always`) |
| `NDL_directmedia.h` | webOS NDL API declarations for direct video buffer queries |
| `CMakeLists.txt` | Build system |
| `build-webos.sh` | One-shot cross-compile script (dependencies + app + IPK packaging) |
| `appinfo.json` | webOS app metadata (ID: `org.homebrew.chiaki`) |
| `config.json` | Runtime config (deployed to TV) |
| `config_json.example` | Annotated config template |

---

## Prerequisites

### Build machine (Linux or WSL2)

- GCC, CMake ≥ 3.16, make, pkg-config, wget, git
- Python 3 with pip
- [webOS native SDK / toolchain](https://github.com/webosbrew/native-toolchain) extracted and `relocate-sdk.sh` run
- [ares-cli](https://github.com/webosbrew/ares-cli-rs) in PATH (`ares-package`, `ares-install`, `ares-setup-device`)
- `nanopb` and `protobuf` Python packages:
  ```bash
  pip3 install nanopb protobuf --break-system-packages
  ```

> **WSL users:** keep build and source files on a Linux filesystem (e.g. `~/`) rather than `/mnt/c/`. The build script works around most WSL/NTFS issues, but git operations and timestamps are more reliable on ext4.

### TV

- LG webOS TV with [Homebrew Channel](https://github.com/webosbrew/webos-homebrew-channel) installed
- Developer Mode enabled via Homebrew Channel

---

## Building

### 1. Clone both repos side by side

```bash
git clone https://github.com/streetpea/chiaki-ng.git
git clone <this-repo-url> chiaki-webos-build
cd chiaki-webos-build
```

### 2. Set your toolchain path

```bash
export TOOLCHAIN_DIR=~/webos-sdk/arm-webos-linux-gnueabi_sdk-buildroot
```

### 3. Run the build script

```bash
./build-webos.sh ../chiaki-ng 2>&1 | tee build.log
```

The script will:
1. Source the webOS SDK environment
2. Cross-compile all dependencies for ARMv7a (OpenSSL, Opus, FFmpeg, json-c, miniupnpc, cURL with WebSocket support, GF-Complete, Jerasure, SS4S)
3. Patch chiaki-ng sources for webOS glibc compatibility
4. Pre-generate nanopb protobuf sources
5. Build `chiaki-webos`
6. Package into an `.ipk` via `ares-package`

Dependencies are cached in `/tmp/webos-staging`; subsequent builds skip them.

> **Important:** Always build via `./build-webos.sh`, not `cmake --build` directly. The script performs essential pre-build steps that cmake cannot handle in a WSL environment.

---

## Installation

```bash
# Add your TV (one-time — TV must be on and in developer mode)
ares-setup-device

# Install the IPK
ares-install --device myTV org.homebrew.chiaki_*.ipk
```

Or use WebOS Dev Manager https://github.com/webosbrew/dev-manager-desktop

---

## First-run: registration

On first launch (or when registration keys are missing from `config.json`), the built-in UI walks you through setup:

1. **IP address screen** — enter your PS5's local IP using the on-screen keyboard
2. **PSN Account ID screen** — enter your numeric PSN Account ID (see below)
3. On your PS5, go to **Settings → System → Remote Play → Link Device** — an 8-digit PIN appears
4. **PIN entry screen** — enter the PIN on the TV
5. The app registers with your PS5, saves keys to `config.json`, and streaming starts immediately

On subsequent launches the app reads saved keys and connects directly.

### Getting your PSN Account ID

```bash
# In the chiaki-ng directory on your PC:
python3 scripts/psn-account-id.py
```

This opens a PSN browser login and prints a numeric ID like `7309513963409468843`. Enter it during registration.

### Importing from an existing chiaki-ng installation

Instead of registering through the TV UI, you can export your chiaki-ng config:

**Option A: INI import** — Copy `Chiaki.conf` (from `%APPDATA%\Roaming\Chiaki\` on Windows or `~/.config/Chiaki/` on Linux) to the TV as `chiaki-ng-Default.ini` in the app directory. The app auto-imports registration credentials on next launch.

**Option B: Manual config** — Copy these fields from `Chiaki.conf` into `config.json`:
- `server/[MAC]/AccountId` → `psn_account_id` (base64)
- `server/[MAC]/RegistKey` → `registered_key` (base64)
- `server/[MAC]/RPKey` → `rp_key` (base64)
- `server/[MAC]/RPKeyType` → `rp_key_type` (integer)

---

## Configuration

Config file location on the TV:
```
/media/developer/apps/usr/palm/applications/org.homebrew.chiaki/config.json
```

### Example config

```json
{
    "host": "192.168.1.100",
    "ps5": true,
    "psn_account_id": "",
    "registered_key": "",
    "rp_key": "",
    "rp_key_type": 2,
    "video_width": 1920,
    "video_height": 1080,
    "video_fps": 60,
    "video_bitrate": 15000,
    "video_codec": "h265",
    "audio_volume": 100,
    "wakeup": true,
    "ps5_mac": "AA:BB:CC:DD:EE:FF",
    "wakeup_delay_ms": 60000,
    "sleep_on_exit": true,
    "log_level": "warning",
    "psn_refresh_token": ""
}
```

### Config fields

| Field | Type | Default | Description |
|---|---|---|---|
| `host` | string | `""` | PS5/PS4 local IP address |
| `ps5` | bool | `true` | `true` for PS5, `false` for PS4 |
| `psn_account_id` | string | `""` | PSN account ID (base64) — written by registration UI |
| `registered_key` | string | `""` | Registration key (base64) — written by registration UI |
| `rp_key` | string | `""` | Remote Play key (base64) — written by registration UI |
| `rp_key_type` | int | `0` | RP key type — written by registration UI |
| `video_width` | int | `1920` | Stream width |
| `video_height` | int | `1080` | Stream height |
| `video_fps` | int | `60` | Stream frame rate |
| `video_bitrate` | int | `15000` | Target bitrate in kbps |
| `video_codec` | string | `"h265"` | `"h265"`, `"h265_hdr"`, or `"h264"` (PS4 always uses H.264) |
| `audio_volume` | int | `100` | Audio volume (0–100) |
| `wakeup` | bool | `false` | Send wakeup packets before connecting |
| `ps5_mac` | string | `""` | PS5 MAC address (used for WoL) |
| `wakeup_delay_ms` | int | `60000` | Max time (ms) to wait for PS5 to wake |
| `sleep_on_exit` | bool | `false` | Send PS5 to rest mode on app exit |
| `log_level` | string | `"warning"` | `"off"`, `"error"`, `"warning"`, `"info"`, `"verbose"`, `"debug"` |
| `psn_refresh_token` | string | `""` | PSN OAuth2 refresh token for cloud wakeup (optional, from chiaki-ng Windows config) |

---

## Gamepad support

Connect a gamepad via Bluetooth or USB. Supported controllers include DualSense, DualShock 4, Xbox Wireless Controller, and most HID-compliant gamepads.

The app uses direct evdev access with `EVIOCGRAB` to take exclusive control of the gamepad at the kernel level. This prevents webOS from intercepting buttons (B→Back, A→OK, Guide→Home) and ensures all inputs reach the PS5 unmodified.

Axis mapping handles both standard HID layouts (DualSense/DualShock) and the Xbox Wireless Controller's webOS-specific axis assignments (ABS_Z/ABS_RZ for right stick, ABS_GAS/ABS_BRAKE for triggers).

The TV remote is not forwarded to the PS5 as controller input.

---

## Logs

Logs are written to `/tmp/chiaki.log` on the TV.

```bash
# Stream logs live
ares-shell --device myTV -- "tail -f /tmp/chiaki.log"

# Download the log file
ares-shell --device myTV -- "cat /tmp/chiaki.log" > chiaki.log
```

Log verbosity is controlled by `log_level` in the config. Critical events (session lifecycle, wakeup status, errors) are always logged regardless of level.

---

## Troubleshooting

**Stream doesn't start / connection timeout**
Ensure Remote Play is enabled on your PS5 (Settings → System → Remote Play → Enable Remote Play). Verify the IP in `config.json` and that the TV and PS5 are on the same subnet.

**Black screen with audio**
Likely a codec or NDL issue. Check logs. Try `"video_codec": "h264"` as a fallback — H.264 has broader compatibility.

**PS5 won't wake from rest mode**
Local UDP wakeup can be unreliable. To enable PSN cloud wakeup, copy the `psn_refresh_token` from your chiaki-ng Windows config (`%APPDATA%\Roaming\Chiaki\Chiaki.conf`, field `psn_refresh_token` under `[General]`) into `config.json`. The app will use Sony's push notification service as the primary wakeup method with UDP as fallback.

**Registration fails / CE-110032-7 on PS5**
The PSN Account ID is wrong. Use `python3 scripts/psn-account-id.py` from the chiaki-ng directory to get the correct numeric ID.

**Registration fails / "invalid PIN"**
The PIN expires in ~60 seconds. Open the TV's registration screen first, then go to PS5 Settings → Remote Play → Link Device, and enter the PIN promptly.

**Gamepad buttons intercepted by webOS (B exits app, etc.)**
The EVIOCGRAB failed — check logs for `EVIOCGRAB FAILED`. This can happen if another process already has the device open. Try disconnecting and reconnecting the controller after launching the app.

**App crashes on launch**
Usually a malformed `config.json`. Check `/tmp/chiaki.log`. Delete the config file to let the app recreate defaults, then re-register.

**Build fails: "cannot find -lchiaki"**
You ran `cmake --build` directly. Always use `./build-webos.sh`.

---

## Dependencies

| Library | Version | Link type | Purpose |
|---|---|---|---|
| chiaki-ng | main | static | PS Remote Play protocol |
| SS4S | — | static | webOS NDL video/audio abstraction |
| OpenSSL | 3.2.1 | static | TLS for PSN API, crypto for chiaki |
| Opus | 1.4 | static | Audio codec (chiaki internal use) |
| FFmpeg | 6.1.1 | static | H.264/H.265 codec support |
| json-c | 0.17 | static | Config file parsing |
| cURL | 8.7.1 | static | PSN API HTTP/WebSocket calls |
| miniupnpc | 2.2.7 | static | UPnP (chiaki dependency) |
| GF-Complete | master | static | Erasure coding (chiaki dependency) |
| Jerasure | 2.0 | static | FEC (chiaki dependency) |
| SDL2 | 2.30.x | dynamic | Window/GL surface, TV remote events |
| nanopb | 0.4.x | static | Protobuf (chiaki submodule) |

---

## License

AGPL-3.0 — same as [chiaki-ng](https://github.com/streetpea/chiaki-ng).
