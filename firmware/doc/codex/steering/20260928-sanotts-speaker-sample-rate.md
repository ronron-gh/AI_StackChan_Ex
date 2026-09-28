# SanoTTS有効環境のスピーカー初期サンプリングレート変更

## 目的

`m5stack-cores3-realtime-sanotts`で発話時に大音量の雑音が発生する問題について、発話直前のスピーカーサンプリングレート変更が原因かを切り分ける。

SanoTTS-jpの出力レートに合わせ、SanoTTS有効環境では`setup()`中のスピーカー初期設定を22,050 Hzにする。

## 対象範囲

- `src/main.cpp`のスピーカー初期設定
- `src/tts/SanoTTS.cpp`の発話前後に行うスピーカー設定処理
- `m5stack-cores3-realtime-sanotts`のビルド確認
- CoreS3実機での雑音、SanoTTS、Realtime API音声の確認

SanoTTSの音声生成、再生速度、バッファリング方式は変更しない。

## 主な変更ファイル

- `src/main.cpp`
- `src/tts/SanoTTS.cpp`
- `doc/codex/steering/20260928-sanotts-speaker-sample-rate.md`

## 実装方針

- `init_mic_spk()`で、`USE_SANOTTS`が有効な場合はスピーカーの初期サンプリングレートを22,050 Hzにする。
- SanoTTSを使用しない環境では、従来どおり64,000 Hzにする。
- `SanoTTS::speak()`では発話前のスピーカー設定を退避せず、`sample_rate`と`stereo`を再設定しない。
- `SanoTTS::speak()`の終了時も、以前のスピーカー設定を復元しない。`setup()`で設定した内容を発話前後で維持する。
- マイクとスピーカーの排他に必要な停止・再開は維持する。今回の切り分け対象は`M5.Speaker.config()`による設定変更に限定する。
- `SANOTTS_PLAYBACK_SAMPLE_RATE`、音量、タスク、バッファリングの設定は変更しない。

## 設計上の注意点

- スピーカー設定はSanoTTS以外の再生処理にも共有される。Realtime API音声、起動音、効果音などに副作用がないことを確認する。
- 22,050 Hzは22.05 kHzを意味する。設定値にはM5Unifiedが受け取るHz単位の`22050`を使用する。
- この変更で雑音が改善しない場合は、スピーカーの停止・再開や音声入出力の切り替えを次の調査対象とする。
- 問題が生じた場合は、`USE_SANOTTS`分岐と`SanoTTS::speak()`の変更を戻して従来の設定・復元処理へ戻せる。

## 確認方法

- `m5stack-cores3-realtime-sanotts`をビルドする。
- 必要に応じて`m5stack-cores3-realtime`もビルドし、SanoTTS無効環境に影響がないことを確認する。
- CoreS3実機で以下を確認する。
  - 起動直後とSanoTTS発話開始時に大音量の雑音が発生しないこと。
  - SanoTTSの音程、速度、区間蓄積再生が従来どおりであること。
  - Realtime APIの受信音声と、`REALTIME_API_WITH_TTS`によるSanoTTS発話が正常であること。
  - 発話後にマイク入力へ正常に復帰すること。

## タスクリスト

- [x] 現在のスピーカー初期設定とSanoTTS発話時の設定変更を確認する。
- [x] SanoTTS有効時の初期サンプリングレートを22,050 Hzに変更する。
- [x] `SanoTTS::speak()`からスピーカー設定と復元処理を取り除く。
- [x] 関係するPlatformIO環境をビルドする。
- [x] CoreS3実機で雑音と各音声経路を確認する。

## 確認結果

- `m5stack-cores3-realtime-sanotts`と`m5stack-cores3-realtime`のPlatformIOビルドに成功した。
- CoreS3実機でしばらく会話を続けても大音量の雑音は再発しなかった。
- 起動時からスピーカーを22,050 Hzに設定し、`SanoTTS::speak()`で設定変更と復元を行わない構成で問題は解決したと判断する。
