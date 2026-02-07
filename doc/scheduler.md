
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

