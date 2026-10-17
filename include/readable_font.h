#pragma once
#include "readable_font_data.h"

static const ReadableGlyph* readableGlyph(const char*& text) {
    const auto* p = reinterpret_cast<const uint8_t*>(text);
    if (!p[0] || !p[1] || !p[2] || (p[0] & 0xf0) != 0xe0) return nullptr;
    const uint32_t code = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
    text += 3;
    for (const auto& glyph : READABLE_GLYPHS)
        if (pgm_read_dword(&glyph.codepoint) == code) return &glyph;
    return nullptr;
}

static int readableWidth(const char* text) {
    int width = 0;
    while (*text) {
        if (!readableGlyph(text)) return -1;
        width += 20;
    }
    return width;
}

// Center the actual ink vertically; no scaling or added stroke is applied.
template<class Canvas>
static bool drawReadable(Canvas& canvas, const char* text, int centerX, int centerY,
                         uint16_t ink, int maxWidth) {
    const int width = readableWidth(text);
    if (width <= 0 || width > maxWidth) return false;
    int top = 24, bottom = 0, left = width, right = 0, advance = 0;
    for (const char* p = text; *p;) {
        const auto* glyph = readableGlyph(p);
        const int a = pgm_read_byte(&glyph->top), b = pgm_read_byte(&glyph->bottom);
        const int l = advance + pgm_read_byte(&glyph->left), r = advance + pgm_read_byte(&glyph->right);
        if (l < left) left = l;
        if (r > right) right = r;
        advance += 20;
        if (a < top) top = a;
        if (b > bottom) bottom = b;
    }
    int x = centerX - (left + right) / 2;
    const int y = centerY - (top + bottom) / 2;
    while (*text) {
        const auto* glyph = readableGlyph(text);
        for (int row = top; row <= bottom; ++row)
            for (int col = 0; col < 20; ++col) {
                const int bit = row * 20 + col;
                if (pgm_read_byte(&glyph->pixels[bit / 8]) & (0x80 >> (bit % 8)))
                    canvas.drawPixel(x + col, y + row, ink);
            }
        x += 20;
    }
    return true;
}
