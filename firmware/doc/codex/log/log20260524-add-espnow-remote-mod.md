# Add ESP-NOW Remote Servo Mod Conversation Log

Date: 2026-05-24

This log records the Codex-assisted conversation and implementation flow for adding the ESP-NOW Remote Control Mod to AI_StackChan_Ex firmware.

## Context

The user wanted to add an ESP-NOW Remote Control application as a `mod` in `firmware/src/mod`, using the official M5Stack Stack-chan ESP-IDF implementation as the reference:

- Reference implementation:
  - `C:\git_self\StackChan\firmware\main\apps\app_espnow_ctrl\app_espnow_ctrl.cpp`
- Target repository:
  - `C:\git\AI_StackChan_Ex\firmware`
- Working policy:
  - Follow `AGENTS.md`.
  - Create a steering file before implementation.
  - Preserve user edits and avoid unrelated refactors.

## Initial Request

The user asked to create an ESP-NOW Remote Control app as a mod, matching the existing official specification. The reference implementation was an ESP-IDF app, while AI_StackChan_Ex firmware uses Arduino/PlatformIO.

Codex inspected:

- `src/mod/ModBase.h`
- `src/mod/ModManager.cpp`
- existing mods such as `StatusMonitorMod` and `VolumeSettingMod`
- `src/main.cpp`
- `src/ServoCustom.*`
- the official `app_espnow_ctrl.cpp`
- the official `hal_espnow.cpp`

Codex identified the official payload format:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | `uint8_t` | target id, `0` for broadcast |
| 1-2 | `int16_t` little endian | yaw |
| 3-4 | `int16_t` little endian | pitch |
| 5-6 | `int16_t` little endian | speed |
| 7 | `uint8_t` | laser enabled |

Codex created the first steering file:

- `doc/codex/steering/20260517-espnow-remote-mod.md`

## Mod Manager Detour

The user then considered a broader Mod management redesign:

- Start in a Mod selection screen.
- Reset the system when switching to another Mod.
- Avoid requiring each Mod to clean up previous Mod state, especially Wi-Fi/ESP-NOW state.

Codex created:

- `doc/codex/steering/20260517-reset-based-mod-selection.md`

After discussion, the user decided to keep the existing Mod philosophy:

- Reuse shared Wi-Fi/Web/FTP/Realtime API infrastructure.
- Make it easy to add app-like Mods.
- Defer the reset-based Mod manager redesign.

Codex marked the reset-based steering as on hold and resumed the ESP-NOW Remote Mod work.

## Phase 1: ESP-NOW Receiver Only

The user requested the smallest possible first implementation:

- Receiver only.
- Channel fixed to `1`.
- Print received data to Serial Monitor.

Codex updated `20260517-espnow-remote-mod.md` with this small scope.

After user approval, Codex implemented:

- `src/mod/EspNowRemote/EspNowRemoteMod.h`
- `src/mod/EspNowRemote/EspNowRemoteMod.cpp`
- registration in `src/main.cpp`
- design notes in `doc/fw_design.md`

Initial behavior:

- Start ESP-NOW receiver on channel 1.
- Copy received data in the ESP-NOW callback.
- Print raw hex data and decoded payload from `idle()`.

Build verification:

- `pio run -e m5stack-core2-realtime`: success
- `pio run -e m5stack-cores3-realtime`: success

## Payload Offset Discovery

The user tested on hardware and reported that `target_id` was at byte offset `20` in the received Arduino `esp_now` data.

Codex updated the decoder:

- Prefer payload offset `20` when enough bytes are available.
- Fall back to offset `0` when only the 8-byte payload is received.
- Print decoded offset in Serial output.

Build verification:

- `pio run -e m5stack-core2-realtime`: success

## Phase 2: Servo Control

The user requested a new steering file before applying received data to servo control.

User-provided policy:

- `main.cpp` has a servo task that follows Avatar gaze, so that task must stop while ESP-NOW remote control is active.
- Map yaw `-1280..1280` to servo x `-45..45` degrees.
- Map pitch `0..900` to servo y `0..-30` degrees.
- Use `ServoCustom::moveTo(x, y)` for the converted values.

Codex created:

- `doc/codex/steering/20260518-espnow-remote-servo.md`

After user approval, Codex implemented:

- `espnow_remote_servo_override` global flag in `main.cpp`.
- Servo task skips gaze control while the flag is enabled.
- `EspNowRemoteMod::init()` enables override.
- `EspNowRemoteMod::pause()` disables override.
- `EspNowRemoteMod::idle()` decodes yaw/pitch and calls `robot->servo->moveTo(x, y)`.

Build verification:

- `pio run -e m5stack-core2-realtime`: success
- `pio run -e m5stack-cores3-realtime`: success

## Yaw Sign and Speed Handling

The user tested and found that yaw was reversed. The user also asked to reflect payload `speed` in servo movement time.

Codex updated:

- yaw `-1280..1280` maps to servo x `45..-45`.
- Added `ServoCustom::moveTo(int degX, int degY, uint32_t millis_for_move)`.
- Passed payload `speed` as movement time.
- Recorded the update in `fw_design.md` and the steering file.

Build verification:

- `pio run -e m5stack-core2-realtime`: success
- `pio run -e m5stack-cores3-realtime`: success

Later, the user adjusted behavior and tested successfully. At the time of the later readback, the code called:

```cpp
robot->servo->moveTo(servo_x, servo_y, 100);
```

while keeping `speed` in the Serial output.

## First Commit and Branch

The user asked Codex to create a branch and commit the work so far.

Codex created:

- branch: `feature/espnow-remote-servo`

Codex committed:

```text
6a41a4d Add ESP-NOW remote servo mod
```

Committed files included:

- ESP-NOW Remote Mod source files
- `main.cpp` registration and servo override
- `ServoCustom` API update
- `doc/fw_design.md`
- steering files
- `src/share/Version.h` update to `0.21.0`

## Phase 3: Wi-Fi Reconnect on Mod Exit

The user asked whether Wi-Fi could reconnect when switching from ESP-NOW Mod to another Mod.

Codex proposed:

- Stop ESP-NOW in `pause()`.
- If not offline, call:
  - `WiFi.disconnect(false, false)`
  - `WiFi.mode(WIFI_STA)`
  - `WiFi.begin()`
- Wait up to 5 seconds for `WL_CONNECTED`.
- Do not reinitialize Web/FTP servers because they are already initialized at boot.

The user asked to create a steering file.

Codex created:

- `doc/codex/steering/20260520-espnow-wifi-reconnect.md`

After user approval, Codex implemented:

- `EspNowRemoteMod::reconnectWiFi()`
- `pause()` now calls `reconnectWiFi()` after stopping ESP-NOW.
- `isOffline == true` skips reconnect.
- Serial output reports reconnect success, IP address, timeout, or offline skip.

Build verification:

- `pio run -e m5stack-core2-realtime`: success
- `pio run -e m5stack-cores3-realtime`: success

The user tested and reported that it worked well.

Codex committed:

```text
44234c5 Reconnect Wi-Fi after ESP-NOW remote mode
```

## README Updates and Optional Mod Registration

The user asked to add the new Mod to the README app list.

Codex added an entry to:

- `README.md`

The user then edited the Japanese wording to specify that the Mod controls servos from pose data received from the optional joystick controller for the official M5Stack Stack-chan.

Codex mirrored the entry in:

- `README_en.md`

The user then commented out `add_mod(new EspNowRemoteMod())` in `init_mod()` because only users with the joystick controller can use this Mod.

Codex committed:

```text
57fa413 Document ESP-NOW remote optional mod
```

Codex pushed:

```text
feature/espnow-remote-servo -> origin/feature/espnow-remote-servo
```

## Merge, Tagging, and Release Tag

The user merged the feature branch into remote `main` and asked Codex to update local `main` and tag the release.

Codex ran:

- `git checkout main`
- `git pull --ff-only origin main`

The merge commit pulled locally was:

```text
9a6dafe Merge pull request #33 from ronron-gh/feature/espnow-remote-servo
```

Codex created annotated tag:

```text
v0.21.0
```

Tag message:

```text
Add ESP-NOW remote servo mod
```

The user then asked to push the tag.

Codex pushed:

```text
v0.21.0 -> origin/v0.21.0
```

## Final Main Outcomes

The feature added:

- ESP-NOW Remote Control Mod implementation under:
  - `src/mod/EspNowRemote/`
- Receiver fixed to channel 1.
- Standard remote payload decoding, with offset `20` preferred for official sender compatibility.
- Servo control from received yaw/pitch data.
- Avatar gaze servo task suppression while the remote Mod is active.
- Wi-Fi reconnect attempt when leaving the ESP-NOW Mod.
- Optional Mod registration documented but commented out in `init_mod()`.
- Japanese and English README entries.
- Design notes and steering files.
- Release version/tag:
  - `v0.21.0`

## Verification Summary

Codex repeatedly verified builds during the work:

- `pio run -e m5stack-core2-realtime`: success
- `pio run -e m5stack-cores3-realtime`: success

The user verified hardware behavior:

- ESP-NOW receive path worked.
- Payload offset `20` was confirmed.
- Servo control worked after yaw sign adjustment.
- Wi-Fi reconnect after leaving ESP-NOW Mod worked.

## Important Notes for Future Work

- ESP-NOW Remote Mod is currently optional and not instantiated by default in `init_mod()`.
- Users need the official M5Stack Stack-chan optional joystick controller or a compatible sender.
- ESP-NOW use disconnects Wi-Fi while active.
- Wi-Fi reconnect is attempted when leaving the Mod, but reconnect failure does not change `isOffline`.
- Laser bit is decoded but not used.
- Target id filtering was discussed but not completed as a main behavior.
- The reset-based Mod manager redesign was explored but explicitly put on hold.

## Commits

```text
6a41a4d Add ESP-NOW remote servo mod
44234c5 Reconnect Wi-Fi after ESP-NOW remote mode
57fa413 Document ESP-NOW remote optional mod
9a6dafe Merge pull request #33 from ronron-gh/feature/espnow-remote-servo
```

## Tag

```text
v0.21.0 - Add ESP-NOW remote servo mod
```
