#ifndef _CHAT_HISTORY_H
#define _CHAT_HISTORY_H

#include <Arduino.h>
#include <deque>

class ChatHistory{
private:
  int max_history;
  std::deque<String> chatHistory_role;
  std::deque<String> chatHistory_funcName;
  std::deque<String> chatHistory_content;

public:
  ChatHistory(int _max_history);
  ~ChatHistory();
  void pop_front();
  void push_back(String role, String funcName, String content);
  int get_size();
  String get_role(int i);
  String get_funcName(int i);
  void set_funcName(int i, String name);
  String get_content(int i);
  void clear();
  void clean_function_role();
};

#endif //_CHAT_HISTORY_H