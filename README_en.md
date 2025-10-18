[日本語](README.md)

# AI Stack-chan Ex
Based on robo8080's [AI Stack-chan](https://github.com/robo8080/AI_StackChan2), we have added the following functions.  
- Added new AI services (new services in bold)
  - LLM: OpenAI ChatGPT / **ModuleLLM**
  - STT: OpenAI Whisper / Google Cloud STT / **ModuleLLM ASR**
  - TTS: **OpenAI TTS** / WEB VOICEVOX / **ElevenLabs** / **AquesTalk** / **ModuleLLM TTS**
  - Wake Word: SimpleVox / **ModuleLLM KWS**
- Compatible with [stackchan-arduino library](https://github.com/mongonta0716/stackchan-arduino)
  - This makes it possible to use YAML for initial configuration and support for serial servos.
- Class design that makes it easy to create additional user applications


> Stack-chan is a super-kawaii palm-sized communication robot developed and released by [Shishikawa-san](https://x.com/stack_chan).
>- [Github](https://github.com/stack-chan/stack-chan)
>- [Discord](https://discord.com/channels/1095725099925110847/1097878659966173225)
>- [ScrapBox](https://scrapbox.io/stack-chan/)

<br>

---
**Table of Contents**
- [Development environment](#development-environment)
- [Basic Usage](#basic-usage)
- [How to Use Module LLM](#how-to-use-module-llm)
- [How to Use OpenAI Realtime API](#how-to-use-openai-realtime-api)
- [Other features](#other-features)
  - [About creating a user application](#about-creating-a-user-application)
  - [Supports SD Updater (Core2 only)](#supports-sd-updater-core2-only)
  - [Face detection by camera (CoreS3 only)](#face-detection-by-camera-cores3-only)
- [About contributions](#about-contributions)
- [Notes](#notes)


## Development environment
- Target device：M5Stack Core2 / CoreS3
- Development PC：
  - OS: Windows11 
  - IDE：VSCode + PlatformIO


## Basic Usage
To use the basic AI conversation features (LLM, STT, TTS via Web APIs) inherited from robo8080's [AI Stack-chan](https://github.com/robo8080/AI_StackChan2), please follow the instructions and setup in the [Basic Usage](doc/basic_usage_en.md) page.

## How to Use Module LLM
By replacing LLM, STT, and TTS with Module LLM APIs, you can run all AI conversation features completely locally.  
After reviewing the [Basic Usage](doc/basic_usage_en.md), please refer to the [How to Configure Module LLM](doc/module_llm_en.md) page for setup instructions.

## How to Use OpenAI Realtime API
With the conventional AI conversation flow, each API (STT → LLM → TTS) introduced latency, often resulting in responses taking over 10 seconds. By using the OpenAI Realtime API, you can input audio data directly to the LLM and receive audio responses, minimizing latency for near real-time conversations.  
To use the Realtime API, please follow the instructions in the [Realtime API](doc/realtime_api_en.md) page.


## Other features
### About creating a user application
By referring to the moddable version of Stack-chan's MOD, we have made it possible to create user applications. (The moddable version of Stack-chan is called the original, and is a [repository](https://github.com/stack-chan/stack-chan) published by Shishikawa-san.)

The source code for user applications is stored in the mod folder, which already contains the applications shown in the table below.
You can also use these as a reference to create and add new applications.

| No. | App Name | Explanation (How to use) | Supplement |
| --- | --- | --- | --- |
| 1 | AI Stack-chan | This is the main app of this repository. | |
| 2 | Pomodoro Timer | This app alternates between a 25 minute alarm and a 5 minute alarm.<br>Button A: Start/Stop<br>Button C: Silent mode off/on | The default setting is silent mode.|
| 3 | Digital Photo Frame | The JPEG file saved in the folder "/app/AiStackChanEx/photo" on the SD card will be displayed on the LCD.<br>Button A: Show next photo <br>Button C: Start slideshow | ・JPEG files saved to the SD card must be 320x240 in size.<br>・During development, there were some cases where the SD card could not be mounted and could not be restored unless it was reformatted. We believe this has been improved, but please back up the data on the SD card just in case.|
| 4 | Status Monitor | Displays various system information.| |

You can register multiple applications using the code below. You can switch between them by flicking the LCD left and right while the application is running.（[Twitter Videos](https://x.com/motoh_tw/status/1841867660746789052)）。

```c++
[main.cpp]
ModBase* init_mod(void)
{
  ModBase* mod;
  add_mod(new AiStackChanMod());      // AI Stack-chan
  add_mod(new PomodoroMod());         // Pomodoro Timer
  add_mod(new StatusMonitorMod());    // Status Monitor
  mod = get_current_mod();
  mod->init();
  return mod;
}
```

### Supports SD Updater (Core2 only)
![](images/sd_updater.jpg)

It now supports SD Updater and can be switched to other SD Updater compatible apps published by NoRi in [BinsPack-for-StackChan-Core2](https://github.com/NoRi-230401/BinsPack-for-StackChan-Core2).

【How to apply】  
① Build with [env:m5stack-core2-sdu].  
② Rename the build result .pio/build/m5stack-core2-sdu/firmware.bin to an appropriate name (e.g. AiStackChanEx.bin) and copy it to the root directory of the SD card.

> ・Currently, the launcher software does not work on Core2 V1.1, so switching is not possible.



### Face detection by camera (CoreS3 only)
![](images/face_detect.jpg)

- When a face is detected, voice recognition is activated.
  - Touching the left center of the LCD will put the device into silent mode, and it will not wake up even if a face is detected. (Instead, Stack-chan will smile while a face is being detected.)
- The camera image is displayed in the upper left corner of the LCD. Touch the image area to turn the display on/off.

※Face detection is disabled by default by commenting out the following in platformio.ini. To enable it, please enable DENABLE_CAMERA and DENABLE_FACE_DETECT.
```
build_flags=
  -DBOARD_HAS_PSRAM
  -DARDUINO_M5STACK_CORES3
  ;-DENABLE_CAMERA
  ;-DENABLE_FACE_DETECT
  -DENABLE_WAKEWORD
```

## About contributions
Issues and pull requests are also welcome. If you have any problems or suggestions for improvement, please contact us via issue first.

## Notes
- Because the folder name is long, the library include path may not work depending on the workspace location. Please make the workspace as close to the C drive as possible. (Example: C:\Git)
