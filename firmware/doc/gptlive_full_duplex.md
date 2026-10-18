# GPTLive CoreS3 Full Duplex 検証

CoreS3内蔵のES7210／AW88298と共有I2Sクロックを直接扱う実験実装。実機での同時入出力・24 kHz・エコーの評価は未完了。

## ビルドと切替

    pio run -e m5stack-cores3-realtime-full-duplex
    pio run -e m5stack-cores3-realtime-full-duplex -t upload

AI Serviceは既存のOpenAI選択を使う。この環境でのみGPTLiveFullDuplexを生成する。CoreS3内蔵音声向けで、Core2／AtomS3R／外部Audio Module／REALTIME_API_WITH_TTSは対象外。WakeWordはこの環境では無効。

通常の半二重方式へ戻す場合は次をビルド・書込みする。

    pio run -e m5stack-cores3-realtime -t upload

## 基本の確認

1. 音量は最初は64程度以下にして開始する。直接I2SではPCMに設定音量の二乗比を適用する。スピーカー音量0でもRXは継続する。
2. 接続が完了しPlease touchが表示されたら、従来の録音開始操作をする。ログに CoreS3 TX+RX started: 24000 Hz PCM16 mono, no AEC が出ることを確認する。
3. 約2秒ごとのログで RX / TX / Sent が増えることと、発話時に inLevel が変化することを確認する。
4. 応答を再生しているときに outLevel が変化し、その間も RX / Sent が増え続けることを確認する。TXには無音の継続送信も含まれるため、TXカウンタだけで応答の再生成功とは判断しない。
5. 録音停止操作で入出力を止める。停止時にはWebSocketも切断し、停止前の応答を次回に持ち越さない。再接続後は再度操作して開始する。
6. モード停止／復帰、切断再接続、音量変更、長い応答でも停止・再開できることを確認する。

全二重版には30秒の自動録音停止と、出力無音でのマイク切替を適用しない。手動停止までセッションの入出力を継続する。直接I2S所有中の効果音・アラーム音はスキップする。非TTSビルドなので既存の非同期TTSタスクは起動しない。別TTSのRobot::speechは無効で、Webの /speech APIは503を返す。

## ログの見方

- RX / TX / Sent: 開始後のサンプル累計。24 kHzなので約2秒あたり48000 sampleが目安。実際のWebSocket受信・送信待ちによりSentは遅れる場合がある。
- RXdrop: 入力キュー8窓（160 ms）が満杯で破棄した新着窓の数。ネットワーク送信が追いつかない場合に増える。
- TXempty: TXが再生リングからデータを取り出せず無音を送った窓の数。応答のないときも増える正常なカウンタであり、常に障害を意味する値ではない。
- queued: 再生待ちPCMのbyte数。512 KiBを超える入力が来るとセッションを停止・リセットする。
- inLevel / outLevel: 直近20 ms窓のPCMピーク。outLevelはソフトウェア音量適用後、DMAへ送る窓の値で、実際の再生とはDMAの待ち時間分ずれる。

## 任意のテスト音

直接I2Sだけの確認には、検証環境のbuild_flagsへ -DGPTLIVE_FULL_DUPLEX_TEST_TONE を一時的に追加して再ビルドする。この場合はWebSocket処理とAPI送信を行わず、画面のI2S test表示でタッチすると100 msの1 kHzテスト音を再生し、その間もRXを収集する。RX / TX / inLevel / outLevelを確認し、再タッチで停止する。Sentは出力しない。確認後はフラグを外して通常の会話検証へ進む。通常ビルドでは自動テスト音は無効。

## 現時点の制約

AEC／ノイズ抑制／再生中の送信マスクは実装していない。スピーカーをマイクが拾って自己応答する可能性がある。まず音量0で入力、次に小音量で同時動作を確認し、エコー処理は別途評価する。

共有クロックは24 kHzで固定し、ES7210／AW88298の初期化をM5Unifiedから直接I2Sへ引き継ぐ。画面・電源・I2CにはM5Unifiedを引き続き使用する。実機成功はビルド成功と区別する。

参照・設計・確認結果は [ステアリング](codex/steering/20261015-gptlive-full-duplex.md) を参照。
