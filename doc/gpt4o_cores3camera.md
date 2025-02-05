
## カメラ画像をGPT-4oに入力（CoreS3のみ）

![](../images/gpt4o_camera.jpg)

この写真のように、対象物をカメラで捉えた状態で額部分をタッチして会話を開始することで、「これは何？」といった質問が可能になります。


> カメラは初期状態ではplatformio.iniで以下のようにコメントアウトし無効化しています。有効化する際はDENABLE_CAMERAを有効化してください。
> ```
> build_flags=
>   -DBOARD_HAS_PSRAM
>   -DARDUINO_M5STACK_CORES3
>   ;-DENABLE_CAMERA
>   ;-DENABLE_FACE_DETECT
>   -DENABLE_WAKEWORD
> ```
>
> 実行中にカメラをOFFにしたい場合はLCDのカメラ画像の部分をタッチしてください。カメラ画像が表示されなくなり、通常の画像入力なしの会話になります。