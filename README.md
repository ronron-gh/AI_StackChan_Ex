[English](README_en.md)

# AI Stack-chan Ex
robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/AI_StackChan2)をベースに、次のように機能拡張しています。  
- AI機能の拡張
  - Module LLM（M5Stackの拡張モジュール）によるAI会話機能の完全ローカル化
  - Realtime APIによる遅延のない会話
- [stackchan-arduinoライブラリ](https://github.com/mongonta0716/stackchan-arduino)に対応
  - これにより、YAMLによる初期設定や、シリアルサーボへの対応が可能になりました。
- ユーザアプリケーションを追加作成しやすいクラス設計

---

## 🎯 このフォーク（takish/AI_StackChan_Ex）の追加機能

[ronron-gh/AI_StackChan_Ex](https://github.com/ronron-gh/AI_StackChan_Ex) からフォークし、以下の機能・改善を追加しています。

### M5Stack 公式 AI デスクトップロボット（M5STACK-K151）対応強化

- **PY32 IOExpander 経由の LED 制御**: M5StackChan キットに搭載されている 12 個の WS2812C LED（サーボベース部）を制御。Idle 中の呼吸アニメ、Listening 中の緑点灯、Embarrassed のピンク表示など。
- **M5GFX バージョン固定（0.1.x）**: CoreS3 で LCD バックライトが点灯しない問題を解決（M5GFX 0.2.x の `board_M5StackChan` 自動検出と M5Unified 0.1.x の不一致回避）。
- **SD カードマウントリトライ**: 起動直後の SD 準備失敗に対し最大 5 × 500ms リトライ。

### Web UI（ブラウザ管理画面）

- **Dashboard** (`/dashboard.html`): System / Power / Network / Storage / Config / Role / Memory を 7 カード表示。5 秒おきに自動更新。
- **Settings** (`/settings.html`): SC_SecConfig.yaml / SC_BasicConfig.yaml / SC_ExConfig.yaml を YAML エディタで直接編集（API キー・パスワードは `***UNCHANGED***` でマスク・復元）。
- **Mute Mode** トグル: 外出時に TTS / 効果音を停止。
- **Speaker Volume** スライダー: 0–255 で即時反映、NVS で再起動後も保持。
- **再起動 API**: `POST /api/restart`。

### シリアル経由 YAML 設定（Wi-Fi 不通時の救済策）

- USB シリアルから SD 上の YAML を直接読み書き可能
- コマンド: `:CONFIG GET <ex|basic|sec>` / `:CONFIG SET <ex|basic|sec> ... :END` / `:CONFIG RESTART` / `:CONFIG WIFI_SCAN` / `:CONFIG HELP`
- ホスト側ツール: `tools/serial_config.py`（pyserial 必要）
- **Wi-Fi スキャン機能**: 近隣 SSID 一覧を取得（外出先で「iPhone17Pro と思ったら実は iPhone17pro だった」みたいな大文字小文字違いを発見できる）

### Wi-Fi フォールバック設定

`SC_ExConfig.yaml` に `wifi_fallback:` セクションを追加し、メイン Wi-Fi 接続失敗時にサブ Wi-Fi（テザリングなど）に自動切替。

### ずんだもん風セリフバリエーション

定型応答のセリフを「次のご用件は？」のような硬い表現から、複数候補のランダム選択へ。

- `phrases::listening` 等で thinking / listening / followup / not_heard / error / greeting / head_pet をランダム化
- 起動時挨拶は 5 秒後に自動消去

### 頭撫でモード

下方向フリック検出で `Embarrassed` 表情 + ピンク LED + 上向きサーボ。3 秒で復帰。

### 連続会話モード（YAML 設定）

`SC_ExConfig.yaml` の `conversation:` で `continuous: true` + `max_turns: N` 指定可能。BGM 定型句（「ご視聴ありがとうございました」等）の誤認識で抜ける保護付き。

### アバター表情の追加

- `Expression::Embarrassed`（恥ずかしがり）
- 吹き出し改善: 角丸長方形、スクロール、改行除去、テキスト量に応じた速度自動調整

### ステアリングファイル運用

実装前に作業方針を `firmware/doc/codex/steering/YYYYMMDD-short-topic.md` に書いてからコードに着手する開発フロー（AGENTS.md 参照）。

---

## ⚠️ セキュリティ注意

- Web UI は **LAN 限定**前提で設計されています。インターネットに公開しないでください。
- API キー・Wi-Fi パスワードは絶対に git にコミットしないでください。SD カード上で直接編集するか、シリアル経由設定機能を使ってください。

---

## 🛠 開発者セットアップ

### 必要ツール

```bash
brew install cppcheck   # 静的解析（任意・推奨）
```

PlatformIO Core は VSCode の PlatformIO 拡張を入れていれば `~/.platformio/penv/bin/pio` にインストール済みです。

### 推奨ワークフロー: Makefile

```bash
make upload    # check → build → upload (推奨。書き込み前に自動チェック)
make build     # ビルドのみ
make check     # 静的解析（変更ファイルのみ）
make lint      # 静的解析（全体）
make monitor   # シリアルモニター
make flash     # 書き込みのみ（check スキップ・緊急用）
make clean     # ビルド成果物の削除
make envs      # 利用可能な platformio env リスト
make           # ヘルプ表示
```

別 env への書き込み:
```bash
make ENV=m5stack-core2 upload
make ENV=m5stack-cores3-llm upload
make PORT=/dev/cu.usbmodem11401 upload
```

**設計判断**: embedded 開発では「書き込み前」が一番効く品質ゲート。`make upload` は必ず check + build を通してから書き込みます。

### 補助: git pre-push hook（任意）

`make upload` を経由せずに直接 `git push` しても安全にするための保険として、pre-push hook も用意してあります。初回のみ：

```bash
./tools/setup-hooks.sh
# または
make setup-hooks
```

これで `git push` 時に cppcheck + pio run が走り、ビルド失敗時は push がブロックされます。

スキップしたい場合: `git push --no-verify`

### CI (GitHub Actions)

`.github/workflows/ci.yml` で PR ごとに以下を自動実行（最後の砦）：

- `cppcheck` 静的解析
- `pio run` フルビルド

ローカルで `make upload` が通っていれば CI も通る前提。

---


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
- [Realtime APIの利用方法](#realtime-apiの利用方法)
- [その他の機能](#その他の機能)
  - [ユーザアプリケーションの作成について](#ユーザアプリケーションの作成について)
- [コントリビューションについて](#コントリビューションについて)
  - [本リポジトリの方針](#本リポジトリの方針)


## 開発環境
- ターゲットデバイス：
  - M5Stack Core2
  - M5Stack CoreS3
  - 🆕AtomS3R + Atomic Echo Base
- 開発PC：
  - OS: Windows11 
  - IDE：VSCode + PlatformIO


## 基本的な利用方法
robo8080さんの[AIｽﾀｯｸﾁｬﾝ](https://github.com/robo8080/AI_StackChan2)の仕組みを継承した基本的なAI会話機能（LLM、STT、TTSのWeb APIの連携によるAI会話）を利用する場合は[こちら](doc/basic_usage.md)のページに従って設定、ビルドしてください。

## Module LLMの利用方法
LLM、STT、TTSをModule LLMのAPIに置き換えることで、AI会話機能を完全ローカル化することができます。  
上記の[基本的な利用方法](doc/basic_usage.md)をご確認の上、[こちら](doc/module_llm.md)のページに従って設定してください。

## Realtime APIの利用方法
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
| 5 | ESP-NOW Remote Control | M5Stack公式ｽﾀｯｸﾁｬﾝのオプションのジョイスティックコントローラから受信した姿勢データでサーボを制御します。<br>受信channelは1固定です。 | ESP-NOW使用中はWi-Fi接続が切断されます。別Modへ切り替えるとWi-Fiの再接続を試みます。 |

アプリケーションは下記コードのように記述することで複数登録できます。実行中にLCDを左右にフリックすることで切り替えることができます（[Xの動画](https://x.com/motoh_tw/status/1841867660746789052)）。

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


## コントリビューションについて
issue、プルリクエストも歓迎です。問題や改善案がありましたら、まずはissueでご連絡ください。  
また、Sponsorとしてご支援いただけると大変助かります。日々のトラブルシューティング、お問い合わせへの対応、新しいハードウェアのサポートなどに役立てさせていただきます。  
ご支援いただく際は、次に示す本リポジトリの方針もご理解いただけますと幸いです。

### 本リポジトリの方針
現状、趣味の範囲での活動ではありますが、次のような方針のもとに開発しています。

- AI音声アシスタントとしての性能の追求  
  生成AI関連のニュースが次々と発表され、ｽﾀｯｸﾁｬﾝをAI音声アシスタントとして実用的なレベルに近づけられる可能性が高まっています。これまでの開発でリアルタイム音声会話、MCPによる機能拡張を実現しており、今後は長期記憶（電源を入れなおしても最近の会話内容を覚えている）の実装を目指しています。
- 派生開発のベースとしての公開  
  本リポジトリの成果をたくさんの方々のAIｽﾀｯｸﾁｬﾝ開発のベースとして役立てていただけるよう、シンプルで汎用的な設計を目指しています。
- 新しいハードウェアへの対応  
  上記の方針に影響しない範囲で、ModuleLLMのような今後の発展が期待できる新しいハードウェアもカバーしていきたいと考えています。
- 本家ｽﾀｯｸﾁｬﾝ リポジトリへの貢献  
  本リポジトリは、いわゆる「本家」や「TypeScript版」と呼ばれるししかわ氏の[リポジトリ](https://github.com/stack-chan/stack-chan)をリスペクトし、一部の設計はその影響を受けています。本リポジトリで得られた成果を用いて、本家ｽﾀｯｸﾁｬﾝの発展にも貢献したいと考えています（私がTypeScriptのスキルがないため、現状はあまり実現できていません。スキルのある方はぜひご検討ください）。

