# Function Calling

- [Function Callingについて](#function-callingについて)
- [Function Callingで呼び出せる機能](#function-callingで呼び出せる機能)

## Function Callingについて
Function CallingはChatGPT等のLLMが提供する機能の一つです。プロンプトに関数群の定義を記述しておくと、リクエストに応じてLLMが使うべき関数の名前と引数を返答してくれます（関数はM5Stack側で実行します）。

## Function Callingで呼び出せる機能
本ソフトですでに実装しているFunction Callingの機能の一覧を下表に示します。プロンプトや関数の実装は FunctionCall.cpp にまとめています。リクエストに応じてｽﾀｯｸﾁｬﾝが関数を使いこなしてくれます。FunctionCall.cppを改造することで、新たな機能を追加するなどのカスタマイズができます。


| No. | 機能 | 使用例 | デモ | 補足 |
| --- | --- | --- | --- | --- |
| 1 | 時刻 | 「今何時？」<br>「今日は何日？」<br>「今日は何曜日？」| |
| 2 | タイマー | 「3分のアラームをセットして」<br>「1時間後にパワーオフして」<br> 「タイマーをキャンセルして」| [動画(X)](https://twitter.com/motoh_tw/status/1675171545533251584) |
| 3 | ウェイクワード登録 | 「ウェイクワードを登録」<br>「ウェイクワードを有効化」||CoreS3のみ|
| 4 | リマインダ | 「新しいリマインダを設定して」|[動画(X)](https://twitter.com/motoh_tw/status/1784956121016627358)||


