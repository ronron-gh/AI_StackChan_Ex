// Copyright (c) Shinya Ishikawa. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#ifndef BALLOON_H_
#define BALLOON_H_
#define LGFX_USE_V1
#include <M5Unified.h>
#include <Arduino.h>
#include <string.h>
#include "DrawContext.h"
#include "Drawable.h"

// M5公式版に近い、画面下部固定のスクロール対応 Balloon
// - 位置: 画面下部中央
// - フォント: speechFont (efontJA_16 デフォルト想定)、TEXT_SIZE=1
// - 長文時は左へスクロール
namespace m5avatar {
class Balloon final : public Drawable {
 public:
  Balloon() = default;
  ~Balloon() = default;
  Balloon(const Balloon &other) = default;
  Balloon &operator=(const Balloon &other) = default;

  void draw(M5Canvas *spi, BoundingRect rect,
            DrawContext *drawContext) override {
    const char *text = drawContext->getspeechText();
    if (text == nullptr || strlen(text) == 0) {
      last_text_ = "";
      return;
    }

    // テキスト更新検知 → スクロール開始時刻リセット
    String current(text);
    if (current != last_text_) {
      last_text_ = current;
      scroll_start_ms_ = millis();
    }

    const lgfx::IFont *font = drawContext->getSpeechFont();
    ColorPalette *cp = drawContext->getColorPalette();
    uint16_t primaryColor = cp->get(COLOR_BALLOON_FOREGROUND);
    uint16_t backgroundColor = cp->get(COLOR_BALLOON_BACKGROUND);

    // 画面下部固定、M5公式版に揃えた角丸長方形（rounded rectangle）
    const int bx = 10;         // 左上 x
    const int by = 178;        // 左上 y
    const int bw = 300;        // 幅
    const int bh = 55;         // 高さ
    const int radius = 12;     // 角丸半径
    const int cx = bx + bw / 2;
    const int cy = by + bh / 2;

    // 外枠 → 内側塗りつぶし
    spi->fillRoundRect(bx - 2, by - 2, bw + 4, bh + 4, radius + 2, primaryColor);
    spi->fillRoundRect(bx, by, bw, bh, radius, backgroundColor);

    // 矢印（左側上向き、顔の口元方向）— 吹き出し外枠と継ぎ目なく接続
    const int ax = bx + 50;
    const int ay = by - 1;     // 吹き出し外枠 (by-2) より1px下を底辺に → 隙間なし＆めり込みなし
    spi->fillTriangle(ax - 12, ay,     ax + 12, ay,     ax, ay - 15, primaryColor);
    spi->fillTriangle(ax - 9,  ay + 2, ax + 9,  ay + 2, ax, ay - 11, backgroundColor);

    // フォント・色設定
    M5.Lcd.setFont(font);
    M5.Lcd.setTextSize(1);
    int textWidth = M5.Lcd.textWidth(text);
    int textHeight = M5.Lcd.fontHeight();

    spi->setFont(font);
    spi->setTextSize(1);
    spi->setTextColor(primaryColor, backgroundColor);

    // 角丸長方形内のテキスト描画領域
    const int textAreaWidth = bw - 30;                // 左右マージン15px
    const int textAreaLeft = bx + 15;
    const int textY = cy;

    // クリップで楕円内に描画を制限
    int32_t prevClipX, prevClipY, prevClipW, prevClipH;
    spi->getClipRect(&prevClipX, &prevClipY, &prevClipW, &prevClipH);
    spi->setClipRect(textAreaLeft, cy - textHeight / 2 - 2,
                     textAreaWidth, textHeight + 4);

    if (textWidth <= textAreaWidth) {
      // 収まる → 中央寄せで静止表示
      spi->setTextDatum(textdatum_t::middle_center);
      spi->drawString(text, cx, textY);
    } else {
      // 長文 → 左へスクロール
      const int pause_ms = 800;
      const int scroll_speed_ms_per_px = 35;
      const int gap = 40;
      unsigned long elapsed = millis() - scroll_start_ms_;
      int offset = 0;
      if (elapsed > (unsigned long)pause_ms) {
        int loop_len = textWidth + gap;
        offset = ((elapsed - pause_ms) / scroll_speed_ms_per_px) % loop_len;
      }
      spi->setTextDatum(textdatum_t::middle_left);
      spi->drawString(text, textAreaLeft - offset, textY);
      // ループ表示（2周目）
      spi->drawString(text, textAreaLeft - offset + textWidth + gap, textY);
    }

    // クリップ解除
    spi->setClipRect(prevClipX, prevClipY, prevClipW, prevClipH);
  }

 private:
  String last_text_ = "";
  unsigned long scroll_start_ms_ = 0;
};

}  // namespace m5avatar

#endif  // BALLOON_H_
