[English](README_en.md)

# AI Stack-chan Ex
robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/AI_StackChan2)をベースに、次のように機能拡張しています。  
- AI機能の拡張
  - Module LLM（M5Stackの拡張モジュール）によるAI会話機能の完全ローカル化
  - OpenAI Realtime APIによる遅延のない会話
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
- [開発環境](#開発環境)
- [基本的な利用方法](#基本的な利用方法)
- [Module LLMの利用方法](#module-llmの利用方法)
- [OpenAI Realtime APIの利用方法](#openai-realtime-apiの利用方法)
- [その他の機能](#その他の機能)
  - [ユーザアプリケーションの作成について](#ユーザアプリケーションの作成について)
  - [SD Updaterに対応（Core2のみ）](#sd-updaterに対応core2のみ)
  - [カメラによる顔検出（CoreS3のみ）](#カメラによる顔検出cores3のみ)
- [コントリビューションについて](#コントリビューションについて)


## 開発環境
- ターゲットデバイス：M5Stack Core2 / CoreS3
- 開発PC：
  - OS: Windows11 
  - IDE：VSCode + PlatformIO


## 基本的な利用方法
robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/AI_StackChan2)の仕組みを継承した基本的なAI会話機能（LLM、STT、TTSのWeb APIの連携によるAI会話）を利用する場合は[こちら](doc/basic_usage.md)のページに従って設定、ビルドしてください。

## Module LLMの利用方法
LLM、STT、TTSをModule LLMのAPIに置き換えることで、AI会話機能を完全ローカル化することができます。  
上記の[基本的な利用方法](doc/basic_usage.md)をご確認の上、[こちら](doc/module_llm.md)のページに従って設定してください。

## OpenAI Realtime APIの利用方法
上記の従来のAI会話機能は、STTで音声をテキストに変換→テキストをLLMに入力→LLMのテキスト出力をTTSで音声に変換というステップが必要なため各APIにおける遅延が重なり応答に10秒以上かかっていました。Realtime APIを利用すると、音声データを直接LLMに入力でき、応答も音声データで返ってくるため、遅延を最小限に抑えられます。  
Realtime APIを利用する際は、[こちら](doc/realtime_api.md)のページに従って設定してください。

## その他の機能
### ユーザアプリケーションの作成について
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

※顔検出は初期状態ではplatformio.iniで以下のようにコメントアウトし無効化しています。有効化する際はDENABLE_CAMERAとDENABLE_FACE_DETECTを有効化してください。
```
build_flags=
  -DBOARD_HAS_PSRAM
  -DARDUINO_M5STACK_CORES3
  ;-DENABLE_CAMERA
  ;-DENABLE_FACE_DETECT
  -DENABLE_WAKEWORD
```

## コントリビューションについて
issue、プルリクエストも歓迎です。問題や改善案がありましたら、まずはissueでご連絡ください。



