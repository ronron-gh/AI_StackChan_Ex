# Function Calling device setting query

## Purpose

PR#36 で追加された画面の明るさとスピーカー音量の設定 Function Calling に対して、現在値を取得する Function Calling を追加する。
これにより、LLM が「少し暗くして」「音量を少し大きくして」のような相対指示を処理する前に現在値を確認できるようにする。

## Scope

- 対象は Function Calling の定義と実行処理に限定する。
- 音量は既存の `robot->spk_volume` を現在値として返す。
- 明るさは `M5.Display.getBrightness()` を現在値として返す。
- YAML 設定値の永続化、Web API、UI、起動時設定の変更は行わない。

## Main files

- `src/llm/ChatGPT/FunctionCall.cpp`
- `src/llm/ChatGPT/FunctionCall.h`

## Implementation plan

- `json_Functions` に `get_volume` と `get_brightness` を追加する。
- `exec_calledFunc()` にそれぞれの分岐を追加する。
- `FunctionCall` に `get_volume()` と `get_brightness()` を追加する。
- レスポンスは LLM が次の設定値を計算しやすいよう、0-255 の現在値を明示して返す。

## Design notes

- `set_volume()` は `robot->spk_volume` と `M5.Speaker` を更新しているため、取得も `robot->spk_volume` を正とする。
- `set_brightness()` は表示ドライバへ直接反映しているため、取得は `M5.Display.getBrightness()` を使う。
- 取得 Function は副作用を持たせない。
- 相対変更そのものは LLM 側が `get_*` の結果から `set_*` の値を決める前提とし、今回の変更では `increase_*` のような別 Function は追加しない。

## Verification

- 可能なら `pio run -e m5stack-core2-realtime` でビルド確認する。
- PlatformIO 依存取得などでネットワークが必要になった場合は、実行前に確認する。
- ビルドが難しい場合は、少なくとも Function 定義 JSON と C++ 分岐の静的確認を行う。
