// Copyright (c) Shinya Ishikawa. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include <Arduino.h>
#include <SD.h>
#include "SubWindow.h"

namespace m5avatar {

SubWindow::SubWindow() {
  isDrawEnable = false;
  drawingBufIdx = 0;

  //LCD_WIDTH = 320;
  //LCD_HEIGHT = 240;
  //RESIZE_RATIO = 3;

  //GPT-4oに解像度の高い画像を渡したいためキャプチャ時のサイズを2倍にした
  LCD_WIDTH = 640;
  LCD_HEIGHT = 480;
  RESIZE_RATIO = 6;

//  subWdWidth = 80;    //縮小率1/4
  subWdWidth = 107;    //縮小率1/3
//  subWdWidth = 160;    //縮小率1/2

//  subWdHeight = 60;    //縮小率1/4
  subWdHeight = 80;    //縮小率1/3
//  subWdHeight = 120;    //縮小率1/2

  subWdSize = subWdWidth * subWdHeight;

  subWdBuf[0] = (uint16_t*)malloc(subWdSize * 2);
  if(subWdBuf[0]==NULL){
    Serial.println("SubWindow: malloc() failed");    
  }
#if defined(USE_DOUBLE_BUFFER)
  subWdBuf[1] = (uint16_t*)malloc(subWdSize * 2);
  if(subWdBuf[1]==NULL){
    Serial.println("SubWindow: malloc() failed");    
  }
#endif

  //テキスト表示用のスプライト
  spriteTxt = new M5Canvas(&M5.Lcd);
  if(spriteTxt == nullptr){
    M5_LOGE("spriteTxt new error");
  }

  //JPEG表示用のバッファ
  JPG_BUF_SIZE = 131072;   //128KB
  //jpgBuf = new uint8_t[JPG_BUF_SIZE];
  jpgBuf = nullptr;
  // SPIRAMから確保するために、ここではnullptrにしておき初回のjpgBuf使用時に確保する。
  // AvatarをSetup()内でnewするように変更すれば、ここで確保してもSPIRAMから確保できるはず。
}


void SubWindow::draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) {
 
  if(isDrawEnable){
    if(drawType == SUB_DRAW_TYPE_CAM565){
      int x = rect.getLeft();
      int y = rect.getTop();
      spi->startWrite();
      spi->setAddrWindow(x, y, subWdWidth, subWdHeight);
      spi->writePixels((uint16_t*)subWdBuf[drawingBufIdx], subWdSize);
      spi->endWrite();
    }
    else if(drawType == SUB_DRAW_TYPE_JPG){
      int x = rect.getLeft();
      int y = rect.getTop();
      spi->drawJpg(jpgBuf, jpgSize, x, y);

#if 0   //SDカードのファイルを直接指定もできるが、描画の度にSDカードから読み込むことになるため非効率   
      if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
        spi->drawJpgFile(SD, "/app/AiStackChan2FuncCall/photo/photo001.jpg");
      }
      else{
        Serial.println("Failed to load SD card settings. System reset after 5 seconds.");
      }
#endif
    }
    else if(drawType == SUB_DRAW_TYPE_TXT){
      int x = rect.getLeft();
      int y = rect.getTop();
      int w = rect.getWidth();
      int h = rect.getHeight();
      spriteTxt->setColorDepth(ctx->getColorDepth());
      //spriteTxt->createSprite(M5.Display.width(), M5.Display.height());
      spriteTxt->createSprite(w, h);
      spriteTxt->setBitmapColor(ctx->getColorPalette()->get(COLOR_PRIMARY),
        ctx->getColorPalette()->get(COLOR_BACKGROUND));
      if (ctx->getColorDepth() != 1) {
        spriteTxt->fillSprite(ctx->getColorPalette()->get(COLOR_BACKGROUND));
      } else {
        spriteTxt->fillSprite(0);
      }
      //spi->setClipRect(x, y, w, h);
      spriteTxt->fillRect(0, 0, w, h, BLACK);
      spriteTxt->setFont(&fonts::lgfxJapanGothic_20);
      spriteTxt->setTextSize(1);
      spriteTxt->setTextColor(WHITE, BLACK);
      spriteTxt->setTextDatum(0);
      spriteTxt->setCursor(0, 0);
      spriteTxt->print(subWdTxtBuf);
      spriteTxt->pushSprite(spi, x, y);
      spriteTxt->deleteSprite();
    }
  }
}

void SubWindow::set_isDrawEnable(bool _isDrawEnable){
  isDrawEnable = _isDrawEnable;
}

void SubWindow::updateDrawContentCam565(uint8_t* src){

  uint16_t* resizeBuf;

  drawType = SUB_DRAW_TYPE_CAM565;

#if defined(USE_DOUBLE_BUFFER)
  if(drawingBufIdx == 0){
    resizeBuf = subWdBuf[1];
  }
  else{
    resizeBuf = subWdBuf[0];
  }
#else
  resizeBuf = subWdBuf[0];
#endif

  //画像を縮小
  uint16_t* buf16 = (uint16_t*)src;
  int idx=0;
  for(int y=0; y<LCD_HEIGHT; y++){
    if(y%RESIZE_RATIO != 0) continue;
    for(int x=0; x<LCD_WIDTH; x++){
      if(x%RESIZE_RATIO != 0) continue;

      resizeBuf[idx++] = buf16[y*LCD_WIDTH + x];

    }

  }

#if defined(USE_DOUBLE_BUFFER)
  //表示するバッファを切り替え
  if(drawingBufIdx == 0){
    drawingBufIdx = 1;
  }
  else{
    drawingBufIdx = 0;
  }
#endif
}


bool SubWindow::updateDrawContentJpg(String& fname){
  bool result;
  drawType = SUB_DRAW_TYPE_JPG;

  if(jpgBuf == nullptr){
    jpgBuf = new uint8_t[JPG_BUF_SIZE];
  }

  for(int i = 0; i < 2; i++){
    jpgSize = copySDFileToRAM(fname.c_str(), jpgBuf, JPG_BUF_SIZE);
    if(jpgSize == 0){
      Serial.println("JPEG load failed");
      result = false;
    }
    else{
      Serial.printf("JPEG loaded : %s (%d byte)\n", fname.c_str(), jpgSize);
      result = true;
      break;
    }
  }
  return result;
}

void SubWindow::updateDrawContentTxt(String txt){
  drawType = SUB_DRAW_TYPE_TXT;
  subWdTxtBuf = txt;
}



// SDカードのファイルを配列にコピー
size_t SubWindow::copySDFileToRAM(const char *path, uint8_t *out, int outBufSize) {
  size_t error = 0;
  size_t readed_size;
  const int BUF_SIZE = 1024;
  Serial.println("Copy SD File to RAM.");

  if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    Serial.println("Failed to load SD card settings.");
    return error;
  }

  if (!SD.exists(path)) {
    Serial.println("File doesn't exist.");
    SD.end();
    return error;
  }

  File fsrc = SD.open(path, FILE_READ);

  if(!fsrc) {
    Serial.println("Failed to open file.");
    SD.end();
    return error;
  }

  uint8_t *buf = new uint8_t[BUF_SIZE];
  if (!buf) {
	  Serial.println("Failed to allocate buffer.");
	  return error;
  }

  size_t len, size, ret;
  size = len = fsrc.size();
  if(outBufSize < size){
	  Serial.println("Output buffer size exceeded.");
	  return error;
  }

  while (len) {
	  size_t s = len;
	  if (s > BUF_SIZE){
		  s = BUF_SIZE;
    }  

	  if(!fsrc.read(buf, s)){
      Serial.println("Failed to read file.");
      SD.end();
      delete[] buf;
      return error;
    }
    
    memcpy(out, buf, s);
    out += s;
	  len -= s;
	  Serial.printf("%d / %d\n", size - len, size);

    delay(20); 
    //このdelayがないと”ff_sd_status(): Check status failed”という
    //エラーが発生し、以降ファイルオープンできなくなる事象が頻発する。
    //発生した場合、SDカードをフォーマットしなおさないと復旧できない。
  }

  delete[] buf;
  fsrc.close();

  Serial.println("*** Done. ***\r\n");
  return size;
}


}  // namespace m5avatar
