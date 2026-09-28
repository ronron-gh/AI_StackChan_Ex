# sanoTTS Arduino検証プロジェクトのひな形

- 目的: CoreS3上でsanoTTS推論コアを検証するため、独立したArduino版PlatformIOプロジェクトを準備する。
- 対象範囲: `C:\git\SanoTTS_Arduino_CoreS3` を新規作成する。既存firmwareの実装は変更しない。
- 主な変更ファイル: 新プロジェクトの `platformio.ini`、`src/main.cpp`、`my_cores3_16MB.csv`。空の `lib/` と `model/` を作成し、中身はユーザーが追加する。
- 実装方針: 既存の `[env:m5stack-cores3]` に合わせ、espressif32 6.3.2、Arduino、esp32s3box、PSRAM・CoreS3定義、フラッシュ設定、通信速度を維持する。lib_depsはM5Unified 0.2.15のみにする。サーボ、Wake Word、ArduinoJson固有の定義は除く。既存パーティションCSVをコピーし、単独で使用できるようにする。
- Hello World: M5Unifiedを初期化し、画面とシリアルに `Hello, World!` を表示する。loopではM5.updateと短い待機を行う。
- 設計上の注意点: 推論コア・モデル・辞書は追加しない。既存プロジェクトへの実行時の副作用はない。実機への書き込みは行わない。
- 確認方法: 作成ファイルと設定参照、コピーしたCSVの一致を静的に確認する。依存取得を伴うビルドは今回は行わない。
- 戻し方: 新規プロジェクトと本ステアリングファイルを削除することで戻せる。

状態: ユーザー承認後、作成完了。設定・Hello Worldソースを静的確認し、パーティションCSVのハッシュ一致とlib/modelが空であることを確認した。ビルド・実機書き込みは未実施。
