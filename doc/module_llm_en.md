
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

```
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

```
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


## Other ways to customize Module LLM
Although it's somewhat more advanced, it's summarized in this article by airpocket-san.

[M5Stack LLM ModuleをLinuxボードとして利用する際のFAQ/Tips](https://elchika.com/article/0e41a4a7-eecc-471e-a259-4fc8d710c26a/)
