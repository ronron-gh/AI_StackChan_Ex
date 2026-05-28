# Idle timeout based sleep / wake

## Purpose

一定時間操作（タッチ・ボタン・ウェイクワード）がない場合、自動的に省エネモードに入る機能を追加する。

操作（タッチ・ウェイクワード）で復帰する。

## Scope

- `firmware/src/main.cpp`
  - グローバルに `last_activity_ms`, `is_idle_state` を追加
  - `loop()` の冒頭で経過時間を判定して sched_fn_sleep / sched_fn_wake を呼ぶ
  - タッチ・ボタン・フリック等の入力検出時に `last_activity_ms` を更新

- 設定はハードコード（必要なら後で YAML 化）：
  - タイムアウト: 5 分（`IDLE_TIMEOUT_MS = 5 * 60 * 1000`）

## Main changes

### 既存資産の流用

- `MySchedule.cpp` の `sched_fn_sleep()` / `sched_fn_wake()` をそのまま再利用
  - `sched_fn_sleep`: CPU 80MHz、LCD 輝度 8、表情 Sleepy
  - `sched_fn_wake`: CPU 240MHz、LCD 輝度 127、表情 Neutral

### main.cpp

```cpp
// 追加: idle 状態管理
static const uint32_t IDLE_TIMEOUT_MS = 5 * 60 * 1000;  // 5分
static unsigned long last_activity_ms = 0;
static bool is_idle_state = false;

void notify_activity() {
  last_activity_ms = millis();
  if (is_idle_state) {
    sched_fn_wake();
    is_idle_state = false;
  }
}

// loop() 冒頭
if (!is_idle_state && (millis() - last_activity_ms > IDLE_TIMEOUT_MS)) {
  sched_fn_sleep();
  is_idle_state = true;
}

// タッチ・ボタン・フリック等の入力検出時に notify_activity() を呼ぶ
```

### ウェイクワード検出時

`AiStackChanMod.cpp` でウェイクワードまたは display_touched が呼ばれた際にも `notify_activity()` を呼ぶ。
（main.cpp の入力検出と重複してもよい。冪等）

## Design notes

- タイムアウト 5 分は妥当な初期値（M5公式版も同様）
- 復帰の最重要動線はタッチ。ウェイクワードでも復帰できる
- sched_fn_wake は MySchedule.cpp に既存のため、ヘッダで extern 宣言して呼ぶ
- 既存のスケジュール機能（時刻ベース）と競合しないよう、is_idle_state で状態管理

## Verification

- ビルド成功
- 起動から 5 分経過 → 画面暗くなり、CPU クロックダウン
- タッチ → 即座に明るくなる
- ウェイクワード「スタックチャン」→ 復帰

## Rollback

- main.cpp の追加コードと extern 宣言を削除
- `git revert <commit>` でも復帰可能

## Out of scope

- タイムアウト時間の YAML 設定化
- 完全 deep sleep（ESP32 deep sleep）— 起動時間がかかるため見送り
- バッテリー駆動時のみ idle 化など条件分岐
