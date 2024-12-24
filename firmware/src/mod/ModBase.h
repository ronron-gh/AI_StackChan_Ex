#ifndef _MOD_BASE_H
#define _MOD_BASE_H

#include <arduino.h>

struct box_t
{
  int x;
  int y;
  int w;
  int h;
  int touch_id = -1;

  void setupBox(int x, int y, int w, int h) {
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
  }
  bool contain(int x, int y)
  {
    return this->x <= x && x < (this->x + this->w)
        && this->y <= y && y < (this->y + this->h);
  }
};


class ModBase{
protected:
  String modName;

public:
  ModBase() {};
  virtual void init(void) {};
  virtual void pause(void) {};
  virtual void update(int page_no) {};
  virtual void btnA_pressed(void) {};
  virtual void btnA_longPressed(void) {};
  virtual void btnB_pressed(void) {};
  virtual void btnB_longPressed(void) {};
  virtual void btnC_pressed(void) {};
  virtual void btnC_longPressed(void) {};
  virtual void display_touched(int16_t x, int16_t y) {};
  virtual void idle(void) {};
};



#endif  //_MOD_BASE_H