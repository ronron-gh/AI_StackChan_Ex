# OpenAI互換エンドポイントの利用方法

llama.cppやOllama、LM Studio等が提供するOpenAI互換API（Chat Completions）のサーバを、ChatGPTの代わりにLLMとして利用することができます。

- [概要](#概要)
- [YAMLの設定方法](#yamlの設定方法)
- [HTTPSエンドポイントの利用方法（ルートCA証明書）](#httpsエンドポイントの利用方法ルートca証明書)
- [動作仕様・制限事項](#動作仕様制限事項)

## 概要
SC_ExConfig.yamlでllmのtypeを4にし、model（必須）とcustomEndpointを設定すると、ChatGPT（type 0）と同じ仕組みのまま接続先だけをOpenAI互換サーバに切り替えられます。自宅のPC等でLLMサーバを動かせば、会話内容をクラウドに送らずにAI会話機能を利用できます。

## YAMLの設定方法
SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

```yaml
llm:
  type: 4                                                          # 4:OpenAI互換エンドポイント
  model: "Gemma-4-31B-it"                                          # サーバで使用するモデル名（必須）
  customEndpoint: "http://192.168.X.XXX:8080/v1/chat/completions"  # Chat CompletionsのURL
```

- model：必須です。空欄のままだとリクエストは拒否されます。なお、type 0（ChatGPT）でもmodelを指定でき、その場合はデフォルトのgpt-4oを上書きします。
- customEndpoint：Chat CompletionsのURLをパス（/v1/chat/completions）まで含めて指定します。https://のURLを指定する場合は次節のルートCA証明書の設定が必要です。空欄の場合は通常通りapi.openai.comに接続します。
- APIキーは、SC_SecConfig.yamlのaiserviceに設定した値が Authorization: Bearer ヘッダとして送信されます。認証不要のサーバの場合は任意の文字列で問題ありません。

## HTTPSエンドポイントの利用方法（ルートCA証明書）
https://のcustomEndpointを利用する場合は、サーバ証明書の検証に使用するルートCA証明書の設定が必要です。PEM形式のルートCA証明書をテキストファイルとしてSDカードに保存し、そのパスをcustomRootCAFileに設定してください。設定されていない場合、リクエストは拒否されます（api.openai.comへフォールバックすることはありません）。

```yaml
llm:
  type: 4
  model: "Gemma-4-31B-it"
  customEndpoint: "https://my-llm.example.com/v1/chat/completions"
  customRootCAFile: "/customRootCA.pem"
```

証明書ファイルの中身の例（改行を保持したPEM形式）：
```
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqh...
-----END CERTIFICATE-----
```

複数のルートCAを信頼させたい場合は、customRootCAFilesにパスのリストを指定します（customRootCAFileとの併用も可能です）。各ファイルの証明書は連結され、すべてルートCAとして信頼されます。

```yaml
  customRootCAFiles:
    - "/customRootCA.pem"
    - "/customRootCA2.pem"
```

> AtomS3RはSDカード非対応のため、YAMLファイルと同様に証明書ファイルもSPIFFSに書き込んでください。書き込み方法は[こちら](./atoms3r.md)を参照ください。

> 起動時のシリアルログにcustomEndpointと読み込んだ証明書の数が出力されるので、設定の確認に利用できます。

## 動作仕様・制限事項
- ストリーミング応答（SSE）には未対応です。type 4ではリクエストで stream: false を明示的に指定します。
- 設定に不備がある場合はリクエストを送信せず、ｽﾀｯｸﾁｬﾝが吹き出しでエラーを表示します（起動時のシリアルログにもエラーが出力されます）。
  - modelが空欄 →「モデル未設定」
  - https://のcustomEndpointなのにルートCAが未設定 →「CA未設定」
