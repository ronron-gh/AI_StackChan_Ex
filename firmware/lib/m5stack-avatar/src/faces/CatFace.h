#ifndef FACES_CATFACE_H_
#define FACES_CATFACE_H_

#include <M5Unified.h>
#include "../BoundingRect.h"
#include "../DrawContext.h"
#include "../Drawable.h"

namespace m5avatar {

class CatMouth : public Drawable {
 private:
  uint16_t minWidth;
  uint16_t maxWidth;
  uint16_t minHeight;
  uint16_t maxHeight;

 public:
  CatMouth() : CatMouth(50, 90, 4, 60) {}
  CatMouth(uint16_t minWidth, uint16_t maxWidth, uint16_t minHeight,
           uint16_t maxHeight)
      : minWidth{minWidth},
        maxWidth{maxWidth},
        minHeight{minHeight},
        maxHeight{maxHeight} {}
  void draw(M5Canvas *spi, BoundingRect rect, DrawContext *ctx) {
    uint16_t primaryColor = ctx->getColorDepth() == 1 ? 1 : ctx->getColorPalette()->get(COLOR_PRIMARY);
    uint16_t backgroundColor = ctx->getColorDepth() == 1 ? ERACER_COLOR : ctx->getColorPalette()->get(COLOR_BACKGROUND);
    uint32_t cx = rect.getCenterX();
    uint32_t cy = rect.getCenterY();
    float openRatio = ctx->getMouthOpenRatio();
    uint32_t h = minHeight + (maxHeight - minHeight) * openRatio;
    uint32_t w = minWidth + (maxWidth - minWidth) * (1 - openRatio);
    if (h > minHeight) {
      spi->fillEllipse(cx, cy, w / 2, h / 2, primaryColor);
      spi->fillEllipse(cx, cy, w / 2 - 4, h / 2 - 4, TFT_RED);
      spi->fillRect(cx - w / 2, cy - h / 2, w, h / 2, backgroundColor);
    }
    spi->fillEllipse(cx, cy - 13, 10, 6, primaryColor);
    spi->fillEllipse(cx - 20, cy, 20, 15, primaryColor);
    spi->fillEllipse(cx + 20, cy, 20, 15, primaryColor);
    spi->fillEllipse(cx - 21, cy - 4, 17, 15, backgroundColor);
    spi->fillEllipse(cx + 21, cy - 4, 17, 15, backgroundColor);
  }
};

class CatFace : public Face {
 public:
  CatFace()
      : Face(new CatMouth(), new BoundingRect(128, 163),
             new Eye(11, false), new BoundingRect(93, 90), 
             new Eye(11, true), new BoundingRect(96, 230), 
             new Eyeblow(15, 0, false), new BoundingRect(67, 96),
             new Eyeblow(15, 0, true), new BoundingRect(72, 230)) {}
};

}  // namespace m5avatar

#endif  // FACES_CATFACE_H_
