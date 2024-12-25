# Function Calling

- [Function Calling](#function-calling)
  - [About Function Calling](#about-function-calling)
  - [Functions that can be called with Function Calling](#functions-that-can-be-called-with-function-calling)
  - [Configuration File（YAML）](#configuration-fileyaml)
    - [Gmail account and app password for sending and receiving emails](#gmail-account-and-app-password-for-sending-and-receiving-emails)
    - [Weather City ID](#weather-city-id)
    - [NewsAPI API key](#newsapi-api-key)
  - [Configuration files (non-YAML)](#configuration-files-non-yaml)
    - [Bus (Train) Timetable](#bus-train-timetable)
    - [Alarm Sound MP3](#alarm-sound-mp3)
  - [Scheduler Function](#scheduler-function)

## About Function Calling
Function Calling is one of the features provided by ChatGPT. If you write the definition of a function group in the prompt passed to ChatGPT, it will respond with the name and arguments of the function to be used in response to the request (the function is implemented and executed on the M5Stack side). For example, you can register functions such as checking today's weather using the weather forecast Web API, or controlling the infrared module to turn off the TV. In short, you can create something like a smart speaker with GhatGPT + M5Stack.

I posted an introductory article on ProtoPedia. There is also a video.  
https://protopedia.net/prototype/4587


## Functions that can be called with Function Calling
The table below shows a list of Function Calling features already implemented in this software.

Prompts and function implementations are collected in FunctionCall.cpp. Stackchan will use the functions according to your requests.

> The ChatGPT model is GPT-4o to improve the accuracy of Function Calling. To change to GPT-4o mini, edit the prompt in FunctionCall.cpp.

```c
String json_ChatString = 
//"{\"model\": \"gpt-4o-mini\","
"{\"model\": \"gpt-4o\","
"\"messages\": [{\"role\": \"user\", \"content\": \"\"}],"
```


| No. | Function | Example | Demo | Supplement |
| --- | --- | --- | --- | --- |
| 1 | Time | 「What time is it?」<br>「What day is it today?」| |
| 2 | Timer | 「Set an alarm for three minutes」<br>「Power off after 1 hour」<br> 「Cancel the timer」| [Movie(Twitter)](https://twitter.com/motoh_tw/status/1675171545533251584) |
| 3 | Note（SD card） | 「Take note of...」<br>「Read the note」<br>「Erase the note」||The note will be saved to the SD card with the file name notepad.txt.|
| 4 | Send Email | 「Email me a note」<br>「Email me...」|[Movie(Twitter)](https://twitter.com/motoh_tw/status/1686403120698736640)|You will need to save your Gmail app password to the SD card (see "Various configuration files").|
| 5 | Receive Email | 「Read the email」|[Movie(Twitter)](https://twitter.com/motoh_tw/status/1688132338293882880)|・Use the same app password as when sending.<br>・It checks the mail server every 5 minutes and, if there is new email, it will let you know "X number of emails received."|
| 6 | Bus (Train) Timetable | 「When is the next bus?」 |[Movie(Twitter)](https://twitter.com/motoh_tw/status/1686404335121686528)|The timetable must be saved to a file on the SD card (see "Various configuration files").|
| 7 | Weather Forecast | 「What's the weather like today?」 || The City ID can be changed in the configuration file on the SD card (see "Various configuration files").|
| 8 | Wake word registration | 「Register the wake word」<br>「Enable wake word」|||
| 9 | Reminders | 「Set a new reminder」|[Movie(Twitter)](https://twitter.com/motoh_tw/status/1784956121016627358)||
| 10 | Latest news (NewsAPI) | 「What's in the news today?」 || The API key must be saved on the SD card (see "Various configuration files").|


## Configuration File（YAML）
The settings required for each of the above functions are done in the YAML file on the SD card. Functions that you do not use can be left blank.

SD card folder：/app/AiStackChanEx  
File name：SC_ExConfig.yaml

### Gmail account and app password for sending and receiving emails
This setting is required if you want to use the email sending/receiving function.
- Gmail account email address
- App password for Gmail account（Follow the instructions [here](https://support.google.com/mail/answer/185833?hl=ja) to get it
- Destination email address


```
mail:
  account: "********@gmail.com"    # Gmail account email address
  app_pwd: "********"              # App password for Gmail account
  to_addr: "********@gmail.com"    # Destination email address
```

### Weather City ID
This setting is required if you want to use the weather forecast function.  
You can find your City ID [here](https://weather.tsukumijima.net/primary_area.xml)

```
weather:
  city_id: "140010"     # Weather City ID
```

### NewsAPI API key
This setting is required when using the News function.  
You can obtain an API key from the [NewsAPI web page](https://newsapi.org/) (there is no charge for Developer plan).

```
news:
  apikey: "********"    # NewsAPI API key
```

## Configuration files (non-YAML)

### Bus (Train) Timetable
If you want to use the timetable function, save the timetable to the SD card as follows.

SD card folder：/app/AiStackChanEx  
File name：bus_timetable.txt（Weekdays）、bus_timetable_sat.txt（Saturday）、bus_timetable_holiday.txt（Sunday）
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

### Alarm Sound MP3
If you save an MP3 file on the SD card as follows, it will be played as an alarm sound for the timer or reminder function.

SD card folder：/app/AiStackChanEx  
File name：alarm.mp3


## Scheduler Function
You can specify a time to execute any callback function. You can also send a request to ChatGPT using Function Calling within the callback function.  
The following actions are implemented as a sample:

- Every morning at 6:30, return from power saving mode
- Every morning at 7:00, tell me today's date, weather, and memo contents (using ChatGPT + Function Call)
- Every night at 23:30, switch to energy saving mode
- Time signal (between 7am and 11pm)
- Check incoming emails every 5 minutes (between 7am and 11pm)

Callback functions are registered using the add_schedule() function and the Schedule class prepared for each type of schedule (specified time, repeating at regular intervals, etc.) as follows:

```c++
/* MySchedule.cpp */

void init_schedule(void)
{
    //6:30 Return from power saving mode
    add_schedule(new ScheduleEveryDay(6, 30, sched_fn_wake));
    //7:00 Speaks today's date, weather, and notes
    add_schedule(new ScheduleEveryDay(7, 00, sched_fn_morning_info));
    //23:30 Switch to energy saving mode
    add_schedule(new ScheduleEveryDay(23, 30, sched_fn_sleep));
    //Time signal (between 7am and 11pm)
    add_schedule(new ScheduleEveryHour(sched_fn_announce_time, 7, 23));
    //Check incoming emails every 5 minutes (between 7am and 11pm)
    add_schedule(new ScheduleIntervalMinute(5, sched_fn_recv_mail, 7, 23));
}

*All default settings are commented out. Please use as necessary.
```

Each Schedule class inherits from the ScheduleBase class, and achieves different behavior (specifying a time, repeating at regular intervals, etc.) depending on how the virtual function run() is overridden. The Schedule classes currently available are as follows:

| Class | Action | Notes |
| --- | --- | --- |
| ScheduleEveryDay | Execute a callback function every day at the specified time | |
| ScheduleEveryHour | Execute a callback function every hour (mainly for time signals)| |
| ScheduleIntervalMinute | Executes the callback function repeatedly at the specified interval [minutes] | |
| ScheduleReminder | Executes a callback function once at a specified time | Used for the Function Call reminder function |

