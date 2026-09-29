# SanoTTS-jpを使用する場合の手順

[SanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp)を使用すると、M5Stack CoreS3上で日本語音声をローカル合成できます。外部の音声合成APIとAPIキーは不要です。

現在はCoreS3のみ対応しています。ライブラリ、モデル、辞書は本リポジトリに含まれていないため、利用者が別途取得して配置する必要があります。モデルと辞書を含むファームウェアを配布する場合も含め、取得元のライセンスとNOTICEを確認してください。

## 必要なファイルの準備

1. AI_StackChan_Exと検証用リポジトリをクローンします。

> Note:  
> 検証用リポジトリは、SanoTTS-jpをPlatformIOのArduinoフレームワーク環境できることを検証したリポジトリです。

   ```sh
   git clone https://github.com/ronron-gh/AI_StackChan_Ex.git
   git clone https://github.com/ronron-gh/SanoTTS_Arduino_CoreS3.git
   ```

2. 検証用リポジトリから、次のディレクトリとファイルをAI_StackChan_Exの`firmware`へコピーします。

   | コピー元 | コピー先 |
   |---|---|
   | `lib/saanotts_core/` | `firmware/lib/saanotts_core/` |
   | `model/student_i8.bin` | `firmware/model/student_i8.bin` |
   | `scripts/`内のファイル一式 | `firmware/scripts/` |

   `lib/saanotts_core/`はディレクトリごとコピーしてください。既存の`firmware/scripts/`は削除せず、検証用リポジトリのファイルを追加します。

3. Git BashでAI_StackChan_Exの`firmware`へ移動し、辞書取得スクリプトを実行します。

   ```sh
   cd AI_StackChan_Ex/firmware
   scripts/get_dict.sh 44000
   ```

   `firmware/model/k1-dict-44000-2mb.bin`が作成されたことを確認してください。辞書はビルド時に自動ではダウンロードされません。

配置後は次の構成になります。

```text
firmware/
├─ lib/
│  └─ saanotts_core/
├─ model/
│  ├─ student_i8.bin
│  └─ k1-dict-44000-2mb.bin
└─ scripts/
   ├─ blob_to_header.py
   ├─ dictionary_to_header.py
   ├─ get_dict.sh
   ├─ platformio_dictionary.py
   └─ platformio_model.py
```

モデルと辞書の取得元、ハッシュ値、ライセンスの詳細は、検証用リポジトリの[モデルと辞書](https://github.com/ronron-gh/SanoTTS_Arduino_CoreS3/blob/main/model/README.md)および[NOTICE](https://github.com/ronron-gh/SanoTTS_Arduino_CoreS3/blob/main/NOTICE.md)を確認してください。

## YAMLの設定

SDカードの`/app/AiStackChanEx/SC_ExConfig.yaml`で、TTSの種類をSanoTTS-jpに変更します。`model`と`voice`は使用しません。

```yaml
tts:
  type: 5  # SanoTTS-jp
  model: ""
  voice: ""
```

## ビルドと書き込み

PlatformIOで`firmware`を開き、環境`m5stack-cores3-sanotts`を選択してビルド、書き込みを行います。

コマンドラインから実行する場合は次のとおりです。

```sh
cd AI_StackChan_Ex/firmware
pio run -e m5stack-cores3-sanotts
pio run -e m5stack-cores3-sanotts -t upload
pio device monitor -e m5stack-cores3-sanotts -b 115200
```

> Note:  
> - ビルド時にモデルと辞書が検査され、ファームウェアへ埋め込まれます。モデルや辞書をSDカードへコピーする必要はありません。  
> - Realtime APIとSanoTTS-jpを組み合わせることも可能です。その場合は環境`m5stack-cores3-realtime-sanotts`を選択してください。Realtime APIについては[こちら](realtime_api.md)を参照ください。

## 再生方式について

標準の`m5stack-cores3-sanotts`環境では、句読点、改行、入力上限で分けた区間ごとに音声を生成し、PCMをPSRAMへ蓄積してから再生します。再生中に次の区間を生成するため、区間間の待ち時間を抑えられます。

再生方式は、`firmware/platformio.ini`の`[sanotts-cores3]`で設定します。

```ini
[sanotts-cores3]
build_flags =
    -DUSE_SANOTTS
    -DSANOTTS_BUFFERED_PLAYBACK
```

`SANOTTS_BUFFERED_PLAYBACK`を外すと、循環バッファを使うストリーミング再生に切り替わります。CoreS3では推論用メモリがPSRAMに置かれるため、処理が再生に間に合わず音切れする場合があります。

設定を変更した場合は、ファームウェアを再ビルドして書き込んでください。

## 補足事項

- SanoTTS-jpは入力文を句読点や改行などで分割して発話します。長い文章も内部の上限に合わせて分割されます。
- 固有名詞、英単語、記号などは意図した読みにならない場合があります。
- 蓄積再生ではPSRAMに音声データを保持します。空き容量が不足すると、次の区間を並行生成せず、現在の区間の再生後に生成を続けます。
