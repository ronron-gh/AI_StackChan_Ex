
## ウェイクワード SimpleVox
SimpleVoxはMechaUmaさんが公開されているライブラリです。robo8080さんのAIｽﾀｯｸﾁｬﾝで導入されており、本ソフトでも引き継いでいます。本ソフトではplatformio.iniのビルドフラグに-DENABLE_WAKEWORDを追加することで有効になります。
ウェイクワードの使い方は[AIｽﾀｯｸﾁｬﾝ2のREADME](https://github.com/robo8080/AI_StackChan2_README/)を参照ください。  

### 複数のウェイクワードを登録可
本ソフトでは、ウェイクワードを10個まで登録することが可能になっています。これにより、複数の人の呼びかけに反応できるようになりました。

登録したウェイクワードのデータはSPIFFSにwakeword#.binという名前で保存されます。現行、10個に到達すると新たなウェイクワードは登録できませんので、Function Callingで「ウェイクワードの#番を削除して」とお願いするか、FFFTP等のFTPクライアントソフトで不要なウェイクワードを削除してください（FTPはユーザ名：stackchan、パスワード：stackchan）。

FFFTPでSPIFFSのファイル一覧を取得した様子

![](../images/ftp.jpg)

> ウェイクワードの誤認識が気になる場合は、WakeWord.h内のDIST_THRESHOLD の値を小さくすると誤認識を減らせますが、小さくし過ぎると逆に正しい呼びかけにも反応しづらくなります。

```
#define DIST_THRESHOLD  (250)
```

> CoreS3はA、Bボタンがないため、ウェイクワードの登録、有効化はFunction Callingで「ウェイクワードを登録」「ウェイクワードを有効化」というように指示してください。
