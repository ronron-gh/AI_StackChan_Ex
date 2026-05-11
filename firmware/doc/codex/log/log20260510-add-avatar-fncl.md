# Add Avatar Expression Function Calling Conversation Log

Date: 2026-05-10

This log records the chat-based flow for the first Codex-assisted feature addition: realtime avatar expression control via Function Calling.

## User

Realtime AI会話用のビルド（env:m5stack-core2-realtime等）に限り、Avatarの表情をAIが感情に合わせてリアルタイムに変更できるようにしたいです。Function Callingで実装すれば実現できると考えています。表情の変更自体は簡単で、Avatar::setExpression()で出来ます。主な変更対象はFunctionCall.cppになると思います。方針を立てられそうですか？

## Assistant

方針は立てられそうです。まず `AGENTS.md` を前提に、Realtime 側の Function Calling が今どう流れているかと、`Avatar::setExpression()` へ安全に届かせる経路を確認します。今回はまだ実装せず、影響範囲と設計の切り方を見ます。

`FunctionCall.cpp` には既に `Avatar.h` とグローバル `avatar` が入っていて、Function Calling から表情を触る前例もあります。次に `Expression` の列挙と Realtime 専用ビルド条件、doc の書き方を見て、最小構成の案にします。

重要な点がひとつ見えました。`json_Functions` は Realtime ChatGPT だけでなく通常 ChatGPT や Gemini Live 側からも参照されているので、単に関数を足すと対象外ビルドにも露出します。Realtime 専用にするなら `#if defined(REALTIME_API)` で宣言と実行分岐を閉じるのが筋です。

はい、方針は立てられます。かなり素直に入れられそうです。

**実装方針**

`FunctionCall.cpp` に Realtime API ビルド限定の function を追加するのがよさそうです。

追加する関数案:

```json
{
  "name": "set_avatar_expression",
  "description": "Change Stack-chan's facial expression to match the current emotion.",
  "parameters": {
    "type": "object",
    "properties": {
      "expression": {
        "type": "string",
        "enum": ["neutral", "happy", "angry", "sad", "doubt", "sleepy"]
      }
    },
    "required": ["expression"]
  }
}
```

実行側では `expression` 文字列を `m5avatar::Expression` に変換して、`avatar.setExpression(Expression::Happy);` のように呼びます。

`json_Functions` は通常の `ChatGPT.cpp` や `GeminiLive.cpp` からも読まれているので、今回の要件どおり「Realtime AI会話用ビルドに限り」にするため、tools 定義、`exec_calledFunc()` 内の分岐、`FunctionCall.h` のメソッド宣言を `#if defined(REALTIME_API)` で囲む方針にしました。

## User

方針はOKです。実装する前に、この方針を作業単位のステアリングファイルとして、doc/codex/steering/20260510-add-avatar-fncl.mdとして保存してください。

## Assistant

了解です。実装前の判断を残す用途なので、方針・対象・制約・作業単位が後で追える形でステアリングファイルを追加します。まず既存の `doc/codex` 配下を確認します。

作業ツリーは clean です。`doc/codex/steering` を作成して、今回の実装方針を Markdown で保存します。

ステアリングファイルを追加しました。最後に内容と差分状態を確認します。

保存しました。

ステアリングファイルはこちらです。

`doc/codex/steering/20260510-add-avatar-fncl.md`

内容としては、以下を記録しています。

- Realtime API ビルド限定にすること
- `set_avatar_expression` function calling の schema 案
- `FunctionCall.cpp` / `FunctionCall.h` / `fw_design.md` を主対象にすること
- `REALTIME_API` で schema と実行分岐を閉じること
- `Expression` の文字列マッピング
- ビルド確認方針

## User

確認しました。では、実装に移りたいと思います。

## Assistant

了解です。ステアリングに沿って、まず対象を `FunctionCall` まわりに絞って実装し、最後に Realtime ビルドで確認します。作業ツリーの状態を見てから編集に入ります。

対象ファイルは想定どおりで、未追跡はステアリングファイルだけです。これから `REALTIME_API` 条件で閉じた形で、schema、実行分岐、メソッドを追加します。

文字化けを含む行をコンテキストに使ったためパッチが当たりませんでした。日本語コメントや既存文字列を壊さないよう、ASCII の近傍だけを使って小さく分けて入れます。

FunctionCall 側の実装を入れました。次に `fw_design.md` に、Realtime 限定の設計意図と関数名だけを短く残します。

設計メモも追記しました。これから差分を確認して、まず `m5stack-core2-realtime` をビルドします。

`pio` コマンドが PATH に見つかりませんでした。PlatformIO の別名やローカル環境があるかを確認して、使える入口を探します。

PlatformIO はユーザー環境の仮想環境にありました。直接パス指定で `m5stack-core2-realtime` をビルドします。

承認付きのビルドは 2 分でタイムアウトしました。大きいファームウェアなので、もう少し長めのタイムアウトで同じビルドを続けます。

`m5stack-core2-realtime` はビルド成功しました。Realtime 限定の `#if` が通常ビルドを壊していないことも確認したいので、軽く `m5stack-core2` もビルドします。

通常の `m5stack-core2` もビルド成功しました。最後に差分と作業ツリーを確認して、今回の変更範囲を整理します。

実装まで完了しました。

`REALTIME_API` ビルド限定で `set_avatar_expression` Function Calling を追加し、AI が `neutral` / `happy` / `angry` / `sad` / `doubt` / `sleepy` を指定して `Avatar::setExpression()` を呼べるようにしました。

確認結果:

- `m5stack-core2-realtime`: ビルド成功
- `m5stack-core2`: ビルド成功

## User

m5stack-core2-realtimeで実機動作を確認できました。ここまでで一旦コミットしてください。ブランチはこのままで大丈夫です。

## Assistant

実機確認まで取れたなら良い区切りです。いまの `codex_startup` ブランチのまま、今回の実装とステアリングファイルをまとめてコミットします。まず差分対象を確認します。

差分は今回の機能追加とステアリングファイルだけです。これらをステージしてコミットします。

コミットできました。最後にブランチ、最新コミット、作業ツリーの状態を確認します。

コミットしました。

- Branch: `codex_startup`
- Commit: `cc201d1 Add realtime avatar expression function call`

## User

プロンプト内の機能説明だけでは、積極的にset_avatar_expression機能を使ってくれないことがあるようなので、システムプロンプトで明確に指示したいです。システムプロンプトは現在、LLMBase.cpp内のsystemRole_memory (またはsystemRole_noMemory)として、memory機能専用の定義があります。set_avatar_expression機能についてもこのような定義を追加したいのですが、うまい方法はありますか？

## Assistant

状況わかります。Function Calling は tool 定義だけだと「使える機能」止まりになりがちなので、Realtime の system instructions 側に「会話中に表情を切り替えること」を明示するのが自然です。まず既存の `systemRole_memory/noMemory` の組み立て方を確認します。

既存構造では `RealtimeChatGPT::load_role()` が `systemRole_memory/noMemory` を選んで、session.update の `instructions` に結合しています。なので追加するなら「メモリ方針」と「表情制御方針」を別の String 定義に分けて、Realtime 側だけで合成するのが一番きれいです。

おすすめは、memory 用の system role と Realtime 表情制御用の system role を分離して合成する形です。

`LLMBase.cpp` に次のような定義を追加する案を提示しました。

```cpp
const String systemRole_realtimeAvatarExpression =
  "Use the set_avatar_expression tool proactively to match Stack-chan's facial expression to the emotional tone of the conversation. "
  "Call it when your emotion changes or when the user's emotion suggests a suitable reaction. "
  "Use neutral, happy, angry, sad, doubt, or sleepy.";
```

そして `LLMBase.h` に `extern` を追加し、`RealtimeChatGPT::load_role()` では memory/noMemory 選択を維持したまま後ろに足す方針を提案しました。

## User

なるほど、その方法が良さそうです。実装を進めてください。

## Assistant

了解です。前回の機能に積み上げる形で、表情制御用の system role を独立定義し、RealtimeChatGPT の instructions にだけ合成します。まず作業ツリーを確認してから編集します。

作業ツリーは clean です。`LLMBase` に Realtime 表情制御の共通定義を追加し、`RealtimeChatGPT::load_role()` で memory 指示に追記する形にします。

実装を入れました。次に設計メモへ「tool 定義だけでなく Realtime instructions に明示指示を合成する」ことを追記します。

設計メモも更新しました。差分を確認して、`m5stack-core2-realtime` をビルドします。

`m5stack-core2-realtime` のビルドは成功しました。最後に状態をまとめて確認します。

実装しました。

変更内容は、表情制御の指示を memory 系の system role から分離して追加する形です。

- `LLMBase.cpp` に `systemRole_realtimeAvatarExpression` を追加
- `LLMBase.h` に `extern` 宣言を追加
- `RealtimeChatGPT.cpp` の `load_role()` で、`systemRole_memory` または `systemRole_noMemory` に表情制御指示を追記
- `fw_design.md` に設計メモを追記

確認:

- `m5stack-core2-realtime`: ビルド成功

## User

GeminiLiveもリアルタイムAPIに分類されるため、GeminiLiveのload_role()にも同じ対応を入れてほしいです。

## Assistant

了解です。Gemini Live もリアルタイム系として同じ system role を合成するのが筋ですね。まず `GeminiLive::load_role()` の構造を確認して、ChatGPT 側と同じ最小変更で入れます。

`GeminiLive::load_role()` は RealtimeChatGPT とほぼ同じ形でした。ここにも同じ `systemRole_realtimeAvatarExpression` を追記し、設計メモの表現を ChatGPT 限定から Realtime 系全体に少し直します。

GeminiLive 側にも同じ合成を入れました。続けて `fw_design.md` のスコープ説明を Gemini Live も含む表現に調整します。

ドキュメントも Realtime 系全体の表現にしました。差分を確認してから、少なくとも `m5stack-core2-realtime` をビルドします。GeminiLive はこの env でもコンパイル対象に入っているので確認できます。

ビルド成功しました。最後に変更状態を確認して、何を入れたかまとめます。

GeminiLive にも同じ対応を入れました。

確認:

- `m5stack-core2-realtime`: ビルド成功

## User

実機で動作確認できましたので、コミットしてください。

## Assistant

了解です。実機確認済みの system role 強化分として、現在の5ファイルをまとめてコミットします。まず staged 前の状態を確認します。

差分は今回のプロンプト指示追加だけです。ステージしてコミットします。

コミットできました。最後に最新コミットと作業ツリーを確認します。

コミットしました。

- Branch: `codex_startup`
- Commit: `10f1e6c Prompt realtime models to use avatar expressions`

## User

今回の機能追加は完了です。今回のように実装に入る前にステアリングファイルを作成するルールをAGENTS.mdに記載して、次回から適用できるようにしてください。

## Assistant

了解です。今回の進め方はかなり良い型になったので、`AGENTS.md` に「実装前に作業単位のステアリングファイルを作る」ルールとして追加します。まず現在の内容を確認して、最小限で追記します。

作業ツリーは clean です。`変更手順` にステアリングファイル作成を明記し、`doc/codex/steering/YYYYMMDD-*.md` の命名と内容も短く追記します。

AGENTS.md にルールを追加しました。内容と差分を確認します。

`AGENTS.md` に追記しました。

追加したルール:

- 実装前に `doc/codex/steering/YYYYMMDD-short-topic.md` を作成する
- 目的、対象範囲、主な変更ファイル、実装方針、注意点、確認方法を書く
- 方針変更があれば同じファイルを更新する
- ステアリングファイルは作業コミットに含める

## User

忘れていましたが、今回は初めてのCodexでの機能追加なので、あとで見返せるよう、表情制御機能の追加の一連のチャットベースのやり取りをdoc/codex/log/log20260510-add-avatar-fncl.mdとして保存してください。

