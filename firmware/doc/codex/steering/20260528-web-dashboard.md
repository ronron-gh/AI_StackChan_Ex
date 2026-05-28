# Web dashboard (phase 1)

## Purpose

Web UI を「カスタム指示・メモリ編集」だけのページから、デバイス全体の稼働状況を一覧できるダッシュボードに拡張する。

ユーザーは SD カードを抜き差しせずにブラウザから稼働情報・現行設定を確認できる。
編集機能はフェーズ3として別 Issue で扱う。

## Scope

- `firmware/incbin/dashboard.html` 新規
- `firmware/incbin/dashboard.js` 新規
- `firmware/incbin/index.html` 新規（ナビ）
- `firmware/src/WebAPI.cpp` — IMPORT_FILE 追加・ハンドラ追加・ `/` 配信先を index.html へ変更
- `firmware/src/WebAPI.h` 必要なら関数宣言追加

## Main changes

### 新エンドポイント

- `GET /` → index.html (ナビ)
- `GET /dashboard.html` → ダッシュボード本体
- `GET /dashboard.js` → JS（fetch + 自動更新）
- `GET /api/status` → JSON で全項目を一括返却

### `/api/status` JSON 構造

```json
{
  "system": { "fw_version": "0.21.0", "uptime_ms": 123456, "chip": "ESP32-S3", "cpu_mhz": 240, "free_heap": 12345, "min_heap": 4321 },
  "power": { "battery_level": 87, "charging": true, "voltage_mv": 4015 },
  "network": { "ssid": "MyWiFi", "ip": "192.168.0.42", "rssi": -55, "mac": "44:1B:F6:E1:DA:10" },
  "storage": { "spiffs_total": 1500000, "spiffs_used": 12345, "sd_total": 16000000000, "sd_used": 1234567 },
  "config": { "llm_type": 0, "tts_type": 0, "tts_voice": "3", "tts_model": "", "stt_type": 1, "wakeword_type": 0 },
  "role": "You are an AI robot...",
  "memory": "User Info: ..."
}
```

API キーは含めない（フェーズ3で別途）。ロール・メモリは先頭 200 文字まで。

### dashboard.html のレイアウト

シンプルカード形式（既存 personalize と同じスタイル基調）:
- ヘッダー: 🤖 AI Stack-chan Ex + ナビ（Dashboard / Personalize）
- System / Power / Network / Storage / Config / Role / Memory の各カード

### dashboard.js

```js
async function refresh() {
  const res = await fetch('/api/status');
  const data = await res.json();
  document.getElementById('fw').textContent = data.system.fw_version;
  // ... 各フィールドを反映
}
refresh();
setInterval(refresh, 5000);
```

### handle_status_json の中身

`String body` を文字列結合で組み立て。`escape_json()` ヘルパで `"` `\\` 等を escape。

## Design notes

- 既存の `IMPORT_FILE` パターンを忠実に踏襲（`personalize_html` / `personalize_js` と同じ）
- JSON 組み立ては既存の文字列ベース（ArduinoJson の動的シリアライズは避けて、シンプルに）
- 自動更新の頻度は 5 秒（heap 等の変化を見たい / 通信負荷も低い）
- ロール・メモリは長文の可能性があるため、先頭 200 文字 + "…" で打ち切り

## Verification

1. `pio run -e m5stack-cores3` でビルド成功
2. CoreS3 にアップロード後、`http://<IP>/` で ナビ表示
3. `/dashboard.html` でカード表示
4. `curl http://<IP>/api/status` で JSON が返る
5. API キーが含まれていないことを確認
6. 5 秒ごとに uptime / heap が更新されることを確認

## Rollback

- 追加ファイル削除
- `WebAPI.cpp` の追加分を削除（handleRoot を personalize_html 配信に戻す）

## Out of scope (別 Issue)

- Issue #2: アクション系 (POST /api/restart, /api/led, /api/servo-test)
- Issue #3: YAML 編集
