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

#include "agl_texture.h"
#include <memory.h>

AGLTexture::AGLTexture(const void *buff, int w, int h, uint32 format)
{
    //создаем текстуру
    createBitmap(buff,w,h,format);
}

void AGLTexture::load(const void *buff, int w, int h, uint32 format)
{
    free();
    createBitmap(buff,w,h,format);
}

void AGLTexture::createBitmap(const void *buff, int w, int h, uint32 format)
{
    hand = new Internal;
    hand->refcount=1;

    //создаем текстуру
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1,&hand->tex);
    glBindTexture(GL_TEXTURE_2D, hand->tex);

    hand->width=w;
    hand->height=h;
    int tmp_w=hand->alwidth=1<<__bsr32(w-1);
    int tmp_h=hand->alheight=1<<__bsr32(h-1);
    hand->pixelFormat=format;

    if(tmp_w!=w || tmp_h!=h)
    {
        uint8 *buffer=new uint8[tmp_h*tmp_w*pixelSize(format)];
        int koff=pixelSize(format);

        int j,i;
        for(i=0;i<hand->height;i++)
        {
            for(j=0;j<(hand->width*koff);j++)
                buffer[i*tmp_w*koff+j]=((const uint8*)buff)[(i*w*koff)+j];
            for(;j<(tmp_w*koff);j++)
                buffer[i*tmp_w*koff+j]=buffer[(i*tmp_w+hand->width-1)*koff+(j%koff)];
        }
        for(;i<tmp_h;i++)
        {
            memcpy(&buffer[i*tmp_w*koff],&buffer[(hand->height-1)*tmp_w*koff],tmp_w*koff);
        }

        //заполняем и настраиваем текстуру
        setBitmap(buffer,tmp_w,tmp_h,format);

        delete []buffer;
    }
    else
    {
        //заполняем и настраиваем текстуру
        setBitmap(buff,tmp_w,tmp_h,format);
    }

    //TODO: контроль поддерживаемых функций
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void AGLTexture::drawVertexes(GLfloat *vertexes, GLfloat *coords, int count, bool strip) const
{
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

    MakeCurrent();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, 0, vertexes);
    glTexCoordPointer(2, GL_FLOAT, 0, coords);
    glDrawArrays(strip?GL_TRIANGLE_STRIP:GL_TRIANGLES, 0, count);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void AGLTexture::drawFrameOver(int w, int h) const
{
    if(!hand)return;

    real32 asp=(real32)w/(real32)hand->width;
    real32 aspy=(real32)h/(real32)hand->height;
    if(aspy>asp)asp=aspy;

    real32 dx=(w-asp*hand->width)/2.0;
    real32 dy=(h-asp*hand->height)/2.0;

    GLfloat vertexes[3*4];
    GLfloat coords[2*4];

    vertexes[0*3+0]=dx; vertexes[0*3+1]=dy; vertexes[0*3+2]=0.0;
    vertexes[1*3+0]=-dx+w; vertexes[1*3+1]=dy; vertexes[1*3+2]=0.0;
    vertexes[2*3+0]=dx; vertexes[2*3+1]=-dy+h; vertexes[2*3+2]=0.0;
    vertexes[3*3+0]=-dx+w; vertexes[3*3+1]=-dy+h; vertexes[3*3+2]=0.0;

    coords[0*2+0]=0.0; coords[0*2+1]=0.0;
    coords[1*2+0]=normalWidth(); coords[1*2+1]=0.0;
    coords[2*2+0]=0.0; coords[2*2+1]=normalHeight();
    coords[3*2+0]=normalWidth(); coords[3*2+1]=normalHeight();

    glColor4f(1.0, 1.0, 1.0, 1.0);
    drawVertexes(vertexes,coords,4);
}

void aglqMakeTriangles(GLfloat *vertexes, GLfloat *coords, ATRect<real32> scr, ATRect<real32> tex,
                       bool flipH=false, bool flipV=false)
{
    vertexes[0*3+0]=scr.tl().x; vertexes[0*3+1]=scr.tl().y; vertexes[0*3+2]=0.0;
    vertexes[1*3+0]=scr.tr().x; vertexes[1*3+1]=scr.tr().y; vertexes[1*3+2]=0.0;
    vertexes[2*3+0]=scr.dl().x; vertexes[2*3+1]=scr.dl().y; vertexes[2*3+2]=0.0;

    vertexes[3*3+0]=scr.tr().x; vertexes[3*3+1]=scr.tr().y; vertexes[3*3+2]=0.0;
    vertexes[4*3+0]=scr.dr().x; vertexes[4*3+1]=scr.dr().y; vertexes[4*3+2]=0.0;
    vertexes[5*3+0]=scr.dl().x; vertexes[5*3+1]=scr.dl().y; vertexes[5*3+2]=0.0;

    coords[0*2+0]=flipH?tex.tr().x:tex.tl().x; coords[0*2+1]=flipV?tex.dl().y:tex.tl().y;
    coords[1*2+0]=flipH?tex.tl().x:tex.tr().x; coords[1*2+1]=flipV?tex.dr().y:tex.tr().y;
    coords[2*2+0]=flipH?tex.dr().x:tex.dl().x; coords[2*2+1]=flipV?tex.tl().y:tex.dl().y;

    coords[3*2+0]=flipH?tex.tl().x:tex.tr().x; coords[3*2+1]=flipV?tex.dr().y:tex.tr().y;
    coords[4*2+0]=flipH?tex.dl().x:tex.dr().x; coords[4*2+1]=flipV?tex.tr().y:tex.dr().y;
    coords[5*2+0]=flipH?tex.dr().x:tex.dl().x; coords[5*2+1]=flipV?tex.tl().y:tex.dl().y;
}

void AGLTexture::drawBox(ATRect<real32> box, real32 border, AColor col)
{
    if(!hand)return;
    if(border>((box.width-1.0)/2.0)) border=(box.width-1.0)/2.0;
    if(border>((box.height-1.0)/2.0)) border=(box.height-1.0)/2.0;

    if(col()) glColor4f(col.rf(), col.gf(), col.bf(), col.af());
    else glColor4f(1.0, 1.0, 1.0, 1.0);

    GLfloat vertexes[3*9*6];
    GLfloat coords[2*9*6];

    ATRect<real32> trc=normalRect();

    aglqMakeTriangles(vertexes,coords,
                      ATRect<real32>(box.x,box.y,border,border),
                      ATRect<real32>(trc.x,trc.y,trc.width,trc.height));

    aglqMakeTriangles(vertexes+3*6,coords+2*6,
                      ATRect<real32>(box.x+border,box.y,box.width-2*border,border),
                      ATRect<real32>(trc.width-convX(1),trc.y,convX(1),trc.height));

    aglqMakeTriangles(vertexes+2*3*6,coords+2*2*6,
                      ATRect<real32>(box.x+box.width-border,box.y,border,border),
                      ATRect<real32>(trc.x,trc.y,trc.width,trc.height),true);

    aglqMakeTriangles(vertexes+3*3*6,coords+3*2*6,
                      ATRect<real32>(box.x,box.y+border,border,box.height-2*border),
                      ATRect<real32>(trc.x,trc.height-convY(1),trc.width,convY(1)));

    aglqMakeTriangles(vertexes+4*3*6,coords+4*2*6,
                      ATRect<real32>(box.x+border,box.y+border,box.width-2*border,box.height-2*border),
                      ATRect<real32>(trc.width-convX(1),trc.height-convY(1),convX(1),convY(1)));

    aglqMakeTriangles(vertexes+5*3*6,coords+5*2*6,
                      ATRect<real32>(box.x+box.width-border,box.y+border,border,box.height-2*border),
                      ATRect<real32>(trc.x,trc.height-convY(1),trc.width,convY(1)),true);

    aglqMakeTriangles(vertexes+6*3*6,coords+6*2*6,
                      ATRect<real32>(box.x,box.y+box.height-border,border,border),
                      ATRect<real32>(trc.x,trc.y,trc.width,trc.height),false,true);

    aglqMakeTriangles(vertexes+7*3*6,coords+7*2*6,
                      ATRect<real32>(box.x+border,box.y+box.height-border,box.width-2*border,border),
                      ATRect<real32>(trc.width-convX(1),trc.y,convX(1),trc.height),false,true);

    aglqMakeTriangles(vertexes+8*3*6,coords+8*2*6,
                      ATRect<real32>(box.x+box.width-border,box.y+box.height-border,border,border),
                      ATRect<real32>(trc.x,trc.y,trc.width,trc.height),true,true);

    drawVertexes(vertexes,coords,9*6,false);
}

void AGLTexture::drawRectPartInt(ATRect<real32> scr_part, ATRect<int32> tex_part) const
{
    ATRect<real32> rc(convX(tex_part.x),convY(tex_part.y),convX(tex_part.width),convY(tex_part.height));
    drawRectPart(scr_part,rc);
}

void AGLTexture::drawRectPart(ATRect<real32> scr_part, ATRect<real32> tex_part) const
{
    if(!hand)return;

    GLfloat vertexes[3*4];
    GLfloat coords[2*4];

    vertexes[0*3+0]=scr_part.tl().x; vertexes[0*3+1]=scr_part.tl().y; vertexes[0*3+2]=0.0;
    vertexes[1*3+0]=scr_part.dr().x; vertexes[1*3+1]=scr_part.tl().y; vertexes[1*3+2]=0.0;
    vertexes[2*3+0]=scr_part.tl().x; vertexes[2*3+1]=scr_part.dr().y; vertexes[2*3+2]=0.0;
    vertexes[3*3+0]=scr_part.dr().x; vertexes[3*3+1]=scr_part.dr().y; vertexes[3*3+2]=0.0;

    coords[0*2+0]=convX(tex_part.tl().x); coords[0*2+1]=convY(tex_part.tl().y);
    coords[1*2+0]=convX(tex_part.dr().x); coords[1*2+1]=convY(tex_part.tl().y);
    coords[2*2+0]=convX(tex_part.tl().x); coords[2*2+1]=convY(tex_part.dr().y);
    coords[3*2+0]=convX(tex_part.dr().x); coords[3*2+1]=convY(tex_part.dr().y);

    glColor4f(1.0, 1.0, 1.0, 1.0);
    drawVertexes(vertexes,coords,4);
}

void AGLTexture::drawRect(real32 x, real32 y, real32 w, real32 h) const
{
    if(!hand)return;

    GLfloat vertexes[3*4];
    GLfloat coords[2*4];

    vertexes[0*3+0]=x; vertexes[0*3+1]=y; vertexes[0*3+2]=0.0;
    vertexes[1*3+0]=x+w; vertexes[1*3+1]=y; vertexes[1*3+2]=0.0;
    vertexes[2*3+0]=x; vertexes[2*3+1]=y+h; vertexes[2*3+2]=0.0;
    vertexes[3*3+0]=x+w; vertexes[3*3+1]=y+h; vertexes[3*3+2]=0.0;

    coords[0*2+0]=0.0; coords[0*2+1]=0.0;
    coords[1*2+0]=normalWidth(); coords[1*2+1]=0.0;
    coords[2*2+0]=0.0; coords[2*2+1]=normalHeight();
    coords[3*2+0]=normalWidth(); coords[3*2+1]=normalHeight();

    glColor4f(1.0, 1.0, 1.0, 1.0);
    drawVertexes(vertexes,coords,4);
}

void AGLTexture::drawParticle(real32 x, real32 y, real32 z, real32 zoom, AColor col) const
{
    if(!hand)return;

    GLfloat vertexes[3*4];
    GLfloat coords[2*4];

    real32 dx=originWidth()*0.5*zoom;
    real32 dy=originHeight()*0.5*zoom;

    vertexes[0*3+0]=x-dx; vertexes[0*3+1]=y-dy; vertexes[0*3+2]=z;
    vertexes[1*3+0]=x+dx; vertexes[1*3+1]=y-dy; vertexes[1*3+2]=z;
    vertexes[2*3+0]=x-dx; vertexes[2*3+1]=y+dy; vertexes[2*3+2]=z;
    vertexes[3*3+0]=x+dx; vertexes[3*3+1]=y+dy; vertexes[3*3+2]=z;

    coords[0*2+0]=0.0; coords[0*2+1]=0.0;
    coords[1*2+0]=normalWidth(); coords[1*2+1]=0.0;
    coords[2*2+0]=0.0; coords[2*2+1]=normalHeight();
    coords[3*2+0]=normalWidth(); coords[3*2+1]=normalHeight();

    if(col()) glColor4f(col.rf(), col.gf(), col.bf(), col.af());
    else glColor4f(1.0, 1.0, 1.0, 1.0);

    drawVertexes(vertexes,coords,4);
}

bool AGLTexture::hasAlpha(uint32 format)
{
   if(((format>>16)&0xff)==32)return true;
   if(format&0xf000)return true;
   return false;
}

int AGLTexture::pixelSize(uint32 format)
{
    int rv=(format>>16)&0xff;
    if(!rv)rv=(format&0xf)+(((format>>4)&0xf))
            +(((format>>8)&0xf))+(((format>>12)&0xf));
    return (rv+7)>>3;
}

AGLTexture::AGLTexture(int w, int h, uint32 format)
{
    hand = new Internal;
    hand->refcount=1;

    //создаем текстуру
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1,&hand->tex);
    glBindTexture(GL_TEXTURE_2D, hand->tex);

    //создаем растр
    //TODO: контроль размера текстуры и ресайзинг
    hand->pixelFormat = format;
    hand->width=w;
    hand->height=h;
    int tmp_w=hand->alwidth=1<<__bsr32(hand->width-1);;
    int tmp_h=hand->alheight=1<<__bsr32(hand->height-1);;
    uint8 *buffer=new uint8[tmp_h*tmp_w*pixelSize(format)];

    memset(buffer,0,tmp_h*tmp_w*pixelSize(format));

    //заполняем и настраиваем текстуру
    setBitmap(buffer,tmp_w,tmp_h,format);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //чистим память
    delete []buffer;
}

AGLTexture::AGLTexture(const AGLTexture &val)
{
    hand=val.hand;
    if(hand)hand->refcount++;
}

AGLTexture& AGLTexture::operator=(const AGLTexture &val)
{
    free();
    hand=val.hand;
    if(hand)hand->refcount++;
    return *this;
}

AGLTexture::~AGLTexture()
{
    free();
}

void AGLTexture::free()
{
    if(!hand)return;
    hand->refcount--;
    if(!hand->refcount)
    {
        glDeleteTextures(1, &hand->tex);
        delete hand;
    }
    hand=NULL;
}

void AGLTexture::setBitmap(const void *buff, int w, int h, uint32 format)
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
}

void AGLTexture::Update(int x, int y, const void *buff, int w, int h, uint32 format)
{
    if(!hand)return;

    glBindTexture(GL_TEXTURE_2D, hand->tex);

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
}
