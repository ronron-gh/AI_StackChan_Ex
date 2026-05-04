# MCP

- [概要](#概要)
- [YAMLの設定方法](#yamlの設定方法)
- [各種MCPサーバの導入方法](#各種mcpサーバの導入方法)
  - [Brave Search](#brave-search)
  - [Googleカレンダー](#googleカレンダー)
  - [LINE Bot](#line-bot)

## 概要
下図のように外部のPC（Linux）で起動したMCPサーバをChatGPTのFunction Callingを介して使用することができます。M5Stack側はSDカード上のYAMLファイルで各MCPサーバのURLを設定するだけで、起動時に自動的に各MCPサーバからTool listを取得してFunction Callingのプロンプトに登録されます。

![](../images/mcp_overview.png)

## YAMLの設定方法
SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

下記のように、llmセクションにmcpServersのリストを追記し、各MCPサーバのURLとPortを設定します。名前("name")は任意の名前で問題ありません。"disabled"をtrueにすると、そのMCPサーバのツールリスト取得は行われず、使用不可な状態となります。

```yaml
llm:
  type: 0            # 0:ChatGPT  1:ModuleLLM  2:ModuleLLM(Function Calling)

  mcpServers:
    [
      {
        "name":"brave-search",
        "disabled":false,
        "url":"192.168.xxx.xxx",
        "port":8000
      },
      {
        "name":"google-calendar",
        "disabled":false,
        "url":"192.168.xxx.xxx",
        "port":8001
      }
    ]
```

正しく設定できると、起動時の画面に接続に成功したMCPサーバ名前が表示されます。

![](../images/mcp_config.jpg)


## 各種MCPサーバの導入方法
現在以下のMCPサーバについて動作を確認済みです。それぞれのMCPサーバの導入方法について以降で解説します。

- Brave Search（Web検索）
- Googleカレンダー
- LINE Bot（自分のLINEアカウントにメッセージを送信）

> これらに限らず、基本的にはトランスポートの方式がSSE（Server-Sent Events）に対応したMCPサーバであれば利用することができます。また、SSEに対応していないMCPサーバも、Supergatewayというツールを利用することでSSEに対応させることができます。以降の例でもSupergatewayを利用しています。

### Brave Search
UbuntuをインストールしたPCでBrave Searchを動かす手順を解説します。予めNode.jsもインストールしてください。
動作を確認したバージョンは次の通りです（最低要件ということではありません）。MCPサーバの負荷は高くないので、低スペックなPCでも問題ありません。Raspberry Pi等のSBCでも動くと思います（動作は未確認）。

動作確認環境：
- Ubuntu: 20.04
- Node.js: 22.15.0

① Brave SearchのAPIキーを取得  
[公式サイト](https://brave.com/ja/search/api/)でAPIキーを取得します。無料プランもあります。

② UbuntuにSupergatewayとBrave Searchをインストール  
```
npm install -g supergateway @modelcontextprotocol/server-brave-search
```

③ 環境変数にAPIキーを設定  
```
export BRAVE_API_KEY=************
```

④ Supergateway経由でBrave Searchを起動  
```
npx -y supergateway --stdio "npx -y @modelcontextprotocol/server-brave-search" --port 8000
```

> 参考サイト：  
> [『Supergateway 完全ガイド：stdio専用MCPサーバーをSSE/WebSocket化してLLM連携を自由自在に』](https://notai.jp/supergateway/)  
> Brave SearchをSupergateway経由で起動する方法はこちらのサイトを参考にさせていただきました。
> こちらのサイトではDockerを使って構築する方法で解説されていますので、Dockerを使いたい方はぜひこちらのサイトを参考にしてください。


### Googleカレンダー
動作確認環境：
- Ubuntu: 20.04

[こちらのリポジトリ](https://github.com/101ta28/google-calendar-mcp-server)「GoogleカレンダーMCPサーバ」をフォークさせていただき、トランスポート仕様をStdioからSSEに変更＆機能の簡略化のカスタマイズをしました。次のようにブランチ(change_to_sse)を指定してクローンし、[README](https://github.com/ronron-gh/google-calendar-mcp-server/blob/change_to_sse/README.ja.md)に従ってMCPサーバを起動してください。

```
git clone -b change_to_sse https://github.com/ronron-gh/google-calendar-mcp-server.git
```

### LINE Bot
LINE Botから自分のLINEアカウントにメッセージを送信できるMCPです。MCPサーバの作り方は[Qiita記事](https://qiita.com/motoh_qiita/items/1d308dc27f03560dd307)として投稿しましたのでご参照ください。
