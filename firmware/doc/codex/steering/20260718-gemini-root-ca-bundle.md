# Gemini Live Root CA バンドル化

## 目的

Gemini Live API 接続時に、利用環境によって Google から提示される証明書チェーンが異なり、単一の信頼証明書では TLS 検証に失敗する問題を軽減する。
`src/rootCA/rootCAgoogleGemini.h` に複数の Google 向け CA 証明書を連結した PEM バンドルを組み込み、mbedTLS が提示されたチェーンに対応する信頼アンカーを選択できるようにする。

## 対象範囲

- Gemini Live API の WebSocket TLS 接続で使用する組み込み CA 証明書
- 対象ホスト: `generativelanguage.googleapis.com:443`
- `WebSocketsClient::beginSslWithCA()` へ渡す CA データの内容

Custom OpenAI Endpoint 用の `customRootCAFile` / `customRootCAFiles`、Google Speech-to-Text、OpenAI Realtime API、TTS の CA 設定は変更しない。

## 主な変更ファイル

- `src/rootCA/rootCAgoogleGemini.h`
  - 単一証明書を、複数の PEM 証明書を連結した Google 向け CA バンドルへ変更する。
  - 各証明書の Subject、Issuer、有効期限、SHA-256 fingerprint、取得元をコメントで記録する。
- `doc/codex/steering/20260718-gemini-root-ca-bundle.md`
  - 本作業の方針と確認方法を記録する。

既存の接続処理を変更する必要がなければ、`src/llm/Gemini/GeminiLive.cpp` は変更しない。

## 実装方針

1. Google Trust Services の公式リポジトリを基準に、実際に報告された複数の証明書チェーンを検証できる CA 証明書を選定する。
2. `openssl s_client` が表示したサーバー証明書をそのまま採用せず、Subject、Issuer、fingerprint を照合して証明書の位置付けを確認する。
3. 選定した証明書を、各 `BEGIN CERTIFICATE` / `END CERTIFICATE` ブロックの間に改行を入れて1つの文字列へ連結する。
4. 既存の変数名 `root_ca_google_gemini` と `beginSslWithCA()` の呼び出しを維持し、変更範囲を証明書ヘッダーに限定する。
5. 古い証明書を無条件に残すのではなく、Google公式情報、実環境で観測されたチェーン、証明書の有効期限を根拠に必要な証明書だけを含める。
6. CAを無効化する `setInsecure()` は使用しない。

## 設計上の注意点

- 複数CAの組み込みは、環境ごとに異なる証明書チェーンへ対応するための信頼候補を増やすものであり、証明書検証を無効化するものではない。
- バンドルに含まれない将来のGoogle証明書チェーンには対応できないため、ファームウェア更新時にGoogle Trust Servicesの公開情報を再確認する。
- 端末時刻の未同期、TLSインスペクションによる独自CAへの差し替え、古いmbedTLSの暗号方式非対応は、本変更だけでは解決しない。
- Googleは特定の中間CAやRoot CAの固定を避けるよう案内している。一方、組み込み機器では一般的なOSのRoot Storeを自動更新できないため、本実装では対象をGoogle向けの必要最小限のCAバンドルに限定し、ファームウェア更新で保守する。
- 複数PEMによるフラッシュ使用量と、TLSハンドシェイク時のヒープ使用量増加を確認する。
- APIキー、Wi-Fi情報などの秘密情報は証明書ヘッダーやログへ追加しない。

## 確認方法

### 証明書の静的確認

- 各PEMをOpenSSLで解析できることを確認する。
- Subject、Issuer、有効期限、SHA-256 fingerprintがGoogle公式情報と一致することを確認する。
- PEMブロック数と区切りを確認し、文字列の欠落や重複定義がないことを確認する。

### 接続確認

- 従来接続できていた環境でGemini Live APIへ接続し、回帰がないことを確認する。
- エラー報告があった環境で接続し、`X509 - Certificate verification failed` が解消することを確認する。
- 可能であれば、各環境で次のコマンドにより提示チェーンを採取し、バンドル内のCAへ検証経路が到達することを確認する。

```sh
openssl s_client -connect generativelanguage.googleapis.com:443 -servername generativelanguage.googleapis.com -showcerts -verify_return_error
```

### ビルド確認

- `m5stack-core2-realtime`
- `m5stack-cores3-realtime`
- `m5stack-atoms3r-realtime`

依存ライブラリ取得などでネットワークアクセスが必要な場合は、実行前にユーザーの了承を得る。

## 戻し方

`src/rootCA/rootCAgoogleGemini.h` のCA文字列を変更前の単一証明書へ戻す。接続処理や設定構造は変更しないため、証明書ヘッダーの差し戻しだけで元の動作へ復帰できる。

## 残リスク

- Google側で新しいCAや証明書チェーンへ切り替わった場合は、バンドルの再更新が必要になる。
- 報告環境でTLSインスペクションが行われている場合、Google公式CAの追加だけでは接続できない。
- 実機を用意できない環境については、ビルドと証明書チェーンの静的検証までとなる。
