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

#ifndef AGL_FONT_H
#define AGL_FONT_H

#ifdef QT_WIDGETS_LIB
#include <QtWidgets>
#endif

#include "agl_texture.h"
#include "../math_vec.h"
#include "../at_array.h"
#include "../astring.h"
#include "../pcache.h"
#include "../acolor.h"
#include "../aimage.h"

//todo: тесты тесты тесты

struct aglFontGlyphInfo
{
    real32 x,y,w,h;
    int tindex;
};

struct aglFontBlock
{
    ATArray<aglFontGlyphInfo> glyphs;
    ATArray<AGLTexture*> textures;
    real32 height;
    real32 spacing;
    bool spacing_is_abs;

    uint32 cacheIndex;
};

class AGLFont
{
public:
    AGLFont();
    AGLFont(AString font, int block_limit);
    ~AGLFont();

    void clear();

    void setFont(const AString &val){curr_font=val;}
    void setColor(const AColor val){curr_color=val;}
    void setAlpha(real32 val){curr_color.setAf(val);}

    vec2d<real32> printSize(const AString &val, int *lineCount=NULL);
    real32 print(const AString &val, const vec2d<real32> &pnt, real32 scale=1.0);
    void printInQuad(const AString &val,
                     vec3d<real32> lt, vec3d<real32> rt,
                     vec3d<real32> rd, vec3d<real32> ld,
                     AColor bg=AColor());

    enum BOXFLAGS
    {
        VER_MASK = 0x03,
        VER_UP = 0x00,
        VER_DOWN = 0x01,
        VER_CENTER = 0x02,
        VER_FULL = 0x03,
        HOR_MASK = 0x0c,
        HOR_LEFT = 0x00,
        HOR_RIGHT = 0x04,
        HOR_CENTER = 0x08,
        HOR_FULL = 0x0c,
        LINE_REORDER = 0x100
    };
    void printInBox(const AString &val, const AQuad<vec3d<real> > &box, AColor bg, int flags=0, real32 txscale=1.0);

    void printon(const AString &val, const vec2d<real32> &pnt, AColor bg, real32 scale=1.0)
    {
        vec2d<real32> sz=printSize(val);
        printInQuad(val,
                    vec3d<real32>(pnt.x,pnt.y,0.0),
                    vec3d<real32>(pnt.x+sz.x*scale,pnt.y,0.0),
                    vec3d<real32>(pnt.x+sz.x*scale,pnt.y+sz.y*scale,0.0),
                    vec3d<real32>(pnt.x,pnt.y+sz.y*scale,0.0),
                    bg
                    );
    }

    enum TextFlags
    {
        TF_PARAGRAF=1,
        TF_CENTER=2,
        TF_NOTAB=4
    };

    vec2d<real32> printInWidth(const AString &val, real32 width, real32 fontSize,
                        bool onlyCalculate=true, vec2d<real32> pnt=vec2d<real32>(),
                        int flags=TF_PARAGRAF);

protected:

#ifdef QT_WIDGETS_LIB
    virtual AImage createGlyph(charx sym);
    virtual real32 getSpacing();
    virtual bool spacingAbs();
#else
    virtual AImage createGlyph(charx sym) = 0;
    virtual real32 getSpacing() {return 0.0;}
    virtual bool spacingAbs() {return false;}    
#endif
    virtual void onStartBlock(){return;}
    virtual void onEndBlock(){return;}

    //настройки отображения
    AColor curr_color;
    AString curr_font;

    //создание и уничтожение блоков
    void createBlock(int index);
    void checkBlocksLifeCicle();

    //формат строки: Type:Size
    ATHash<AString, ATHash<int,aglFontBlock> > fonts;
    PriorityCache<ADual<AString,int> > cache;
    int blocklimit;

    //списки вершин и текстурных координат
    ATArray<GLfloat> vertexes;
    ATArray<GLfloat> coords;
};

#endif // AGL_FONT_H
