# Function Calling

- [Function Calling](#function-calling)
  - [Function Callingについて](#function-callingについて)
  - [Function Callingで呼び出せる機能](#function-callingで呼び出せる機能)
  - [設定ファイル（YAML）](#設定ファイルyaml)
    - [メール送受信用のGmailアカウント、アプリパスワード](#メール送受信用のgmailアカウントアプリパスワード)
    - [天気予報のCity ID](#天気予報のcity-id)
    - [NewsAPIのAPIキー](#newsapiのapiキー)
  - [設定ファイル（YAML以外）](#設定ファイルyaml以外)
    - [バス（電車）の時刻表](#バス電車の時刻表)
    - [アラーム音のMP3](#アラーム音のmp3)
  - [スケジューラ機能](#スケジューラ機能)

## Function Callingについて
Function CallingはChatGPTが提供する機能の一つです。ChatGPTに渡すプロンプトに関数群の定義を記述しておくと、リクエストに応じて使うべき関数の名前と引数を返答してくれます（関数の実装、実行はM5Stack側です）。例えば、今日の天気を天気予報のWeb APIを使って調べたり、赤外線モジュールを制御してテレビを消したりといった関数を登録することができます。要するに、GhatGPT＋M5Stackでスマートスピーカーのようなものを自作できます。

ProtoPediaに紹介記事を投稿しました。動画もあります。  
https://protopedia.net/prototype/4587


## Function Callingで呼び出せる機能
Function Callingですでに実装している機能の一覧を下表に示します。

プロンプトや関数の実装は FunctionCall.cpp にまとめています。リクエストに応じてｽﾀｯｸﾁｬﾝが関数を使いこなしてくれます。FunctionCall.cppを改造することで、新たな機能を追加するなどのカスタマイズができます。

> ChatGPTのモデルはFunction Callingの精度を上げるためにGPT-4oにしています。GPT-4o miniに変更するにはFunctionCall.cpp内のプロンプトを編集してください。

```c
String json_ChatString = 
//"{\"model\": \"gpt-4o-mini\","
"{\"model\": \"gpt-4o\","
"\"messages\": [{\"role\": \"user\", \"content\": \"\"}],"
```


| No. | 機能 | 使用例 | デモ | 補足 |
| --- | --- | --- | --- | --- |
| 1 | 時刻 | 「今何時？」<br>「今日は何日？」<br>「今日は何曜日？」| |
| 2 | タイマー | 「3分のアラームをセットして」<br>「1時間後にパワーオフして」<br> 「タイマーをキャンセルして」| [動画(X)](https://twitter.com/motoh_tw/status/1675171545533251584) |
| 3 | メモ（SDカード） | 「～をメモして」<br>「これから言うことをメモして」<br>「メモを読んで」<br>「メモを消去して」||メモは notepad.txt というファイル名でSDカードに保存されます。|
| 4 | メール送信 | 「メモをメールして」<br>「～をメールして」|[動画(X)](https://twitter.com/motoh_tw/status/1686403120698736640)|GmailのアプリパスワードをSDカードに保存しておく必要があります（「各種設定ファイル」参照）。|
| 5 | メール受信 | 「メールを読んで」|[動画(X)](https://twitter.com/motoh_tw/status/1688132338293882880)|・送信と同じアプリパスワードを使います。<br>・メールサーバを5分毎に確認し、新しいメールがあれば「〇件のメールを受信しています」と教えてくれます。|
| 6 | バス（電車）時刻表 | 「次のバスの時間は？」<br>「その次は？」 |[動画(X)](https://twitter.com/motoh_tw/status/1686404335121686528)|時刻表をSDカードのファイルに保存しておく必要があります（「各種設定ファイル」参照）。|
| 7 | 天気予報 | 「今日の天気は？」<br>「明日の天気は？」 || ・robo8080さんが[Gistに公開してくださったコード](https://gist.github.com/robo8080/60a7bb619f6bae66aa97496371884386)を参考にさせていただきました。<br>・City IDはSDカードの設定ファイルで変更することができます（「各種設定ファイル」参照）。|
| 8 | ウェイクワード登録 | 「ウェイクワードを登録」<br>「ウェイクワードを有効化」|||
| 9 | リマインダ | 「新しいリマインダを設定して」|[動画(X)](https://twitter.com/motoh_tw/status/1784956121016627358)||
| 10 | 最新ニュース (NewsAPI連携) | 「今日のニュースは？」 || ・[K.Yamaさん(X)](https://twitter.com/USDJPY2010)から提供いただいたコードを参考にさせていただきました。<br>・APIキーをSDカードに保存しておく必要があります（「各種設定ファイル」参照）。|


## 設定ファイル（YAML）
上記の機能毎に必要な設定はSDカードのYAMLファイルで行います。使用しない機能についてはブランクで問題ありません。

SDカードフォルダ：/app/AiStackChanEx  
ファイル名：SC_ExConfig.yaml

### メール送受信用のGmailアカウント、アプリパスワード
メール送受信機能を使う場合に設定が必要です。
- Gmailアカウントのメールアドレス
- Gmailアカウントのアプリパスワード（[こちら](https://support.google.com/mail/answer/185833?hl=ja)の説明に沿って取得してください）
- 送信先のメールアドレス


```
mail:
  account: "********@gmail.com"    # Gmailアカウントのメールアドレス
  app_pwd: "********"              # Gmailアカウントのアプリパスワード
  to_addr: "********@gmail.com"    # 送信先のメールアドレス
```

### 天気予報のCity ID
天気予報機能を使う場合に設定が必要です。  
City IDは[こちら](https://weather.tsukumijima.net/primary_area.xml)で調べることができます。

```
weather:
  city_id: "140010"     # 天気予報APIのCity ID
```

### NewsAPIのAPIキー
News機能を使う場合に設定が必要です。  
APIキーは[NewsAPIのWEBページ](https://newsapi.org/)から取得できます(Developerプランであれば料金はかかりません)。

```
news:
  apikey: "********"    # NewsAPIのAPIキー
```

## 設定ファイル（YAML以外）

### バス（電車）の時刻表
時刻表機能を使う場合は、SDカードに次のように時刻表を保存してください。
※現行は祝日の判別はできません。

フォルダ：/app/AiStackChanEx  
ファイル名：bus_timetable.txt（平日）、bus_timetable_sat.txt（土曜）、bus_timetable_holiday.txt（日曜）
```
06:01
06:33
06:53
・
・
・
21:33
22:03
22:33
```

### アラーム音のMP3
SDカードに次のようにMP3ファイルを保存しておくと、タイマー機能やリマインダ機能のアラーム音として再生されます。

フォルダ：/app/AiStackChanEx  
ファイル名：alarm.mp3

> SDカードの相性により音が途切れることがあるため、起動時にMP3ファイルをSPIFFSにコピーして使用するように改善しました。すでにSPIFFSにalarm.mp3が存在する場合は起動時のコピーは行われません。




## スケジューラ機能
時間を指定して任意のコールバック関数を実行することができます。コールバック関数内で、Function Callingを使う要求をChatGPTに送ることもできます。  
サンプルとして次の動きを実装してあります。

- 毎朝6:30 省エネモードから復帰
- 毎朝7:00 今日の日付、天気、メモの内容を話す (ChatGPT＋Function Callを使用)
- 毎晩23:30 省エネモードに遷移
- 時報（7時から23時の間）
- 5分置きに受信メールを確認（7時から23時の間）

コールバック関数は、次のようにadd_schedule()という関数と、スケジュールの種類毎（時間指定、一定間隔で繰り返し等）に用意したScheduleクラスを使って登録します。

```c++
/* MySchedule.cpp */

void init_schedule(void)
{
    add_schedule(new ScheduleEveryDay(6, 30, sched_fn_wake));               //6:30 省エネモードから復帰
    add_schedule(new ScheduleEveryDay(7, 00, sched_fn_morning_info));       //7:00 今日の日付、天気、メモの内容を話す
    add_schedule(new ScheduleEveryDay(23, 30, sched_fn_sleep));             //23:30 省エネモードに遷移

    add_schedule(new ScheduleEveryHour(sched_fn_announce_time, 7, 23));     //時報（7時から23時の間）

    add_schedule(new ScheduleIntervalMinute(5, sched_fn_recv_mail, 7, 23)); //5分置きに受信メールを確認（7時から23時の間）
}

※初期状態はすべてコメントアウトされています。必要に応じてご利用ください。
```

各ScheduleクラスはScheduleBaseクラスを継承しており、仮想関数run()をどのようにオーバライドするかによって異なる動き（時間指定、一定間隔で繰り返し等）を実現しています。現在用意しているScheduleクラスは次の通りです。

| クラス | 動作 | 備考 |
| --- | --- | --- |
| ScheduleEveryDay | 毎日、指定した時間にコールバック関数を実行する | |
| ScheduleEveryHour | 毎時、コールバック関数を実行する（主に時報用）| |
| ScheduleIntervalMinute | 指定した間隔[分]でコールバック関数を繰り返し実行する | |
| ScheduleReminder | 一度だけ指定した時間にコールバック関数を実行する | Function Callのリマインダー機能で使用している |

