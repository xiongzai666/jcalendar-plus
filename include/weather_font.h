#pragma once
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

// A weather line is at most 48 x 16 pixels; no full display buffer is needed.
class WeatherFontBitmap : public Adafruit_GFX {
    uint8_t bits[96] = {};
public:
    WeatherFontBitmap() : Adafruit_GFX(48, 16) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || x >= 48 || y < 0 || y >= 16) return;
        const int bit = y * 48 + x;
        const uint8_t mask = 0x80 >> (bit % 8);
        if (color) bits[bit / 8] |= mask;
        else bits[bit / 8] &= ~mask;
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t width, uint16_t color) override {
        for (int i = 0; i < width; ++i) drawPixel(x + i, y, color);
    }
    void drawFastVLine(int16_t x, int16_t y, int16_t height, uint16_t color) override {
        for (int i = 0; i < height; ++i) drawPixel(x, y + i, color);
    }
    bool ink(int x, int y) const {
        const int bit = y * 48 + x;
        return bits[bit / 8] & (0x80 >> (bit % 8));
    }
};

// Nominal 15.5/16 = 31/32. The 16px height rounds back to 16 physical rows.
// Integer area coverage and a 50% threshold retain strictly black/white pixels.
template<class Canvas>
static bool drawWeather155(Canvas& canvas, const char* text, int center,
                           int baseline, uint16_t color) {
    WeatherFontBitmap bitmap;
    U8G2_FOR_ADAFRUIT_GFX font;
    font.begin(bitmap);
    font.setFont(u8g2_font_wqy16_t_gb2312);
    const int sourceWidth = font.getUTF8Width(text);
    if (sourceWidth <= 0 || sourceWidth > 48) return false;
    font.setFontMode(1);
    font.setFontDirection(0);
    font.setForegroundColor(1);
    font.setBackgroundColor(0);
    font.drawUTF8(0, 14, text);
    const int width = (sourceWidth * 31 + 16) / 32;
    const int left = center - width / 2;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < width; ++x) {
        const int start = x * sourceWidth, end = (x + 1) * sourceWidth;
        int covered = 0;
        for (int sx = start / width; sx < (end + width - 1) / width; ++sx) {
            const int a = start > sx * width ? start : sx * width;
            const int b = end < (sx + 1) * width ? end : (sx + 1) * width;
            if (bitmap.ink(sx, y)) covered += b - a;
        }
        if (covered * 2 >= sourceWidth)
            canvas.drawPixel(left + x, baseline - 14 + y, color);
    }
    return true;
}
