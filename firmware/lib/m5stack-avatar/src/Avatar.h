// Copyright (c) Shinya Ishikawa. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#ifndef AVATAR_H_
#define AVATAR_H_
#include "ColorPalette.h"
#include "Face.h"
#include <M5GFX.h>

namespace m5avatar {
class Avatar {
 private:
  Face *face;
  bool _isDrawing;
  Expression expression;
  float breath;
  float eyeOpenRatio;
  float mouthOpenRatio;
  float gazeV;
  float gazeH;
  float rotation;
  float scale;
  ColorPalette palette;
  const char *speechText;
  int colorDepth;
  BatteryIconStatus batteryIconStatus;
  int32_t batteryLevel;
  const lgfx::IFont *speechFont;

 public:
  Avatar();
  explicit Avatar(Face *face);
  ~Avatar() = default;
  Avatar(const Avatar &other) = default;
  Avatar &operator=(const Avatar &other) = default;
  Face *getFace() const;
  ColorPalette getColorPalette() const;
  void setColorPalette(ColorPalette cp);
  void setFace(Face *face);
  void init(int colorDepth = 1);
  Expression getExpression();
  void setBreath(float f);
  float getBreath();
  void setGaze(float vertical, float horizontal);
  void getGaze(float *vertical, float *horizontal);
  void setExpression(Expression exp);
  void setEyeOpenRatio(float ratio);
  void setMouthOpenRatio(float ratio);
  void setSpeechText(const char *speechText);
  void setSpeechFont(const lgfx::IFont *speechFont);
  void setRotation(float radian);
  void setPosition(int top, int left);
  void setScale(float scale);
  void draw(void);
  bool isDrawing();
  void start(int colorDepth = 1);
  void stop();
  void addTask(TaskFunction_t f, const char* name, int stackSize = 2048, int priority = 1);
  void suspend();
  void resume();
  void setBatteryIcon(bool iconStatus);
  void setBatteryStatus(bool isCharging, int32_t batteryLevel);

  void updateSubWindowCam565(uint8_t* buf);      //motoh
  bool updateSubWindowJpg(String& fname);      //motoh
  void updateSubWindowTxt(String txt, int top = 0, int left = 0, int width = 320, int height = 240);         //motoh
  void updateSubWindowQrcode(String& txt);      //motoh
  void set_isSubWindowEnable(bool isEnable);  //motoh
  void setFaceOffsetX(int16_t offset_x);      //motoh
};


class DriveContext {
 private:
  // TODO(meganetaaan): cyclic reference
  Avatar *avatar;

 public:
  DriveContext() = delete;
  explicit DriveContext(Avatar *avatar);
  ~DriveContext() = default;
  DriveContext(const DriveContext &other) = delete;
  DriveContext &operator=(const DriveContext &other) = delete;
  Avatar *getAvatar();
};

}  // namespace m5avatar

#endif  // AVATAR_H_
