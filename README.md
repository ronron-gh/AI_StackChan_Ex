[English](README_en.md)

# AI Stack-chan Ex
robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/AI_StackChan2)をベースに、以下のような機能拡張を行っています。  
- 対応AIサービスの追加 (太字が追加対応したもの)
  - LLM: OpenAI ChatGPT / **ModuleLLM**
  - STT: OpenAI Whisper / Google Cloud STT / **ModuleLLM ASR**
  - TTS: **OpenAI TTS** / WEB VOICEVOX / **ElevenLabs** / **AquesTalk** / **ModuleLLM TTS**
  - Wake Word: SimpleVox / **ModuleLLM KWS**
- [stackchan-arduinoライブラリ](https://github.com/mongonta0716/stackchan-arduino)に対応
  - これにより、YAMLによる初期設定や、シリアルサーボへの対応が可能になりました。
- ユーザアプリケーションを追加作成しやすいクラス設計


> ｽﾀｯｸﾁｬﾝは[ししかわさん](https://x.com/stack_chan)が開発、公開している、手乗りサイズのｽｰﾊﾟｰｶﾜｲｲコミュニケーションロボットです。
>- [Github](https://github.com/stack-chan/stack-chan)
>- [Discord](https://discord.com/channels/1095725099925110847/1097878659966173225)
>- [ScrapBox](https://scrapbox.io/stack-chan/)

<br>

---
**Table of Contents**
- [AI Stack-chan Ex](#ai-stack-chan-ex)
  - [開発環境](#開発環境)
  - [利用可能なAIサービス](#利用可能なaiサービス)
    - [LLM](#llm)
    - [Speech to Text (STT)](#speech-to-text-stt)
    - [Text to Speech (TTS)](#text-to-speech-tts)
    - [Wake Word](#wake-word)
  - [YAMLによる初期設定](#yamlによる初期設定)
    - [SC\_SecConfig.yaml](#sc_secconfigyaml)
    - [SC\_BasicConfig.yaml](#sc_basicconfigyaml)
    - [SC\_ExConfig.yaml](#sc_exconfigyaml)
  - [ユーザアプリケーションの作成について](#ユーザアプリケーションの作成について)
  - [その他の機能](#その他の機能)
    - [SD Updaterに対応（Core2のみ）](#sd-updaterに対応core2のみ)
    - [カメラによる顔検出（CoreS3のみ）](#カメラによる顔検出cores3のみ)
  - [コントリビューションについて](#コントリビューションについて)
  - [注意事項](#注意事項)


## 開発環境
- デバイス：M5Stack Core2 / CoreS3
- IDE：VSCode Platformio (Arduino)

## 利用可能なAIサービス
会話に必要な各種AIサービスの対応状況を示します。  
どのAIサービスを利用するかは、SDカード上のYAMLファイルの設定で選択できます（APIキーは別途取得していただく必要があります）。

### LLM
|   |ローカル実行|日本語|英語|備考|
|---|---|---|---|---|
|OpenAI ChatGPT|×|〇|〇|・Function Callingに対応[詳細ページ](doc/function_calling.md)<br>・CoreS3のカメラ画像を入力可能[詳細ページ](doc/gpt4o_cores3camera.md)|
|ModuleLLM|〇|〇|〇| [ModuleLLMを使用する際の設定方法](doc/module_llm.md)をご確認ください |

### Speech to Text (STT)

|   |ローカル実行|日本語|英語|備考|
|---|---|---|---|---|
|Google Cloud STT|×|〇|〇| |
|OpenAI Wisper|×|〇|〇| |
|ModuleLLM ASR|〇|×|〇| [ModuleLLMを使用する際の設定方法](doc/module_llm.md)をご確認ください |

### Text to Speech (TTS)

|   |ローカル実行|日本語|英語|備考|
|---|---|---|---|---|
|Web版VoiceVox|×|〇|×| |
|ElevenLabs|×|〇|〇| |
|OpenAI TTS|×|〇|〇| |
|AquesTalk|〇|〇|×|別途ライブラリと辞書データのダウンロードが必要。[詳細ページ](doc/tts_aquestalk.md)|
|ModuleLLM TTS|〇|×|〇| [ModuleLLMを使用する際の設定方法](doc/module_llm.md)をご確認ください |

### Wake Word

|   |ローカル実行|日本語|英語|備考|
|---|---|---|---|---|
|SimpleVox|×|〇|〇|[詳細ページ](doc/wakeword_simple_vox.md) |
|ModuleLLM KWS|〇|×|〇| ・[ModuleLLMを使用する際の設定方法](doc/module_llm.md)をご確認ください<br> ・"Hi Stack"等、日本語環境でも使いやすいワードにすることは可|

## YAMLによる初期設定
SDカードに保存するYAMLファイルで各種設定を行います。

YAMLファイルは次の3種類があります。
- SC_SecConfig.yaml  
  Wi-Fiパスワード、APIキーの設定。（扱いに注意が必要な情報）
- SC_BasicConfig.yaml  
  サーボに関する設定。
- SC_ExConfig.yaml  
  その他、アプリ固有の設定。

### SC_SecConfig.yaml
SDカードフォルダ：/yaml  
ファイル名：SC_SecConfig.yaml

Wi-Fiパスワード、各種AIサービスのAPIキーを設定します。

```
wifi:
  ssid: "********"
  password: "********"

apikey:
  stt: "********"       # ApiKey of SpeechToText Service (OpenAI Whisper/ Google Cloud STT 何れかのキー)
  aiservice: "********" # ApiKey of AIService (OpenAI ChatGPT)
  tts: "********"       # ApiKey of TextToSpeech Service (VoiceVox / ElevenLabs/ OpenAI 何れかのキー)
```


### SC_BasicConfig.yaml
SDカードフォルダ：/yaml  
ファイル名：SC_BasicConfig.yaml

サーボに関する設定をします。

```
servo: 
  pin: 
    # ServoPin
    # Core1 PortA X:22,Y:21 PortC X:16,Y:17
    # Core2 PortA X:33,Y:32 PortC X:13,Y:14
    # CoreS3 PortC X:18,Y:17
    # Stack-chanPCB Core1 X:5,Y:2 Core2 X:19,Y27
    # When using SCS0009, x:RX, y:TX (not used).(StackchanRT Version:Core1 x16,y17, Core2: x13,y14)
    x: 33
    y: 32
  center:
    # SG90 X:90, Y:90
    # SCS0009 X:150, Y:150
    # Dynamixel X:180, Y:270
    x: 90
    y: 90
  offset: 
    # Specified by +- from 90 degree during servo initialization
    x: 0
    y: 0

servo_type: "PWM" # "PWM": SG90PWMServo, "SCS": Feetech SCS0009
```

> SC_BasicConfig.yamlには他にも様々な基本設定が記述されていますが、現状、本ソフトが対応しているのは上記の設定のみです。


### SC_ExConfig.yaml
SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

AIサービスの選択や、サービス毎のパラメータを設定します。

```
llm:
  type: 0                            # 0:ChatGPT  1:ModuleLLM

tts:
  type: 0                            # 0:VOICEVOX  1:ElevenLabs  2:OpenAI TTS  3:AquesTalk 4:ModuleLLM

  model: ""                          # VOICEVOX (modelは未対応)
  #model: "eleven_multilingual_v2"    # ElevenLabs
  #model: "tts-1"                     # OpenAI TTS
  #model: ""                          # AquesTalk (modelは未対応)

  voice: "3"                         # VOICEVOX (ずんだもん)
  #voice: "AZnzlk1XvdvUeBnXmlld"      # ElevenLabs
  #voice: "alloy"                     # OpenAI TTS
  #voice: ""                          # AquesTalk (voiceは未対応)

stt:
  type: 0                            # 0:Google STT  1:OpenAI Whisper  2:ModuleLLM(ASR)

wakeword:
  type: 0                            # 0:SimpleVox  1:ModuleLLM(KWS)
  keyword: ""                        # SimpleVox (初期設定は不可。ボタンB長押しで登録)
  #keyword: "HI STUCK"                # ModuleLLM(KWS)

# ModuleLLM
moduleLLM:
  # Serial Pin
  # Core2 Rx:13,Tx:14
  # CoreS3 Rx:18,Tx:17
  rxPin: 13
  txPin: 14

```

## ユーザアプリケーションの作成について
moddable版ｽﾀｯｸﾁｬﾝ（本家と呼ばれている、ししかわさん公開の[リポジトリ](https://github.com/stack-chan/stack-chan)）のMODを参考に、ユーザアプリケーションを作成できるようにしました。

ユーザアプリケーションのソースコードはmodフォルダに格納しており、すでに下表に示すアプリケーションが入っています。
これらを参考に新たなアプリケーションを作成し追加することも可能です。

| No. | アプリ名 | 説明(使い方) | 補足 |
| --- | --- | --- | --- |
| 1 | AIｽﾀｯｸﾁｬﾝ | 本リポジトリのメインアプリです。 | |
| 2 | ポモドーロタイマ | 25分のアラームと5分のアラームを交互に繰り返すアプリです。<br>ボタンA：スタート/ストップ<br>ボタンC：無音モード解除/設定 | 初期状態は無音モードです。|
| 3 | デジタルフォトフレーム | SDカードのフォルダ "/app/AiStackChanEx/photo" に保存したJPEGファイルをLCDに表示します。<br>ボタンA：次の写真を表示 <br>ボタンC：スライドショー開始 | ・SDカードに保存するJPEGファイルはサイズを320x240にしておく必要があります。<br>・開発中、SDカードがマウント不可になり、再フォーマットしないと復旧しない事象が発生していました。改善したつもりですが、念のためSDカードのデータはバックアップをお願いします。|
| 4 | ステータスモニタ | 各種システム情報を表示します。| |

アプリケーションは下記コードのように複数登録できます。実行中にLCDを左右にフリックすることで切り替えることができます（[Xの動画](https://x.com/motoh_tw/status/1841867660746789052)）。

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


## その他の機能
### SD Updaterに対応（Core2のみ）
![](images/sd_updater.jpg)

SD Updaterに対応し、NoRiさんの[BinsPack-for-StackChan-Core2](https://github.com/NoRi-230401/BinsPack-for-StackChan-Core2)で公開されている他のSD Updater対応アプリとの切り替えが可能になりました。

【適用方法】  
① env:m5stack-core2-sduでビルドする。  
② ビルド結果の.pio/build/m5stack-core2-sdu/firmware.binを適切な名前（AiStackChanEx.bin等）に変更し、SDカードのルートディレクトリにコピーする。

> ・現状、Core2 V1.1ではランチャーソフトが動作しないため切り替えはできません。



### カメラによる顔検出（CoreS3のみ）
![](images/face_detect.jpg)

- 顔を検出すると音声認識を起動します。
  - LCD中央左側をタッチするとサイレントモードになり、顔検出しても起動しません。（代わりに、顔検出している間ｽﾀｯｸﾁｬﾝが笑顔になります。）
- LCDの左上隅にカメラ画像が表示されます。画像部分をタッチすると表示ON/OFFできます。

※顔検出は初期状態ではplatformio.iniで以下のようにコメントアウトし無効化しています。
```
build_flags=
  -DBOARD_HAS_PSRAM
  -DARDUINO_M5STACK_CORES3
  ;-DENABLE_FACE_DETECT
  -DENABLE_WAKEWORD
```

## コントリビューションについて
issue、プルリクエストも歓迎です。問題や改善案がありましたら、まずはissueでご連絡ください。

## 注意事項
- フォルダ名が長いため、ワークスペースの場所によってはライブラリのインクルードパスが通らない場合があります。なるべくCドライブ直下に近い場所をワークスペースにしてください。(例 C:\Git)

