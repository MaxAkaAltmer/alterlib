/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2018 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#ifndef ACOLOR_H
#define ACOLOR_H

#include "types.h"

class AColor
{
public:
    AColor(){color = 0;}
    AColor(const AColor &val){color=val.color;}
    AColor(uint32 val){color=val;}
    AColor(uint8 r, uint8 g, uint8 b, uint8 a=255){set(r,g,b,a);}

    AColor& operator=(const AColor &val){color=val.color;return *this;}
    AColor& operator=(uint32 val){color=val;return *this;}

    AColor swapRGB(){return (color&0xff00ff00)|((color>>16)&0xff)|((color&0xff)<<16);}

    AColor scale(real32 factor)
    {
        uint rc=r()*factor;
        uint gc=g()*factor;
        uint bc=b()*factor;
        return AColor(rc>255?255:rc,gc>255?255:gc,bc>255?255:bc,a());
    }

    AColor scaleTransparent(real32 factor)
    {
        uint ac=a()*factor;
        return AColor(r(),g(),b(),ac);
    }

    uint8 r(){return color&0xff;}
    uint8 g(){return (color>>8)&0xff;}
    uint8 b(){return (color>>16)&0xff;}
    uint8 a(){return (color>>24)&0xff;}

    real32 rf(){return r()/255.0;}
    real32 gf(){return g()/255.0;}
    real32 bf(){return b()/255.0;}
    real32 af(){return a()/255.0;}

    void setA(uint8 val){color=(color&0xffffff)|(uint32(val)<<24);}
    void setG(uint8 val){color=(color&0xffff00ff)|(uint32(val)<<8);}
    void setB(uint8 val){color=(color&0xff00ffff)|(uint32(val)<<16);}
    void setR(uint8 val){color=(color&0xffffff00)|val;}

    void setAf(real32 val){int tmp=val*255; if(tmp<0)tmp=0; if(tmp>255)tmp=255; color=(color&0xffffff)|(tmp<<24);}
    void setGf(real32 val){int tmp=val*255; if(tmp<0)tmp=0; if(tmp>255)tmp=255; color=(color&0xffff00ff)|(tmp<<8);}
    void setBf(real32 val){int tmp=val*255; if(tmp<0)tmp=0; if(tmp>255)tmp=255; color=(color&0xff00ffff)|(tmp<<16);}
    void setRf(real32 val){int tmp=val*255; if(tmp<0)tmp=0; if(tmp>255)tmp=255; color=(color&0xffffff00)|tmp;}

    void setf(real32 r, real32 g, real32 b, real32 a=1.0){ setAf(a);setRf(r);setGf(g);setBf(b); }
    void set(uint8 r, uint8 g, uint8 b, uint8 a=255){ color=(uint32)r|(uint32(g)<<8)|(uint32(b)<<16)|(uint32(a)<<24); }

    uint32& operator()(){return color;}

private:

    uint32 color;
};

#endif // ACOLOR_H
