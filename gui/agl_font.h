/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2023 Maxim L. Grishin  (altmer@arts-union.ru)

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
#include "../amath_vec.h"
#include "../at_array.h"
#include "../astring.h"
#include "../at_priority_cache.h"
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
    alt::array<aglFontGlyphInfo> glyphs;
    alt::array<AGLTexture*> textures;
    real32 height;
    real32 spacing;
    bool spacing_is_abs;

    uint32 cacheIndex;
};

class AGLFont
{
public:
    AGLFont();
    AGLFont(alt::string font, int block_limit);
    virtual ~AGLFont();

    void clear();

    void setFont(const alt::string &val){curr_font=val;}
    void setColor(const alt::colorRGBA val){curr_color=val;}
    void setAlpha(real32 val){curr_color.setAf(val);}

    alt::vec2d<real32> printSize(const alt::string &val, int *lineCount=NULL);
    real32 print(const alt::string &val, const alt::vec2d<real32> &pnt, real32 scale=1.0);
    void printInQuad(const alt::string &val,
                     alt::vec3d<real32> lt, alt::vec3d<real32> rt,
                     alt::vec3d<real32> rd, alt::vec3d<real32> ld,
                     alt::colorRGBA bg=alt::colorRGBA());

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
    void printInBox(const alt::string &val, const alt::quad3d<real > &box, alt::colorRGBA bg, int flags=0, real32 txscale=1.0);

    void printon(const alt::string &val, const alt::vec2d<real32> &pnt, alt::colorRGBA bg, real32 scale=1.0)
    {
        alt::vec2d<real32> sz=printSize(val);
        printInQuad(val,
                    alt::vec3d<real32>(pnt.x,pnt.y,0.0),
                    alt::vec3d<real32>(pnt.x+sz.x*scale,pnt.y,0.0),
                    alt::vec3d<real32>(pnt.x+sz.x*scale,pnt.y+sz.y*scale,0.0),
                    alt::vec3d<real32>(pnt.x,pnt.y+sz.y*scale,0.0),
                    bg
                    );
    }

    enum TextFlags
    {
        TF_PARAGRAF=1,
        TF_CENTER=2,
        TF_NOTAB=4
    };

    alt::vec2d<real32> printInWidth(const alt::string &val, real32 width, real32 fontSize,
                        bool onlyCalculate=true, alt::vec2d<real32> pnt=alt::vec2d<real32>(),
                        int flags=TF_PARAGRAF);

protected:

    virtual alt::image createGlyph(charx sym);
    virtual real32 getSpacing();
    virtual bool spacingAbs();

    virtual void onStartBlock(){return;}
    virtual void onEndBlock(){return;}

    //настройки отображения
    alt::colorRGBA curr_color;
    alt::string curr_font;

    //создание и уничтожение блоков
    void createBlock(int index);
    void checkBlocksLifeCicle();

    //формат строки: Type:Size
    alt::hash<alt::string, alt::hash<int,aglFontBlock> > fonts;
    alt::cache<alt::pair<alt::string,int> > cache;
    int blocklimit;

    #ifndef QT_WIDGETS_LIB
    alt::hash<alt::string, void*> fonts_data;
    #endif

    //списки вершин и текстурных координат
    alt::array<GLfloat> vertexes;
    alt::array<GLfloat> coords;
};

#endif // AGL_FONT_H
