#include "ChatHistory.h"

ChatHistory::ChatHistory(int _max_history) {
  max_history = _max_history;
}

ChatHistory::~ChatHistory() {
  
}

void ChatHistory::pop_front(){
  chatHistory_role.pop_front();
  chatHistory_funcName.pop_front();
  chatHistory_content.pop_front();
}

void ChatHistory::push_back(String role, String funcName, String content){
  chatHistory_role.push_back(role);
  chatHistory_funcName.push_back(funcName);
  chatHistory_content.push_back(content);

  // チャット履歴が最大数を超えた場合、古い質問と回答を削除
  if (get_size() > max_history)
  {
    pop_front();
  }
}

int ChatHistory::get_size(){
  return chatHistory_role.size();
}

String ChatHistory::get_role(int i){
  return chatHistory_role[i];
}

String ChatHistory::get_funcName(int i){
  return chatHistory_funcName[i];
}

void ChatHistory::set_funcName(int i, String name){
  chatHistory_funcName[i] = name;
}

String ChatHistory::get_content(int i){
  return chatHistory_content[i];
}

void ChatHistory::clear(){
  chatHistory_role.clear();
  chatHistory_funcName.clear();
  chatHistory_content.clear();
}

void ChatHistory::clean_function_role(){
  String role;
  String funcName;
  String content;
  int size = get_size();

  for(int i=0; i<size; i++){
    role = get_role(0);
    funcName = get_funcName(0);
    content = get_content(0);
    pop_front();
    if(role != "function"){
      push_back(role, funcName, content);
    }

  }

#if 0
  //結果確認
  size = get_size();
  for(int i=0; i<size; i++){
    role = get_role(i);
    funcName = get_funcName(i);
    content = get_content(i);
    Serial.printf("role %02d: %s\n", i, role.c_str() );
    Serial.printf("funcName %02d: %s\n", i, funcName.c_str() );
    Serial.printf("content %02d: %s\n", i, content.c_str() );
  }
#endif
}