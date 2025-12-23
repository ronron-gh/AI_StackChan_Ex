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
  - ページやダイアログ内の言語は英語のみ。

#### Custom Instructions (Role)
- 構成
  - フォーム
    - カスタム指示（ロール）の入出力。
  - 設定ボタン
    - API /role_set をPOSTし、フォームの内容を設定する。
- 画面更新時の動作
  - API /role_get をPOSTし、現在設定されているカスタム指示を取得してフォームに表示する。 
