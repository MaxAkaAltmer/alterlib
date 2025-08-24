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

#include "agl_texture.h"
#include <memory.h>

using namespace alt;

void* GLTexture::mapData()
{
    if(!hand || !hand->api_related) return nullptr;
    GLApiResInternal *api = (GLApiResInternal*)hand->api_related;

    api->buffer.resize(hand->width*hand->height*pixelSize(hand->pixelFormat));

    return api->buffer();
}
void GLTexture::unmapData()
{
    if(!hand || !hand->api_related) return;
    GLApiResInternal *api = (GLApiResInternal*)hand->api_related;

    update(0,0,api->buffer(),hand->width,hand->height,hand->pixelFormat);
}

void GLTexture::draw(vec3d<real32> *vertex, vec2d<real32> *coord, int count, bool strip, alt::colorRGBA col) const
{
    if(!hand || !hand->api_related) return;
    GLApiResInternal *api = (GLApiResInternal*)hand->api_related;

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);

    if(hasAlpha(hand->pixelFormat))
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    glBindTexture(GL_TEXTURE_2D, api->tex);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, vertex);
    glTexCoordPointer(2, GL_FLOAT, 0, coord);
    glDrawArrays(strip?GL_TRIANGLE_STRIP:GL_TRIANGLES, 0, count);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void GLTexture::resize(int w, int h, uint32 format, int filter)
{
    if(hand && hand->api_related && hand->alheight >= h && hand->alwidth >= w && hand->pixelFormat == format)
    {
        hand->width = w;
        hand->height = h;
        return;
    }

    free();

    hand = new Internal;
    hand->alheight = h;
    hand->alwidth = w;
    hand->height = h;
    hand->width = w;
    hand->pixelFormat = format;
    hand->refcount = 1;
    hand->api_related = new GLApiResInternal;

    GLApiResInternal *api = (GLApiResInternal*)hand->api_related;

    //создаем текстуру
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1,&api->tex);
    glBindTexture(GL_TEXTURE_2D, api->tex);

    //создаем растр
    //TODO: контроль размера текстуры и ресайзинг
    hand->pixelFormat = format;
    hand->width=w;
    hand->height=h;
    hand->alwidth=1<<alt::imath::bsr32(hand->width-1);;
    hand->alheight=1<<alt::imath::bsr32(hand->height-1);;

    //заполняем и настраиваем текстуру
    update(0,0,nullptr,hand->alwidth,hand->alheight,format);

    if(filter<0)
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

}

void GLTexture::free()
{
    if(!hand) return;
    hand->refcount --;

    if(!hand->refcount && hand->api_related)
    {
        GLApiResInternal *api = (GLApiResInternal*)hand->api_related;
        delete api;
    }
    delete hand;
    hand = nullptr;
}

void GLTexture::update(int x, int y, const void *buff, int w, int h, uint32 format)
{
    if(!hand || !hand->api_related) return;
    GLApiResInternal *api = (GLApiResInternal*)hand->api_related;

    glBindTexture(GL_TEXTURE_2D, api->tex);

    if(!x && !y && w == hand->alwidth && h == hand->alheight)
    {
        if(format==(32<<16) || format==0x8888)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==(24<<16) || format==0x888)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==0x4444)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, buff);
        }
        else if(format==(16<<16) || format==0x565)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buff);
        }
        else if(format==(8<<16) || format==0x8000)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0,
                     GL_ALPHA, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==0x1008888)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                         GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buff);
        }
        else if(format == 0x4f)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                         GL_RGBA, GL_FLOAT, buff);
        }
    }
    else
    {
        if(format==(8<<16) || format==0x8000)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_ALPHA, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==0x4444)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, buff);
        }
        else if(format==(16<<16) || format==0x565)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buff);
        }
        else if(format==(24<<16) || format==0x888)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGB, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==(32<<16) || format==0x8888)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGBA, GL_UNSIGNED_BYTE, buff);
        }
        else if(format==0x1008888)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buff);
        }
        else if(format == 0x4f)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y,
                            w, h,
                            GL_RGBA, GL_FLOAT, buff);
        }
    }
}
