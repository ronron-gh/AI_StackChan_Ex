---
name: serial-debug
description: AI_StackChan_Ex の CoreS3 にシリアル経由でアクセスする際の定石操作を案内する。ポート検出、cat プロセス kill、起動ログ取得、YAML 読み書き、Wi-Fi スキャン、USB トラブル対応など。
---

# Serial Debug for AI_StackChan_Ex

CoreS3 ＋ M5StackChan キット向けのシリアル経由操作レシピ集。

## 前提

- 接続: USB-C ケーブル（**データ対応**、充電専用ケーブルは書き込み失敗の原因）
- Mac 直挿し推奨（USB ハブ経由は電力不足で書き込み中切断のリスク）
- ポート: 通常 `/dev/cu.usbmodem*`（数字部分は接続毎に変わる）

## 必須の初期処理

シリアル経由の操作前に **必ず** 既存の `cat /dev/cu.usbmodem*` プロセスを kill する。CLAUDE.md にある「ログ保存ループ」がポートを掴んでると `pio upload` / `serial_config.py` / `pio device monitor` がエラーになる。

```bash
pkill -9 -f "cat /dev/cu.usbmodem" 2>/dev/null
sleep 1
ls /dev/cu.usbmodem* 2>/dev/null   # ポート存在確認
```

## ポート検出

```bash
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
echo "Port: $PORT"
```

ポートが見えない → デバイス完全 OFF か USB 接続切れ。電源ボタン軽く 1 回押して起動 → 再確認。

## 起動ログ取得（短時間）

書き込み直後の起動シーケンスを取りたい：

```bash
sleep 25   # boot 完了待ち（Wi-Fi 接続まで含めると 25-35 秒）
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
python3 - <<'EOF'
import serial, time
ser = serial.Serial("$PORT", 115200, timeout=0.3)
deadline = time.monotonic() + 8
buf = ""
while time.monotonic() < deadline:
    chunk = ser.read(2048)
    if chunk:
        buf += chunk.decode("utf-8", errors="replace")
print(buf[-3000:] if len(buf) > 3000 else buf)
ser.close()
EOF
```

特定キーワードでフィルタしたい時は `for line in buf.split("\n"): if any(k in line for k in [...]):` パターン。

## 持続的ログ保存（USB 切断耐性あり）

CLAUDE.md にある定型：

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

**書き込み実行前には必ず止める**:

```bash
pkill -9 -f "cat /dev/cu.usbmodem"
```

## YAML 設定の読み書き

`tools/serial_config.py` 経由。

```bash
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

# 取得
python3 tools/serial_config.py --port "$PORT" get ex /tmp/ex.yaml
python3 tools/serial_config.py --port "$PORT" get basic /tmp/basic.yaml
python3 tools/serial_config.py --port "$PORT" get sec /tmp/sec.yaml

# 書き戻し
python3 tools/serial_config.py --port "$PORT" set ex /tmp/ex.yaml

# 反映のため再起動
python3 tools/serial_config.py --port "$PORT" restart
```

**機密情報の取り扱いに注意**:
- YAML には Wi-Fi パスワード・OpenAI API キーが含まれる
- `cat` / `tail` などで内容を画面表示しない
- 編集は Python / sed の in-place で（パスワード行は触らない）
- 一時ファイルは作業後に `rm -f /tmp/ex.yaml` で削除

## Wi-Fi スキャン（近隣 SSID 一覧）

外出先で iPhone テザリングに繋がらない時の救済：

```bash
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
python3 - <<'EOF'
import serial, time
ser = serial.Serial("$PORT", 115200, timeout=0.5)
time.sleep(0.3)
while ser.in_waiting:
    ser.read(ser.in_waiting)
ser.write(b":CONFIG WIFI_SCAN\n")
deadline = time.monotonic() + 20
buf = ""
while time.monotonic() < deadline:
    chunk = ser.read(512)
    if chunk:
        buf += chunk.decode("utf-8", errors="replace")
        if "\n:END" in buf or buf.rstrip().endswith(":END"):
            time.sleep(0.5)
            buf += ser.read(2048).decode("utf-8", errors="replace")
            break
start = buf.find(":WIFI_SCAN")
print(buf[start:] if start >= 0 else buf)
ser.close()
EOF
```

実例: SSID の大文字小文字違い（`iPhone17Pro` vs `iPhone17pro`）はこの方法で発見できる。

## トラブルシューティング

### 書き込み失敗 "Failed to write to target RAM" / "Device not configured"

USB スタックがおかしくなってる可能性。試す順：

1. `pkill -9 -f "cat /dev/cu.usbmodem"` でポートを掴んでるプロセスを kill
2. USB ケーブル抜き差し
3. **Mac のフタを閉じる→開く**（USB スタックがリセットされる、効くこと多い）
4. 別の USB ポートに変える（特にハブ経由してたら直挿しに）
5. **ケーブル交換**（充電専用ケーブルは NG、データ対応必須）
6. Mac 再起動

### 書き込み途中で USB 切断（60% あたりで Inflate error）

ケーブル / 電源の問題。`platformio.ini` の `upload_speed` を `1500000` → `460800` に下げると安定することがある。

### 手動ダウンロードモードに入りたい

CoreS3 の電源ボタンを **3 秒以上長押し** → 画面真っ暗・お尻 LED 赤点灯のままで停止。この状態で `pio upload` を実行すれば自動でダウンロードモード処理される。長押しすぎる（6 秒超）と完全 OFF になるので注意。

## 関連ファイル

- `tools/serial_config.py` — シリアル経由 YAML 設定ツール
- `firmware/src/share/SerialConfig.cpp` — デバイス側ハンドラ
- `Makefile` — `make monitor` でシリアルモニター起動
- `CLAUDE.md` — プロジェクト全般の運用ルール

## 推奨フロー

書き込み + 動作確認の典型例:

```bash
make upload                                # check + build + upload
sleep 25                                   # 起動待ち
# 起動ログ確認したければ短時間 python で読む
# その後は通常通り操作（ウェイクワードで会話 / Web UI 等）
```
