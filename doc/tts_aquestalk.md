
# AquesTalkの使い方

AquesTalkを使用する場合は次の手順が必要です。

1. AquesTalk ESP32のライブラリ＆辞書ファイルをダウンロード  
   以下のURLから「AquesTalk ESP32(Small辞書版) 	」をダウンロードします。  
   https://www.a-quest.com/download.html

   ※Small辞書版でなくてもよいかもしれませんが、動作未確認です。

2. AquesTalkの使用ライセンスを購入  
   ※ライセンスがなくても評価版として使用できますが、「ナ行、マ行」の音韻がすべて「ヌ」になる制限があります。
   
3. libフォルダにダウンロードしたライブラリを展開  
   ![](../images/aquestalk_lib.png)

   ※AquesTalkTTS_M5.cpp と AquesTalkTTS.h は examples\M5Stack\M5_AquesTalk_KM から持ってくる。
   
4. AquesTalkTTS_M5.cpp の変数LICENSE_KEYにライセンスキーを設定  
   ![](../images/aquestalk_license_key.png)

5. SDカードにダウンロードした辞書データを保存  
   ディレクトリは"/aq_dic/aqdic_m.bin"とする。
   
6. platformio.iniのbuild_flagsに以下の記述を追加  
   
   ```
   build_flags=
    -DUSE_AQUESTALK
    -Llib/aquestalk/src/esp32s2/    ※M5Core2の場合。CoreS3ならesp32s3。
    -laquestalk_s
   ``` 
