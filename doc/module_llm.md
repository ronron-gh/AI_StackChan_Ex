
# Module LLMを使用する際の設定方法
![](../images/module_llm.jpg)

写真のようにｽﾀｯｸﾁｬﾝにModule LLMをスタックして使用する際の設定方法について記載します。

- [platformio.iniの設定](#platformioiniの設定)
- [YAMLの設定](#yamlの設定)
  - [シリアル通信PIN](#シリアル通信pin)
  - [ウェイクワード (KWSを使う場合のみ)](#ウェイクワード-kwsを使う場合のみ)
  - [サーボPINの制限](#サーボpinの制限)
- [付録A. Function Callingの実装方法](#付録a-function-callingの実装方法)
  - [(1) Hugging Faceのモデルをaxmodelに変換する](#1-hugging-faceのモデルをaxmodelに変換する)
  - [(2) モデルをStackFlowに取り込む](#2-モデルをstackflowに取り込む)
  - [(3) ロールにFunctionの説明を記述する](#3-ロールにfunctionの説明を記述する)
  - [YAMLの設定](#yamlの設定-1)
- [付録B. その他、Module LLMのカスタマイズ方法](#付録b-その他module-llmのカスタマイズ方法)


## platformio.iniの設定
以下の設定を追加します。

```
build_flags=
    -DUSE_LLM_MODULE
lib_deps =
    m5stack/M5Module-LLM@1.0.0
```

Core2の場合は[env:m5stack-core2-llm]、CoreS3の場合は[env:m5stack-cores3-llm]を選択すると上記設定が有効になります。

## YAMLの設定

### シリアル通信PIN
M5 CoreとModule LLMとの間のシリアル通信で使用するPINの設定です。  
Core2とCoreS3でPINが異なるためご注意ください。

SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

```
moduleLLM:
  # Serial Pin
  # Core2 Rx:13,Tx:14
  # CoreS3 Rx:18,Tx:17
  rxPin: 13
  txPin: 14
```

### ウェイクワード (KWSを使う場合のみ)

SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

```
wakeword:
  type: 1                            # 0:SimpleVox  1:ModuleLLM(KWS)
  keyword: "HI STUCK"                # ウェイクワード
```

### サーボPINの制限
下記の通り、Core2、CoreS3ともにポートCのPINがModule LLMのシリアル通信に当たるため使えません。  
また、CoreS3はPIN2がカメラ用クロックに当たるためポートAも使えません。  
([トップページ](../README.md)で記載の通り、サーボ設定はSC_BasicConfig.yamlで行ってください。)

■Core2
| |PIN|使用状況|
|---|---|---|
|**ポートA**|**32/33**|**サーボに割り当て可**|
|**ボートB**|**26/36**|**サーボに割り当て可**|
|ポートC|13/14|Module LLMとのシリアル通信|

■CoreS3
| |PIN|使用状況|
|---|---|---|
|ポートA|1/2|カメラ用クロック(PIN2)|
|**ボートB**|**8/9**|**サーボに割り当て可**|
|ポートC|17/18|Module LLMとのシリアル通信|

## 付録A. Function Callingの実装方法
やや上級者向けですが、LLMモデルをHugging Faceで公開されているFunction Calling対応モデルに入れ替えることで、Module LLMでもFunction Callingが可能になります。

例として、本ソフトでは[こちらの動画(Twitter)](https://x.com/motoh_tw/status/1895120657182269737)のように、M5Stack Core側に実装したアラーム機能をFunction Callingで呼び出せるようにしました。以降、Function Callingを使用するために必要な手順を記載します。

### (1) Hugging Faceのモデルをaxmodelに変換する
Function Callingが可能なLLMモデルをHugging Faceからダウンロードして、Module LLMで実行可能なaxmodelに変換します。[こちらのQiita記事](https://qiita.com/motoh_qiita/items/1b0882e507e803982753)の手順に沿って実施してください。  

### (2) モデルをStackFlowに取り込む
モデルをM5Stack Coreから使用できるようにするために、モデルをModule LLM側のStackFlowというフレームワークに取り込みます。[こちらのQiita記事](https://qiita.com/motoh_qiita/items/772464595e414711bbc9)の手順に沿って実施してください。  

### (3) ロールにFunctionの説明を記述する
ロールはModule LLM側のトークナイザのPythonに記述されています。そのロールに記述されているFunctionの説明を本ソフトで実装したアラーム機能に変更します。LLMサービスがすでに起動している場合は、変更を反映するためにModule LLMの再起動が必要かもしれません。

ファイルの場所：  
/opt/m5stack/scripts/SmolLM-360M-Instruct-fncl_tokenizer.py

ロールの記述内容：

```
fncl_prompt = """You are a helpful assistant with access to the following functions. Use them if required -
 [
    {
        "type": "function",
        "function": {
            "name": "set_alarm",
            "description": "Set alarm.",
            "parameters": {
                "type": "object",
                "properties": {
                    "min": {
                        "type": "integer",
                        "description": 'Set the alarm time in minutes.',
                    },
                },
                "required": ["min"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "stop_alarm",
            "description": "Stop alarm.",
            "parameters": {
                "type": "object",
                "properties": {},
            },
        },
    },
  ] For each function call return a json object with function name and arguments within <toolcall></toolcall> XML tags as follows:
<toolcall>
{'name': , 'arguments': }
</toolcall>
"""
```

### YAMLの設定
YAMLを次のように設定します。

SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml
```
llm:
  type: 2      # 0:ChatGPT  1:ModuleLLM  2:ModuleLLM(Function Calling)
```

必要な手順は以上です。あとは、PlatformIOでビルドして、M5Stack Coreに書き込み実行してください。

## 付録B. その他、Module LLMのカスタマイズ方法
こちらのairpocketさんの記事でまとめられています。

[M5Stack LLM ModuleをLinuxボードとして利用する際のFAQ/Tips](https://elchika.com/article/0e41a4a7-eecc-471e-a259-4fc8d710c26a/)
