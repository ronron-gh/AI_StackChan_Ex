
## Wake word SimpleVox
SimpleVox is a library released by MechaUma-san. It was introduced in robo8080's AI Stack-chan, and is also used in this software. In this software, this can be enabled by adding -DENABLE_WAKEWORD to the build flags in platformio.ini.  
For instructions on how to use the wake word, please refer to the [AI StackChan2 README](https://github.com/robo8080/AI_StackChan2_README/).

### Multiple wake words can be registered
This software allows you to register up to 10 wake words, which allows the device to respond to calls from multiple people.

The registered wake word data will be saved in SPIFFS as wakeword#.bin. Currently, once you reach 10 wake words, you cannot register a new one. Please ask in Function Calling to "delete wake word #", or delete unnecessary wake words using FTP client software such as FFFTP.（FTP user name：stackchan、password：stackchan）。

Obtaining a list of SPIFFS files using FFFTP

![](../images/ftp.jpg)

> If you are concerned about false recognition of the wake word, you can reduce false recognition by lowering the value of DIST_THRESHOLD in WakeWord.h. However, making it too small can make it harder for the device to respond to correct calls.

```
#define DIST_THRESHOLD  (250)
```

> Since the CoreS3 does not have A and B buttons, to register and enable wake words, please use Function Calling to say "Register wake word" and "Enable wake word."
