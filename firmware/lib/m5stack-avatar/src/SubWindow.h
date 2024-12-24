// Copyright (c) Shinya Ishikawa. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#ifndef SUBWINDOW_H_
#define SUBWINDOW_H_

#define LGFX_USE_V1
#include <M5GFX.h>
#include "DrawContext.h"
#include "Drawable.h"

//#define USE_DOUBLE_BUFFER

namespace m5avatar {

typedef enum e_sub_draw_type
{
  SUB_DRAW_TYPE_CAM565,
  SUB_DRAW_TYPE_JPG,
  SUB_DRAW_TYPE_TXT
} SUB_DRAW_TYPE;

class SubWindow final : public Drawable {
 private:
  uint16_t r;
  bool isLeft;

  //カメラ画像表示用
  int32_t RESIZE_RATIO, LCD_WIDTH, LCD_HEIGHT;
  uint32_t subWdWidth, subWdHeight, subWdSize;
  uint16_t* subWdBuf[2];
  uint32_t drawingBufIdx;

  //JPEG表示用
  uint32_t JPG_BUF_SIZE;
  uint8_t* jpgBuf;
  int32_t jpgSize;

  //テキスト表示用
  String subWdTxtBuf;
  bool isDrawEnable;
  uint16_t drawType;    //画像 or テキスト
  M5Canvas *spriteTxt;  //テキスト表示用のスプライト

  size_t copySDFileToRAM(const char *path, uint8_t *out, int outBufSize);

 public:
  // constructor
  SubWindow();
  ~SubWindow() = default;
  //SubWindow(const SubWindow &other) = default;
  //SubWindow &operator=(const SubWindow &other) = default;
  void draw(M5Canvas *spi, BoundingRect rect,
            DrawContext *drawContext) override;

  void set_isDrawEnable(bool _isDrawEnable);
  void updateDrawContentCam565(uint8_t* buf);
  bool updateDrawContentJpg(String& fname);
  void updateDrawContentTxt(String txt);
};

}  // namespace m5avatar

#endif  // SUBWINDOW_H_
