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


#ifndef QF_GL_TEXTURE_H
#define QF_GL_TEXTURE_H

#ifdef ANDROID_NDK
    #include <EGL/egl.h> // requires ndk r5 or newer
    #include <GLES/gl.h>
#elif __APPLE__
    #include <glu.h>
    #include <glext.h>
#elif QT_OPENGL_LIB
    #undef WIN32_LEAN_AND_MEAN
    #include <QtOpenGL>
    #include <GL/glu.h>
#elif WIN32
    #include <windows.h>
    #include <GL/glu.h>
    #include "../external/glext/glext.h"
#else
    #include <GL/glu.h>
    #include <GL/glext.h>
#endif

#include "../amath_int.h"
#include "../amath_vec.h"
#include "../acolor.h"
#include "../abyte_array.h"

#include "atexture.h"

namespace alt {

struct GLApiResInternal
{
    GLuint tex;
    alt::byteArray buffer;
};

class GLTexture: public alt::texture
{
public:
    GLTexture(): alt::texture() {}
    GLTexture(const GLTexture &val): alt::texture(val) {}
    GLTexture(const void *buff, int w, int h, uint32 format, int filter = 0)
        : alt::texture()
    {
        resize(w,h,format,filter);
        update(0,0,buff, w, h, format);
    }
    GLTexture(int w, int h, uint32 format, int filter = 0)
        : alt::texture()
    {
        resize(w,h,format,filter);
    }
#ifdef QT_WIDGETS_LIB
    AGLTexture(const QImage &img)
        : alt::texture()
    {
        resize(img.width(),img.height(),img.depth()<<16);
        update(0,0,img.bits(),img.height(),img.depth()<<16);
    }
#endif

    ~GLTexture() { GLTexture::free(); }

    void resize(int w, int h, uint32 format, int filter = 0) final;
    void update(int x, int y, const void *buff, int w, int h, uint32 format) final;
    void free() final;
    void draw(vec3d<real32> *vertex, vec2d<real32> *coord, int count, bool strip, alt::colorRGBA col=alt::colorRGBA()) const final;

    uintz getID() const
    {
        if(!hand || !hand->api_related) return -1;
        return ((GLApiResInternal*)hand->api_related)->tex;
    }

    bool isValid() const final
    {
        return hand && hand->api_related;
    }

    void* mapData() final;
    void unmapData() final;


};

} //namespace alt

#endif // QF_GL_TEXTURE_H
