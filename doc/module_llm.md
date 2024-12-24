
# Module LLMを使用する際の設定方法
![](../images/module_llm.jpg)
写真のようにｽﾀｯｸﾁｬﾝにModule LLMをスタックして使用する際の設定方法について記載します。

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


## その他、Module LLMのカスタマイズ方法
やや上級者向けですが、こちらのairpocketさんの記事でまとめられています。

[M5Stack LLM ModuleをLinuxボードとして利用する際のFAQ/Tips](https://elchika.com/article/0e41a4a7-eecc-471e-a259-4fc8d710c26a/)
