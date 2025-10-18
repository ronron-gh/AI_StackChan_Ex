# How to Configure Module LLM
![](../images/module_llm.jpg)

This document describes how to set up Module LLM when stacking it on Stack-chan as shown in the picture.

>Operation has only been confirmed with the 2nd lot of Module LLM (firmware: M5_LLM_ubuntu_v1.3_20241203-mini). For instructions on how to update the firmware, please refer to [here](https://docs.m5stack.com/ja/stackflow/module_llm/image) (M5Stack official website).

- [platformio.ini Settings](#platformioini-settings)
- [YAML Settings](#yaml-settings)
  - [Serial Communication PIN](#serial-communication-pin)
  - [Using LLM](#using-llm)
  - [Using STT](#using-stt)
  - [Using TTS](#using-tts)
  - [Using Wake Word (KWS)](#using-wake-word-kws)
- [Servo PIN Restrictions](#servo-pin-restrictions)
- [Appendix A. Other Customization Methods for Module LLM](#appendix-a-other-customization-methods-for-module-llm)
- [Appendix B. How to Implement Function Calling](#appendix-b-how-to-implement-function-calling)
- [Appendix C. How to Enable Japanese for STT and TTS](#appendix-c-how-to-enable-japanese-for-stt-and-tts)
  - [How to Install Whisper](#how-to-install-whisper)
  - [How to Install MeloTTS](#how-to-install-melotts)

## platformio.ini Settings
Add the following settings:

```
build_flags=
    -DUSE_LLM_MODULE
lib_deps =
    m5stack/M5Module-LLM@1.0.0
```

For Core2, select [env:m5stack-core2-llm], and for CoreS3, select [env:m5stack-cores3-llm] to enable the above settings.

## YAML Settings
SD card folder: /app/AiStackChanEx  
File name: SC_ExConfig.yaml

### Serial Communication PIN
This is the PIN setting used for serial communication between the M5 Core and Module LLM.  
Please note that the PINs differ between Core2 and CoreS3.

```yaml
moduleLLM:
  # Serial Pin
  # Core2 Rx:13,Tx:14
  # CoreS3 Rx:18,Tx:17
  rxPin: 13
  txPin: 14
```

### Using LLM
Select 1:ModuleLLM as the LLM type.

```yaml
llm:
  type: 1               # 0:ChatGPT  1:ModuleLLM  2:ModuleLLM(Function Calling)
```

### Using STT
Select 2:ModuleLLM(ASR) as the STT type.  
> 3:ModuleLLM(Whisper) is a Japanese-compatible model and requires additional package installation. See Appendix C for details.

```yaml
stt:
  type: 2               # 0:Google STT  1:OpenAI Whisper  2:ModuleLLM(ASR)  3:ModuleLLM(Whisper)
```

### Using TTS
Select 4:ModuleLLM as the TTS type.

```yaml
tts:
  type: 0               # 0:VOICEVOX  1:ElevenLabs  2:OpenAI TTS  3:AquesTalk 4:ModuleLLM
```

### Using Wake Word (KWS)
Select 1:ModuleLLM(KWS) as the wake word type and set the keyword.

>Keywords must be written in all capital letters in order to be recognized.

```yaml
wakeword:
  type: 1                            # 0:SimpleVox  1:ModuleLLM(KWS)
  keyword: "HI JIMMY"                # Wake word
```

## Servo PIN Restrictions
As shown below, the PINs of Port C cannot be used on either Core2 or CoreS3 because they are used for serial communication with Module LLM.  
Also, Port A cannot be used on CoreS3 because PIN2 is used for the camera clock.  
(As described on the [top page](../README_en.md), please configure the servo settings in SC_BasicConfig.yaml.)

■Core2
| |PIN|Usage|
|---|---|---|
|**Port A**|**32/33**|**Can be assigned to a servo**|
|**Port B**|**26/36**|**Can be assigned to a servo**|
|Port C|13/14|Serial communication with Module LLM|

■CoreS3
| |PIN|Usage|
|---|---|---|
|Port A|1/2|Camera Clock (PIN2)|
|**Port B**|**8/9**|**Can be assigned to a servo**|
|Port C|17/18|Serial communication with Module LLM|

## Appendix A. Other Customization Methods for Module LLM
For more advanced customization, see the following article by airpocket:

[M5Stack LLM Module as a Linux Board: FAQ/Tips](https://elchika.com/article/0e41a4a7-eecc-471e-a259-4fc8d710c26a/)

## Appendix B. How to Implement Function Calling
This is for advanced users, but Function Calling is possible with Module LLM by replacing the LLM model with a Function Calling-compatible model published on Hugging Face.

As an example, this software allows you to call the alarm function implemented on the M5Stack Core side by Function Calling, as shown in [this video (Twitter)](https://x.com/motoh_tw/status/1895120657182269737). Below are the steps required to use Function Calling.

>Operation has only been confirmed with the 2nd lot of Module LLM (firmware: M5_LLM_ubuntu_v1.3_20241203-mini). For instructions on how to update the firmware, please refer to [here](https://docs.m5stack.com/ja/stackflow/module_llm/image) (M5Stack official website).

#### (1) Convert the Hugging Face model to axmodel
Download the LLM model that supports Function Calling from Hugging Face and convert it into an axmodel that can be executed by Module LLM. Follow the steps in [this Qiita article](https://qiita.com/motoh_qiita/items/1b0882e507e803982753).

#### (2) Import the model into StackFlow
To make the model usable from M5Stack Core, import the model into StackFlow (the framework on the Module LLM side). Please follow the steps in [this Qiita article](https://qiita.com/motoh_qiita/items/772464595e414711bbc9).

#### (3) Describe the function in the role
The role is described in the tokenizer (Python) on the Module LLM side. Change the Function description described in the role to the alarm function implemented in this software. If the LLM service is already running, you may need to restart Module LLM to reflect the changes.

File location:  
/opt/m5stack/scripts/SmolLM-360M-Instruct-fncl_tokenizer.py

Role description:

```
fncl_prompt = """You are a helpful assistant with access to the following functions. Use them if required -
 [
    {
        "type": "function",
        "function": {
            "name": "set_alarm",
            "description": "Set alarm.",
            "parameters": {
                "type": "object",
                "properties": {
                    "min": {
                        "type": "integer",
                        "description": 'Set the alarm time in minutes.',
                    },
                },
                "required": ["min"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "stop_alarm",
            "description": "Stop alarm.",
            "parameters": {
                "type": "object",
                "properties": {},
            },
        },
    },
  ] For each function call return a json object with function name and arguments within <toolcall></toolcall> XML tags as follows:
<toolcall>
{'name': , 'arguments': }
</toolcall>
"""
```

#### (4) YAML Configuration
Set the YAML as follows:

SD card folder: /app/AiStackChanEx  
File name: SC_ExConfig.yaml
```yaml
llm:
  type: 2      # 0:ChatGPT  1:ModuleLLM  2:ModuleLLM(Function Calling)
```

That's all you need to do. Now build the latest version of this software (AI Stack-chan Ex) with PlatformIO, write it to the M5Stack Core, and run it.

## Appendix C. How to Enable Japanese for STT and TTS
As of December 2024, the firmware included with Module LLM at purchase does not support Japanese for ASR (STT) and TTS, but you can enable Japanese by installing the Whisper package for ASR and the MeloTTS package for TTS. The installation procedures are described below.

> For TTS, you can also select AquesTalk, which can be run locally on the M5Stack Core, to enable Japanese (see [this page](tts_aquestalk.md) for how to install AquesTalk).

> Operation has only been confirmed with the 2nd lot of Module LLM (firmware: M5_LLM_ubuntu_v1.3_20241203-mini). For instructions on how to update the firmware, please refer to [here](https://docs.m5stack.com/ja/stackflow/module_llm/image) (M5Stack official website).

### How to Install Whisper

#### (1) Install Whisper-related packages
Connect ModuleLLM to the Internet and run the following commands to install.

> All commands are listed in the [M5Stack official documentation](https://modulellm-tutorial.readthedocs.io/en/latest/arduino_examples/text-to-speech.html#).

■Download and add the M5Stack apt repository key to the system  
*If you have already done this, you can skip this step.*
```bash
wget -qO /etc/apt/keyrings/StackFlow.gpg https://repo.llm.m5stack.com/m5stack-apt-repo/key/StackFlow.gpg
echo 'deb [arch=arm64 signed-by=/etc/apt/keyrings/StackFlow.gpg] https://repo.llm.m5stack.com/m5stack-apt-repo jammy ax630c' > /etc/apt/sources.list.d/StackFlow.list
```

Get a list of available software.
```bash
apt update
```

■Update to the latest software packages  
*If you have already done this, you can skip this step.*
```
apt install lib-llm llm-sys
```

■Install the Whisper package
```
apt install llm-whisper llm-kws llm-vad
apt install llm-model-whisper-tiny llm-model-silero-vad llm-model-sherpa-onnx-kws-zipformer-gigaspeech-3.3m-2024-01-01
```

#### (2) YAML Configuration
In the YAML file of this software, select "3:ModuleLLM(Whisper)" as the STT type.

SD card folder: /app/AiStackChanEx  
File name: SC_ExConfig.yaml
```yaml
stt:
  type: 3      # 0:Google STT  1:OpenAI Whisper  2:ModuleLLM(ASR)  3:ModuleLLM(Whisper)
```

That's all you need to do. Now build the latest version of this software (AI Stack-chan Ex) with PlatformIO, write it to the M5Stack Core, and run it.

### How to Install MeloTTS

#### (1) Install MeloTTS-related packages on ModuleLLM
Connect ModuleLLM to the Internet and run the following commands to install.

> All commands are listed in the [M5Stack official documentation](https://modulellm-tutorial.readthedocs.io/en/latest/arduino_examples/text-to-speech.html#).

■Download and add the M5Stack apt repository key to the system  
*If you have already done this, you can skip this step.*
```bash
wget -qO /etc/apt/keyrings/StackFlow.gpg https://repo.llm.m5stack.com/m5stack-apt-repo/key/StackFlow.gpg
echo 'deb [arch=arm64 signed-by=/etc/apt/keyrings/StackFlow.gpg] https://repo.llm.m5stack.com/m5stack-apt-repo jammy ax630c' > /etc/apt/sources.list.d/StackFlow.list
```

Get a list of available software.
```bash
apt update
```

■Update to the latest software packages  
*If you have already done this, you can skip this step.*
```
apt install lib-llm llm-sys
```

■Install the MeloTTS package
```
apt install llm-melotts
apt install llm-model-melotts-ja-jp
```

#### (2) YAML Configuration
In the YAML file of this software, select "4:ModuleLLM" as the TTS type and specify "melotts-ja-jp" as the model.

SD card folder: /app/AiStackChanEx  
File name: SC_ExConfig.yaml
```yaml
tts:
  type: 4                    # 0:VOICEVOX  1:ElevenLabs  2:OpenAI TTS  3:AquesTalk 4:ModuleLLM
  model: "melotts-ja-jp"     # ModuleLLM (Japanese)  *If not specified, English is used
  voice: ""                  # AquesTalk, ModuleLLM (voice not supported)
```

That's all you need to do. Now build the latest version of this software (AI Stack-chan Ex) with PlatformIO, write it to the M5Stack Core, and run it.
