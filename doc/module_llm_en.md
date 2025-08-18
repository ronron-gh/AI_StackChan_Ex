
# How to enable Module LLM
![](../images/module_llm.jpg)

This article describes how to set up Module LLM when stacking it on Stack-chan as shown in the picture.

## platformio.ini
Add the following settings.

```
build_flags=
    -DUSE_LLM_MODULE
lib_deps =
    m5stack/M5Module-LLM@1.0.0
```

The above settings will be enabled by selecting [env:m5stack-core2-llm] for Core2, or [env:m5stack-cores3-llm] for CoreS3.

## YAML

### Serial Communication PIN
This is the PIN setting used for serial communication between the M5 Core and Module LLM.  
Please note that the PIN is different for Core2 and CoreS3.

SD card folder：/app/AiStackChanEx  
File name：SC_ExConfig.yaml

```yaml
moduleLLM:
  # Serial Pin
  # Core2 Rx:13,Tx:14
  # CoreS3 Rx:18,Tx:17
  rxPin: 13
  txPin: 14
```

### Wake word (only if using KWS)

SD card folder：/app/AiStackChanEx  
File name：SC_ExConfig.yaml

```yaml
wakeword:
  type: 1                            # 0:SimpleVox  1:ModuleLLM(KWS)
  keyword: "HI STACK"                # Wake word
```

### Servo PIN Restrictions
As shown below, the PIN of Port C cannot be used on either Core2 or CoreS3 because it is used for serial communication with Module LLM.  
Also, Port A cannot be used on CoreS3 because PIN2 is used for the camera clock.  
(As described on the [top page](../README.md), please configure the servo settings in SC_BasicConfig.yaml.)

■Core2
| |PIN|Usage|
|---|---|---|
|**Port A**|**32/33**|**Can be assigned to a servo**|
|**Port B**|**26/36**|**Can be assigned to a servo**|
|Port C|13/14|Serial communication with the LLM module|

■CoreS3
| |PIN|Usage|
|---|---|---|
|Port A|1/2|Camera Clock(PIN2)|
|**Port B**|**8/9**|**Can be assigned to a servo**|
|Port C|17/18|Serial communication with the LLM module|


## Appendix A. Other ways to customize Module LLM
Although it's somewhat more advanced, it's summarized in this article by airpocket-san.

[M5Stack LLM ModuleをLinuxボードとして利用する際のFAQ/Tips](https://elchika.com/article/0e41a4a7-eecc-471e-a259-4fc8d710c26a/)

## Appendix B. How to implement Function Calling
Although it is somewhat more advanced, Function Calling is possible with Module LLM by replacing the LLM model with a Function Calling-compatible model published on Hugging Face.

As an example, this software allows you to call the alarm function implemented on the M5Stack Core side by Function Calling, as shown in [this video (Twitter)](https://x.com/motoh_tw/status/1895120657182269737). Below, we will describe the steps required to use Function Calling.

>Operation has only been confirmed with the 2nd lot of Module LLM (firmware: M5_LLM_ubuntu_v1.3_20241203-mini). For instructions on how to update the firmware, please refer to [here](https://docs.m5stack.com/ja/stackflow/module_llm/image) (M5Stack official website).

### (1) Convert the Hugging Face model to axmodel
Download the LLM model that supports Function Calling from Hugging Face and convert it into an axmodel that can be executed by Module LLM. Follow the steps in [this Qiita article](https://qiita.com/motoh_qiita/items/1b0882e507e803982753).

### (2) Import the model into StackFlow
To make the model usable from M5Stack Core, import the model into StackFlow (the framework on the Module LLM side). Please follow the steps in [this Qiita article](https://qiita.com/motoh_qiita/items/772464595e414711bbc9).

### (3) Describe the function in the role
The role is described in the tokenizer (Python) on the Module LLM side. Change the Function description described in the role to the alarm function implemented in this software. If the LLM service is already running, you may need to restart Module LLM to reflect the changes.

File location：  
/opt/m5stack/scripts/SmolLM-360M-Instruct-fncl_tokenizer.py

Role description：

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

### YAML Configuration
Set the YAML as follows:

SD card folder：/app/AiStackChanEx  
File name：SC_ExConfig.yaml
```yaml
llm:
  type: 2      # 0:ChatGPT  1:ModuleLLM  2:ModuleLLM(Function Calling)
```

That's all you need to do. Now build it with PlatformIO and write it to the M5Stack Core and run it.
