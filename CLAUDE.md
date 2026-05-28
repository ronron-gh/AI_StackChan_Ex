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
