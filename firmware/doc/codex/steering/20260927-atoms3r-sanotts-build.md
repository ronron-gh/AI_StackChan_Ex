# AtomS3RでのSanoTTS-jp試験ビルド

## 目的

ユーザーが追加した`m5stack-atoms3r-realtime-sanotts`環境でSanoTTS-jpをビルドできるようにし、AtomS3R実機での動作確認を可能にする。

## 対象範囲

- SanoTTS用ビルド前検証にあるCoreS3限定ガードの解除
- `m5stack-atoms3r-realtime-sanotts`環境のビルド確認
- AtomS3Rでの実機動作はユーザーが確認する
- SanoTTSの合成・再生処理、メモリ確保、タスク構成は変更しない

## 主な変更ファイル

- `build_scripts/platformio_sanotts_validate.py`
- `doc/codex/steering/20260927-atoms3r-sanotts-build.md`

ユーザーが変更中の`platformio.ini`と`src/share/Version.h`は、本作業では編集しない。

## 実装方針

- `$BOARD == esp32s3box`を要求している事前検証を削除し、`m5stack-atoms3r`ボードでも処理を継続できるようにする。
- SanoTTSライブラリ、モデル、辞書、生成スクリプトの存在確認とSHA-256検証は従来どおり維持する。
- ユーザー作成済みの`m5stack-atoms3r-realtime-sanotts`環境をそのまま使い、追加の環境定義やビルドフラグ変更は行わない。
- CoreS3専用というエラーメッセージもガードと一緒に削除する。
- AtomS3R対応を確定するドキュメント更新は、実機確認後に行う。

## 設計上の注意点

- AtomS3RもESP32-S3とPSRAMを使用するため、現在の`SAAN_PIE=1`、`BOARD_HAS_PSRAM`、PSRAM割り当て処理を試せる構成である。
- ビルド成功は、内蔵スピーカーでの再生、マイクとの排他、推論速度、PSRAM容量、Realtime APIとの同時動作を保証しない。
- 現在の検証スクリプトからボード制限を外すため、将来ほかの環境が誤ってSanoTTS設定を継承した場合もボード名では拒否されない。必要ファイルの不備は引き続き拒否される。
- 問題が発生した場合は実機ログを基に、AtomS3R固有のガードまたは設定を改めて追加する。

## 確認方法

- `m5stack-atoms3r-realtime-sanotts`をPlatformIOでビルドし、従来のCoreS3限定エラーが発生しないことを確認する。
- モデル、辞書、ライブラリの検証スクリプトが実行されることをビルドログで確認する。
- 変更差分がボード名ガードの削除とステアリングファイルに限定されることを確認する。
- ユーザーがAtomS3R実機で起動、Realtime API接続、日本語発話、音切れ、リップシンク、マイク復帰を確認する。

## 確認結果

- CoreS3限定ガードの解除後、モデル・辞書の検査とSanoTTSコアを含むコンパイル・リンクは完了した。
- リンク後のサイズ検査で、ファームウェア4,459,501バイトに対して現在のAtomS3R用アプリ領域が3,342,336バイトのため、133.4%としてビルドが停止した。
- 静的RAMは85,672バイト／327,680バイト（26.1%）であり、今回のビルド阻害要因はフラッシュのパーティション容量である。
- 実機確認へ進むには、8MBフラッシュ内でアプリ領域を拡張するAtomS3R用パーティション構成を別途検討する必要がある。

## タスクリスト

- [x] AtomS3R用環境とCoreS3限定ガードの位置を確認する
- [x] AtomS3R側のESP32-S3・PSRAM・SanoTTSビルドフラグを確認する
- [x] CoreS3限定の事前検証を削除する
- [x] `m5stack-atoms3r-realtime-sanotts`をビルドし、リンク後の容量超過を確認する
- [x] 差分と既存検証の維持を確認する
- [ ] AtomS3R実機で動作確認する
