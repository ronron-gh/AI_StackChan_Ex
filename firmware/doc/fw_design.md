# FW Design Information
Notes on FW design, etc.

## Task

| Task name | function | Stack size [bytes] | Priority |
| --- | --- | --- | --- |
| loopTask | Arduino loop task | 8192 | 1 |
| drawLoop | Avatar control | 4 * 1024 | 1 |
| saccade | Avatar control | 1024 | 2 |
| updateBreath | Avatar control | 1024 | 2 |
| blink | Avatar control | 1024 | 2 |
| lipSync | Lip Sync for avatar| 2048 | 1 |
| servo | Servo control synchronized with the avatar | 2048 | 1 |
| battery_check | Battery level check | 2048 | 1 |
| asyncTtsStreamTask | TTS streaming play | 5 * 1024 | 2 |

## Web App
WebAPI.cpp のインラインアセンブラ(マクロ：IMPORT_FILE)で incbinフォルダ内のhtmlファイルやjsファイルをプログラム領域に埋め込む。

### Personalize page
- ファイル構成
  - incbin/personalize.html
  - incbin/personalize.js
- 言語
  - ページやダイアログ内の言語は英語版のみ。
- 画面構成
  - Role (Custom Instructions)

#### Role (Custom Instructions)
- 構成
  - フォーム
    - ロール（カスタム指示）の入出力。
  - 設定ボタン
    - API /role_set をPOSTし、フォームの内容を設定する。
- 画面更新時の動作
  - API /role_get をPOSTし、現在設定されているカスタム指示を取得してフォームに表示する。 

#### Memory
- 構成
  - フォーム
    - 取得した記憶内容を表示。
  - Clearボタン
    - API /memory_clear をPOSTし、記憶内容を消去する。
    - 消去を実行する前に、OK/Cancelのダイアログを表示して本当に消去してよいかを確認する。
- 画面更新時の動作
  - API /memory_get をPOSTし、記憶内容を取得してフォームに表示する。
 