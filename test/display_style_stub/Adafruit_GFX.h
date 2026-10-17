#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
class Print { public: virtual size_t write(uint8_t)=0; virtual ~Print(){}; };
// Hardware-only replacement. All text layout and font decoding remain production code.
class Adafruit_GFX {
public:
 uint16_t pixels[400*300];
 bool recording=false; int glyphPixels=0,minX=400,minY=300,maxX=-1,maxY=-1;
 void reset(){std::fill(pixels,pixels+400*300,uint16_t(0xffff));}
 Adafruit_GFX(){reset();}
 void beginGlyph(){recording=true;glyphPixels=0;minX=400;minY=300;maxX=-1;maxY=-1;}
 void pixel(int x,int y,uint16_t color){
  if(recording){++glyphPixels;minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);}
  if(x>=0&&x<400&&y>=0&&y<300)pixels[y*400+x]=color;
 }
 void drawPixel(int x,int y,uint16_t color){pixel(x,y,color);}
 void drawFastHLine(int16_t x,int16_t y,int16_t n,uint16_t color){for(int i=0;i<n;++i)pixel(x+i,y,color);}
 void drawFastVLine(int16_t x,int16_t y,int16_t n,uint16_t color){for(int i=0;i<n;++i)pixel(x,y+i,color);}
 void fillRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){for(int i=0;i<h;++i)drawFastHLine(x,y+i,w,c);}
 void drawRect(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
};
