#include <Arduino.h>
#include <deque>
#include "ModManager.h"
#include "ModBase.h"
#include <Avatar.h>

using namespace m5avatar;


/// 外部参照 ///
extern Avatar avatar;

///////////////
static bool g_avatar_status = true;
std::deque<ModBase*> modList;


static void avatar_fadeout(bool reverse)
{
  int16_t offset = 0;
  while(offset <= 320){
    offset += 10;
    if(reverse){
      avatar.setFaceOffsetX(offset);
    }
    else{
      avatar.setFaceOffsetX(-offset);
    }
    delay(10);
  }
}

void add_mod(ModBase* mod)
{
  modList.push_back(mod);
}

ModBase* change_mod(bool reverse)
{
  ModBase* mod;
  avatar_fadeout(reverse);
  mod = modList[0];
  mod->pause();
  if(reverse){
    mod = modList.back();
    modList.pop_back();
    modList.push_front(mod);
  }
  else{
    modList.pop_front();
    modList.push_back(mod);
  }
  mod = modList[0];
  avatar.setFaceOffsetX(0);
  mod->init();
  return mod;
}

ModBase* get_current_mod(void)
{
  return modList[0];
}

