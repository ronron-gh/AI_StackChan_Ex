// Copyright (c) Shinya Ishikawa. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include "Eye.h"

namespace m5avatar {

Eye::Eye(uint16_t x, uint16_t y, uint16_t r, bool isLeft) : Eye(r, isLeft) {}

Eye::Eye(uint16_t r, bool isLeft) : r{r}, isLeft{isLeft} {}

void Eye::draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) {
  Expression exp = ctx->getExpression();
  uint32_t x = rect.getCenterX();
  uint32_t y = rect.getCenterY();
  Gaze g = ctx->getGaze();
  float openRatio = ctx->getEyeOpenRatio();
  uint32_t offsetX = g.getHorizontal() * 3;
  uint32_t offsetY = g.getVertical() * 3;
  uint16_t primaryColor = ctx->getColorDepth() == 1 ? 1 : ctx->getColorPalette()->get(COLOR_PRIMARY);
  uint16_t backgroundColor = ctx->getColorDepth() == 1 ? 0 : ctx->getColorPalette()->get(COLOR_BACKGROUND);

  if (openRatio > 0) {
    // ハート目: 円の代わりにハートマークを描く
    if (exp == Expression::HeartEyes) {
      int cx = x + offsetX;
      int cy = y + offsetY;
      int hr = (int)(r * 0.7f);
      int dx = (int)(r * 0.55f);
      int dy = (int)(-r * 0.25f);
      spi->fillCircle(cx - dx, cy + dy, hr, primaryColor);
      spi->fillCircle(cx + dx, cy + dy, hr, primaryColor);
      // ハート下部の三角
      spi->fillTriangle(
        cx - dx - hr + 2, cy + dy + hr / 4,
        cx + dx + hr - 2, cy + dy + hr / 4,
        cx,               cy + r + 2,
        primaryColor);
      return;
    }
    // 驚き: 大きめ目に瞳孔のような同心円
    if (exp == Expression::Surprised) {
      int rr = r + 3;
      spi->fillCircle(x + offsetX, y + offsetY, rr,     primaryColor);
      spi->fillCircle(x + offsetX, y + offsetY, rr / 2, backgroundColor);
      spi->fillCircle(x + offsetX, y + offsetY, rr / 4, primaryColor);
      return;
    }
    spi->fillCircle(x + offsetX, y + offsetY, r, primaryColor);
    // TODO(meganetaaan): Refactor
    if (exp == Expression::Angry || exp == Expression::Sad) {
      int x0, y0, x1, y1, x2, y2;
      x0 = x + offsetX - r;
      y0 = y + offsetY - r;
      x1 = x0 + r * 2;
      y1 = y0;
      x2 = !isLeft != !(exp == Expression::Sad) ? x0 : x1;
      y2 = y0 + r;
      spi->fillTriangle(x0, y0, x1, y1, x2, y2, backgroundColor);
    }
    if (exp == Expression::Happy || exp == Expression::Sleepy || exp == Expression::Embarrassed) {
      int x0, y0, w, h;
      x0 = x + offsetX - r;
      y0 = y + offsetY - r;
      w = r * 2 + 4;
      h = r + 2;
      if (exp == Expression::Happy || exp == Expression::Embarrassed) {
        y0 += r;
        spi->fillCircle(x + offsetX, y + offsetY, r / 1.5, backgroundColor);
      }
      spi->fillRect(x0, y0, w, h, backgroundColor);
    }
  } else {
    int x1 = x - r + offsetX;
    int y1 = y - 2 + offsetY;
    int w = r * 2;
    int h = 4;
    spi->fillRect(x1, y1, w, h, primaryColor);
  }
}
}  // namespace m5avatar
