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

#ifndef AGLPRIMITIVE_H
#define AGLPRIMITIVE_H

#ifdef ANDROID_NDK
    #include <EGL/egl.h> // requires ndk r5 or newer
    #include <GLES/gl.h>
#elif __APPLE__
    #include <glu.h>
    #include <glext.h>
#elif QT_OPENGL_LIB
    #include <QtOpenGL>
    #include <GL/glu.h>
#else
    #include <GL/glu.h>
    #include <GL/glext.h>
#endif

#include "../amath_int.h"
#include "../amath_vec.h"
#include "../acolor.h"

class AGLPrimitive
{
public:
    AGLPrimitive();

    static void drawBox(alt::rect<real32> box, alt::colorRGBA col=alt::colorRGBA(), real32 border=0);
};

#endif // AGLPRIMITIVE_H
