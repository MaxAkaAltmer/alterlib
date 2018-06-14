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

#include "agl_primitive.h"

AGLPrimitive::AGLPrimitive()
{

}

void AGLPrimitive::drawBox(ATRect<real32> box, AColor col)
{
    if(col()) glColor4f(col.rf(), col.gf(), col.bf(), col.af());
    else glColor4f(1.0, 1.0, 1.0, 1.0);

    GLfloat vertexes[3*4];

    vertexes[0*3+0]=box.x; vertexes[0*3+1]=box.y; vertexes[0*3+2]=0.0;
    vertexes[1*3+0]=box.x+box.width; vertexes[1*3+1]=box.y; vertexes[1*3+2]=0.0;
    vertexes[2*3+0]=box.x; vertexes[2*3+1]=box.y+box.height; vertexes[2*3+2]=0.0;
    vertexes[3*3+0]=box.x+box.width; vertexes[3*3+1]=box.y+box.height; vertexes[3*3+2]=0.0;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, vertexes);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
}
