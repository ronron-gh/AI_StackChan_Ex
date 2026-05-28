# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Overview

AI Stack-chan Ex — M5Stack 用 AI スタックチャンの拡張版。
Core2 / CoreS3 / AtomS3R + Module LLM 対応。

詳細な実装方針は [`firmware/AGENTS.md`](firmware/AGENTS.md) を参照。

## Development Workflow

### Build & Upload

PlatformIO CLI を直接呼ぶのが最速：

```bash
# CLI パス（VSCode の PlatformIO 拡張が入っていれば存在する）
PIO=~/.platformio/penv/bin/pio

# ビルドのみ（CoreS3 用）
$PIO run -e m5stack-cores3

# ビルド + 書き込み
$PIO run -e m5stack-cores3 -t upload

# シリアルモニター
$PIO device monitor -e m5stack-cores3 -p /dev/cu.usbmodem11401 -b 115200
```

VSCode UI を使う必要はない。コマンドの方が出力をログとして残しやすく、エージェントからも扱いやすい。

### 利用可能な env

```bash
grep "^\[env:" firmware/platformio.ini
```

主な選択肢：
- `m5stack-cores3` — 通常 OpenAI ChatGPT
- `m5stack-cores3-realtime` — OpenAI Realtime API
- `m5stack-cores3-llm` — ModuleLLM（完全ローカル）
- Core2 / AtomS3R も同様の命名

### シリアルログをファイルに保存（デバッグ用）

USB 切断・再接続にも耐える保存ループ：

```bash
nohup bash -c '
  while true; do
    if [ -e /dev/cu.usbmodem11401 ]; then
      cat /dev/cu.usbmodem11401 2>/dev/null >> /tmp/coreS3.log
    fi
    sleep 0.1
  done
' > /dev/null 2>&1 &
disown
```

書き込み実行前には `pkill -f "cat /dev/cu.usbmodem"` でポートを解放する。

### SD カード YAML の編集

ファームの再ビルドは不要。SD カードを Mac に挿してファイルを直接編集：

- `/yaml/SC_SecConfig.yaml` — Wi-Fi、APIキー
- `/yaml/SC_BasicConfig.yaml` — サーボ設定
- `/app/AiStackChanEx/SC_ExConfig.yaml` — AI 構成（LLM/TTS/STT/wakeword）

⚠️ **API キーや Wi-Fi パスワードを絶対に git にコミットしない**。SD カード上で直接編集する。

## 既知の注意点

### M5GFX のバージョン固定（重要）

`firmware/platformio.ini` で `m5stack/M5GFX @ ^0.1.0` を明示指定している。理由：

- 依存解決放置だと M5GFX 0.2.x（最新）が引き込まれる
- M5GFX 0.2.x は `board_M5StackChan` を autodetect する新ロジックを持つ
- 一方 M5Unified 0.1.x / 0.2.7 はそのボードIDを知らないため、CoreS3 用初期化が中途半端になり LCD バックライト点灯しない
- → M5GFX を 0.1.x に固定し、CoreS3 として認識させる

### M5StackChan キット使用時のサーボ設定

`SC_BasicConfig.yaml` の `servo_type`:
- `"M5_SCS"` — PY32 IOExpander 経由（公式キット標準）
- `"SCS"` — PY32 を使わない（PY32 が応答しない場合のフォールバック）
- `"PWM"` — SG90 等の PWM サーボ

PY32 が応答しないと `stackchan-arduino` の `Stackchan_servo.cpp` で `while (1)` のリトライループに入る点に注意。

### SD カードマウントのタイミング

起動直後の SD カード初期化が一度失敗することがある。`main.cpp` でリトライ（最大 5 × 500ms）を実装済み。失敗が継続する場合のみ `Failed to load SD card settings. System reset after 5 seconds.` を出して再起動する。

## ステアリングファイル運用

実装前に作業方針を `firmware/doc/codex/steering/YYYYMMDD-short-topic.md` に書いてからコードに着手する（AGENTS.md の方針）。

例: `firmware/doc/codex/steering/20260528-speech-bubble-improvements.md`

## レイヤードアーキテクチャ（厳守）

`/layered-review` で検出した依存違反を整理し、PR #12 で driver / TTS / app 層の独立性を回復した。**今後追加するコードもこの構造を守ること**。

### レイヤー階層（下が低レベル、上が高レベル）

```
L6  main.cpp / WebAPI.cpp        ← composition root / presentation
L5  mod/                          ← use case / application
L4  Robot, ServoCustom            ← orchestration
L3  llm/, tts/, stt/, wakeword/   ← external service integration
L2  driver/                       ← hardware drivers
L1  share/, app/, rootCA/         ← utilities / value objects / state
L0  外部ライブラリ                ← M5Unified, ESP-IDF, Arduino 等
```

### 依存ルール（DO）

- **下のレイヤーへの依存のみ許可**: 例 `mod/ → Robot, driver/`、`Robot → llm/, tts/, stt/`、`driver/ → share/, app/`
- **callback / listener で依存を逆転**: 低レベル層が高レベルの動作を必要とする場合は関数オブジェクトを init で受け取る。例: `IdleMotion::init(ServoMoveFn move_fn, CanMoveFn can_move_fn)`、`PlayMP3::set_event_listeners(on_start, on_stop)`
- **ヘッダの include は必要最小限**: 抽象基底ヘッダ（`TTSBase.h` 等）は具体実装ヘッダを引き込まない
- **composition root は main.cpp**: 具体クラスの new と callback の注入は main.cpp で行う

### 依存ルール（DON'T = 違反）

- ❌ **driver/ から Robot.h / Avatar.h を include** — 逆方向依存。callback で受ける
- ❌ **driver/ から `extern bool servo_home` などの上位 state を参照** — 状態は引数 or callback 経由
- ❌ **mod/ A から mod/ B のヘッダを include** — Mod は独立、`ModManager` 経由でやり取り
- ❌ **抽象基底ヘッダで具体実装ヘッダを include** — 例 `TTSBase.h` で `PlayMP3.h` を引き込まない
- ❌ **同一レイヤー内の他コンポーネントへの依存** — service → service など

### 新規ファイル配置の判断フロー

1. ハードウェアに直接アクセスするか？ → `driver/`
2. 外部 API / クラウドサービスとの通信か？ → `llm/`, `tts/`, `stt/`, `wakeword/`
3. プロジェクト全体の小さい共有ユーティリティか？ → `share/`
4. アプリケーション状態（mute / LED / idle 等）と中央集権的なロジックか？ → `app/`
5. 特定のユースケース（音楽再生 / Pomodoro 等）か？ → `mod/`
6. 上記いずれにも当てはまらず orchestration なら → `Robot.cpp` への追加

### グローバル extern の扱い

現状 `extern Avatar avatar;` / `extern Robot* robot;` / `extern bool servo_home;` 等のグローバルが多数存在する。新規に extern を増やさない方針：

- 新機能は composition root（main.cpp）で具体オブジェクトを作り、callback / 参照を渡す
- どうしても必要な状態は `app/` 配下に閉じた static として持ち、getter で公開

### 違反検出

PR レビュー時に `/layered-review` を実行することで依存違反を検出できる。`make check` (cppcheck) は文法エラーを検出するが、レイヤー違反は検出しない。

### 既存コード改修時

- 触らずに済むならそのまま（既存違反は別 PR で段階的に解消する）
- 触る必要があれば、その PR の範囲でレイヤー違反を解消する（boy scout rule）
- composition root への callback 注入が増えるのは想定内
