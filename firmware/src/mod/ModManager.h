#ifndef _MOD_MANAGER_H
#define _MOD_MANAGER_H

#include "ModBase.h"


#define MAX_UI_NUM  (10)

extern void add_mod(ModBase* mod);
extern ModBase* change_mod(bool reverse = false);
extern ModBase* get_current_mod(void);


#endif  //_MOD_MANAGER_H
