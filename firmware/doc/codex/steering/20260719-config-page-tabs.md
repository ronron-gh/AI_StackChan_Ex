# Config Page Tabs

## 目的

Web UI の Config ページを Wi-Fi、AI Service、Servo の3つのタブに整理し、設定項目を探しやすくする。
Save、Reload、Restart はタブ共通の操作としてタブ領域の外に置き、Save では表示中のタブに限らず全タブの入力値をまとめて保存する。

## 対象範囲

- `incbin/config.html` のタブUIと設定パネルの構造、スタイル
- `incbin/config.js` のタブ切り替え処理
- `doc/fw_design.md` の Config page 設計記述

Web API、JSON形式、YAML生成処理、設定項目そのものは変更しない。

## 主な変更ファイル

- `incbin/config.html`
- `incbin/config.js`
- `doc/fw_design.md`
- `doc/codex/steering/20260719-config-page-tabs.md`

## 実装方針

1. Configフォーム内に `Wi-Fi`、`AI Service`、`Servo` のタブボタンを追加する。
2. 既存の3つの設定領域を、それぞれ対応するタブパネルとして構成する。
3. 初期表示は `Wi-Fi` タブとする。
4. タブ選択時は対応するパネルのみ表示し、ほかのパネルは非表示にする。非表示パネル内の入力要素はDOM上に保持し、入力値を失わないようにする。
5. Save、Reload、Restart とステータス／エラーメッセージはタブパネルの外に配置する。
6. Save は既存の `collectConfig()` を使用し、全タブの入力値から従来どおり1つのJSONを生成して `/config` にPOSTする。
7. Reload は全設定を再取得して全タブへ反映し、現在選択中のタブは維持する。
8. タブには `role="tablist"`、`role="tab"`、`role="tabpanel"` と対応するARIA属性を設定する。クリックに加え、左右矢印、Home、Endキーでタブを移動できるようにする。
9. 狭い画面でも3つのタブ名が収まり、共通操作ボタンが既存どおり扱えるレスポンシブ表示にする。

## 設計上の注意点

- タブ切り替えだけではフォーム送信やAPI通信を行わない。
- タブを切り替えても未保存の入力値を保持する。
- `fieldset` と見出しの意味構造を維持しつつ、選択中タブが視覚的に明確になるようにする。
- パスワード表示切り替え、Servo Type変更確認、Pin Presetなど既存のイベント処理を維持する。
- Save成功後のSDカード取り外し案内とRestart確認ダイアログを維持する。
- 埋め込みファイル容量への影響を小さくし、外部ライブラリは追加しない。

## 確認方法

- 初期表示で Wi-Fi タブだけが表示されること。
- 各タブをクリックおよびキーボードで切り替えられること。
- タブをまたいで入力した値が、切り替え後も保持されること。
- Save時のJSONに Wi-Fi、AI Service、Servo の全設定が含まれること。
- Reloadで全タブの値が更新され、選択中のタブが維持されること。
- Save、Reload、Restartがどのタブからも操作できること。
- デスクトップ幅とスマートフォン幅で、タブ、入力欄、操作ボタンに重なりやはみ出しがないこと。
- `m5stack-core2-realtime` と `m5stack-cores3-realtime` をビルドし、埋め込みWeb UIを含めてコンパイルできること。

## 戻し方

タブボタンとタブ切り替え処理を削除し、3つの設定領域を常時表示へ戻す。APIおよび保存形式は変更しないため、設定データの移行や復元は不要。
