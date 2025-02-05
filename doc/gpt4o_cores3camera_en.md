
## Input camera images to GPT-4o (CoreS3 only)

![](../images/gpt4o_camera.jpg)

As shown in this photo, you can capture an object with the camera, touch the forehead and start a conversation, allowing you to ask questions such as "What is this?"


> The camera is disabled by default in platformio.ini by commenting it out as follows. To enable it, enable DENABLE_CAMERA.
> ```
> build_flags=
>   -DBOARD_HAS_PSRAM
>   -DARDUINO_M5STACK_CORES3
>   ;-DENABLE_CAMERA
>   ;-DENABLE_FACE_DETECT
>   -DENABLE_WAKEWORD
> ```
>
> If you want to turn off the camera during execution, touch the camera image on the LCD. The camera image will no longer be displayed and you will be able to talk normally without image input.
