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


#ifndef QF_GL_TEXTURE_H
#define QF_GL_TEXTURE_H

#ifdef ANDROID_NDK
    #include <EGL/egl.h>
    #include <GLES/gl.h>
#elif QT_OPENGL_LIB
    #include <QtOpenGL>
    #include <GL/glu.h>
#else
    #include <GL/glu.h>
    #include <GL/glext.h>
#endif

#include "../math_int.h"
#include "../math_vec.h"
#include "../acolor.h"

/////////////////////////////////////////////////////////////////////////
//ТЕКСТУРНЫЙ ОБЪЕКТ
class AGLTexture
{
public:
    AGLTexture(){hand=NULL;}
#ifdef QT_WIDGETS_LIB
    AGLTexture(const QImage &img)
    {
        createBitmap(img.bits(),img.width(),img.height(),img.depth()<<16);
    }
#endif
    AGLTexture(const void *buff, int w, int h, uint32 format, int filter = 0);
    AGLTexture(int w, int h, uint32 format, int filter = 0);
    AGLTexture(const AGLTexture &val);
    ~AGLTexture();

    void resize(int w, int h, uint32 format, int filter = 0);
    void load(const void *buff, int w, int h, uint32 format, int filter = 0);

    void free();
    bool isValid(){return hand;}

    AGLTexture& operator=(const AGLTexture &val);

    void drawParticle(real32 x, real32 y, real32 z, real32 zoom, AColor col=AColor()) const;
    void drawRect(real32 x, real32 y, real32 w, real32 h) const;
    void drawRectPart(ATRect<real32> scr_part, ATRect<real32> tex_part) const;
    void drawRectPartInt(ATRect<real32> scr_part, ATRect<int32> tex_part) const;
    void drawFrameOver(int w, int h) const; //вписывает экран (w,h) в текстуру
    void drawBox(ATRect<real32> box, real32 border, AColor col=AColor());

    void MakeCurrent() const
    {
        if(!hand) return;
        glBindTexture(GL_TEXTURE_2D, hand->tex);
    }

    void Update(int x, int y, const void *buff, int w, int h, uint32 format);

    GLuint getID() const
    {
        if(!hand) return -1;
        return hand->tex;
    }

    real32 normalWidth() const
    {
        if(!hand) return 0.0;
        return (real32)originWidth()/(real32)hand->alwidth;
    }
    real32 normalHeight() const
    {
        if(!hand) return 0.0;
        return (real32)originHeight()/(real32)hand->alheight;
    }
    int alignedWidth() const
    {
        if(!hand) return 0;
        return hand->alwidth;
    }
    int alignedHeight() const
    {
        if(!hand) return 0;
        return hand->alheight;
    }
    int originWidth() const
    {
        if(!hand) return 0;
        return hand->width;
    }
    int originHeight() const
    {
        if(!hand) return 0;
        return hand->height;
    }
    real32 convX(int32 x) const
    {
        if(!hand) return 0.0;
        return (real32)x/(real32)hand->alwidth;
    }
    real32 convY(int32 y) const
    {
        if(!hand) return 0.0;
        return (real32)y/(real32)hand->alheight;
    }

    ATRect<int> originRect()
    {
        if(!hand) return ATRect<int>();
        return ATRect<int>(0,0,hand->width,hand->height);
    }
    ATRect<real32> normalRect()
    {
        if(!hand) return ATRect<real32>();
        return ATRect<int>(0,0,normalWidth(),normalHeight());
    }

    static int pixelSize(uint32 format);
    static bool hasAlpha(uint32 format);

private:

    struct Internal
    {
        int refcount;

        GLuint tex;
        int width, height;
        int alwidth, alheight;
        uint32 pixelFormat;
    };
    Internal *hand;

    void drawVertexes(GLfloat *vertexes, GLfloat *coords, int count, bool strip=true) const;

    void createBitmap(const void *buff, int w, int h, uint32 format, int filter = 0);
    void setBitmap(const void *buff, int w, int h, uint32 format);

};

#endif // QF_GL_TEXTURE_H
