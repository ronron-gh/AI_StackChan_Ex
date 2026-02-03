# Function Calling

- [About Function Calling](#about-function-calling)
- [Functions that can be called with Function Calling](#functions-that-can-be-called-with-function-calling)

## About Function Calling
Function Calling is one of the features provided by LLMs such as ChatGPT. If you write the definition of a set of functions in the prompt, the LLM will respond with the name and arguments of the function to be used in response to the request (the function is executed on the M5Stack side).


## Functions that can be called with Function Calling
The table below shows a list of Function Calling features already implemented in this software. Prompts and function implementations are collected in FunctionCall.cpp. Stackchan will use the functions according to your requests.


| No. | Function | Example | Demo | Supplement |
| --- | --- | --- | --- | --- |
| 1 | Time | 「What time is it?」<br>「What day is it today?」| |
| 2 | Timer | 「Set an alarm for three minutes」<br>「Power off after 1 hour」<br> 「Cancel the timer」| [Movie(Twitter)](https://twitter.com/motoh_tw/status/1675171545533251584) |
| 3 | Wake word registration | 「Register the wake word」<br>「Enable wake word」|||
| 4 | Reminders | 「Set a new reminder」|[Movie(Twitter)](https://twitter.com/motoh_tw/status/1784956121016627358)||

