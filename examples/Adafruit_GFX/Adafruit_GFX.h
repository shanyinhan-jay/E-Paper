#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#include "Arduino.h"
#include "Print.h"

class Adafruit_GFX : public Print {
public:
  Adafruit_GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h) {}
  
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
  
  virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    for (int16_t i = 0; i < h; i++) {
        drawPixel(x, y + i, color);
    }
  }

  virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    for (int16_t i = 0; i < w; i++) {
        drawPixel(x + i, y, color);
    }
  }
  
  // Minimal Print implementation
  virtual size_t write(uint8_t c) { return 1; }
  virtual size_t write(const uint8_t *buffer, size_t size) { return size; }

protected:
  int16_t WIDTH, HEIGHT;
};

#endif
