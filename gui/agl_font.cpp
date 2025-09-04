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

#include "agl_font.h"
#include "../afile.h"
#include <math.h>

AGLFont::AGLFont(alt::string font, int block_limit)
{
    //дефолтные настройки
    blocklimit=block_limit;
    curr_color=alt::colorRGBA(255,255,255,255);
    curr_font=font;
}
AGLFont::AGLFont()
{
    blocklimit=16;
    #ifdef QT_WIDGETS_LIB
    QFont fnt("Courier",14);
    fnt.setBold(true);
    setFont(fnt.toString());
    #endif
}

#ifdef QT_WIDGETS_LIB
alt::image AGLFont::createGlyph(charx sym)
{
    //шрифт
    QFont font;
    font.fromString(curr_font);
    QFontMetrics fm(font);

    QString val=QString(QChar(sym));
    real32 cw=fm.width(val);
    real32 ch=fm.height();

    if(cw<=0 || ch<=0)return alt::image();

    //рендер
    QPicture gly;
    QPainter qp;
    qp.begin(&gly);
    qp.setFont(font);
    QPen pen;
    pen.setColor(QColor(255,255,255,255));
    qp.setPen(pen);

    qp.drawText(QRectF(0,0,cw,ch), val);

    //генерим текстуру
    qp.end();
    QImage pix(cw,ch,QImage::Format_ARGB32);
    pix.fill(QColor(255,255,255, 0));
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    gly.play(&p);

    return alt::image(pix.width(),pix.height(),pix.bits());
}

real32 AGLFont::getSpacing()
{
    QFont font;
    font.fromString(curr_font);
    return font.letterSpacing();
}

bool AGLFont::spacingAbs()
{
    QFont font;
    font.fromString(curr_font);
    return (font.letterSpacingType()==QFont::AbsoluteSpacing)?true:false;
}
#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "../external/stb/stb_truetype.h"
#include "battery/embed.hpp"

struct fontDatum
{
    const uint8 *static_font_data = nullptr;
    uint8 *dynamic_font_data = nullptr;
    uintz size;
    stbtt_fontinfo font;
};

alt::image AGLFont::createGlyph(charx sym)
{
    fontDatum *datum = nullptr;
    if(fonts_data.contains(curr_font))
    {
        datum = (fontDatum*)fonts_data[curr_font];
    }
    else
    {
        std::vector<std::string> list = b::embed_list();
        for(int i=0;i<list.size();i++)
        {
            if(list[i] == curr_font())
            {
                datum = new fontDatum;
                datum->static_font_data = b::embed(list[i]).data();
                datum->size = b::embed(list[i]).size();
                break;
            }
        }
        if(!datum)
        {
            alt::file hand(curr_font);
            if(hand.open())
            {
                datum = new fontDatum;
                datum->size = hand.size();
                datum->dynamic_font_data = new uint8[datum->size];
                if(datum->size != hand.read(datum->dynamic_font_data,datum->size))
                {
                    delete [](datum->dynamic_font_data);
                    delete datum;
                    datum = nullptr;
                }
            }
        }

        if(datum)
        {
            const uint8 *font_buffer = datum->dynamic_font_data ? datum->dynamic_font_data : datum->static_font_data;
            if (!stbtt_InitFont(&datum->font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer,0)))
            {
                if(datum->dynamic_font_data)
                    delete []datum->dynamic_font_data;
                delete datum;
                datum = nullptr;
            }
        }
    }
    if(!datum)
        return alt::image();

    int font_pixel_height = 16; //todo: configurable

    int width, height, xoff, yoff;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(
        &datum->font, 0, stbtt_ScaleForPixelHeight(&datum->font, font_pixel_height),
        sym, &width, &height, &xoff, &yoff
    );

    alt::image glyph(width,height);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x) {
            int alpha = bitmap[y * width + x];
            glyph[(y * width + x)] = 0xffffff|(alpha<<24);
        }
    }

    stbtt_FreeBitmap(bitmap, NULL);

    return glyph;
}

real32 AGLFont::getSpacing()
{
    return 0.0;
}

bool AGLFont::spacingAbs()
{
    return true;
}

#endif

void AGLFont::clear()
{
    alt::array< alt::hash<int,aglFontBlock> > fparts=fonts.values();
    for(int i=0;i<fparts.size();i++)
    {
        alt::array<aglFontBlock> blocks=fparts[i].values();
        for(int j=0;j<blocks.size();j++)
        {
            alt::array<alt::GLTexture*> texs=blocks[j].textures;
            for(int k=0;k<texs.size();k++)
            {
                delete texs[k];
            }
        }
    }
    fonts.clear();
    cache.Clear();

    #ifndef QT_WIDGETS_LIB
    for(int i=0;i<fonts_data.size();i++)
    {
        fontDatum *datum = (fontDatum*)fonts_data.value(i);
        if(datum->dynamic_font_data)
            delete []datum->dynamic_font_data;
        delete datum;
    }
    #endif
}

AGLFont::~AGLFont()
{
    clear();
}

alt::vec2d<real32> AGLFont::printSize(const alt::string &val, int *lineCount)
{
    alt::vec2d<real32> siz(0,0);
    if(val.isEmpty())return siz;

    uint32 uval=0;
    val.unicode_at(0,uval);
    uint32 index=uval>>8;
    aglFontBlock *block;

    //создаем блоки
    if(!fonts.contains(curr_font))createBlock(index);
    if(!fonts[curr_font].contains(index))createBlock(index);

    block=&fonts[curr_font][index];
    cache.Update(block->cacheIndex);

    if(lineCount)*lineCount=1;

    real32 xoff=0.0,yoff=0.0;
    for(int i=0;i<val.size();)
    {
        i+=val.unicode_at(i,uval);
        if((uval>>8)!=index)
        {
            index=(uval>>8);
            //создаем блоки
            if(!fonts.contains(curr_font))createBlock(index);
            if(!fonts[curr_font].contains(index))createBlock(index);
            block=&fonts[curr_font][index];
            cache.Update(block->cacheIndex);
        }

        //учитываем перенос строки
        if(uval==uint8('\r'))continue;
        if(uval==uint8('\n'))
        {
            if(siz.x<xoff)siz.x=xoff;
            xoff=0.0;
            yoff+=block->height;
            if(lineCount)(*lineCount)++;
            continue;
        }

        int ind=uval&0xff;

        aglFontGlyphInfo *inf=&block->glyphs[ind];
        real32 cwidth=inf->w*256.0;

        real32 incsz=(block->spacing_is_abs?(block->spacing):(cwidth+block->spacing));
        if(uval==uint8('\t'))
        {
            int cnt=xoff/incsz;
            xoff=(cnt+1)*incsz;
        }
        else
        {
            xoff+=incsz;
        }
    }
    yoff+=block->height;
    if(siz.x<xoff)siz.x=xoff;
    siz.y=yoff;

    return siz;
}

void AGLFont::printInBox(const alt::string &val, const alt::quad3d<real > &box, alt::colorRGBA bg, int flags, real32 txscale)
{
    if(val.isEmpty())return;
    int count;
    alt::vec2d<real32> size=printSize(val,&count)*txscale;

    alt::array<alt::string> lines=val.split('\n');
    alt::array<bool> format;
    format.resize(lines.size()).fill(false);

    alt::vec3d<real> midUp=(box.leftUp()+box.rightUp())/2.0;
    alt::vec3d<real> midDown=(box.leftDown()+box.rightDown())/2.0;

    alt::vec3d<real> midLeft=(box.leftUp()+box.leftDown())/2.0;
    alt::vec3d<real> midRight=(box.rightUp()+box.rightDown())/2.0;

    real h=midUp.Distance(midDown);
    real w=midLeft.Distance(midRight);

    //попытка перестроить строки
    if(((flags&LINE_REORDER) || (flags&HOR_MASK)==HOR_FULL) && w<size.x && (w/h)<(size.x/size.y))
    {
        format.clear();
        real sbox=w*h;
        real stxt=size.x*size.y;
        real limit;
        real kofflim=1.0;
        if(sbox<stxt)
        {
            kofflim=sqrt(stxt/sbox);
            limit=kofflim*w/size.x;
        }
        else
        {
            limit=w/size.x;
        }

        alt::array<alt::string> new_lines;
        for(int i=0;i<lines.size();i++)
        {
            alt::vec2d<real32> lsiz=printSize(lines[i])*txscale;
            real ll=kofflim*lsiz.x/size.x;
            if(ll>limit)
            {
                alt::array<alt::string> words=lines[i].split(' ',true);
                alt::vec2d<real32> spsiz=printSize(" ")*txscale;
                real currlim=0.0;
                alt::string tstr;
                for(int j=0;j<words.size();j++)
                {
                    alt::vec2d<real32> wsiz=printSize(words[j])*txscale;
                    if(!tstr.isEmpty())tstr+=" ";
                    tstr+=words[j];
                    currlim+=kofflim*(wsiz.x+spsiz.x)/(real)size.x;
                    if(currlim>limit)
                    {
                        new_lines.append(tstr);
                        if(j!=words.size()-1)format.append(true);
                        else format.append(false);
                        tstr.clear();
                        currlim=0.0;
                    }
                }
                if(!tstr.isEmpty())
                {
                    new_lines.append(tstr);
                    format.append(false);
                }
            }
            else
            {
                new_lines.append(lines[i]);
                format.append(false);
            }
        }
        lines=new_lines;
        size=printSize(alt::string::join(lines,'\n'),&count)*txscale;
    }

    real hstep,hstart,hline;

    real scale=1.0;
    if(h<size.y)
    {
        scale=h/size.y;
    }
    if(w<size.x)
    {
        real tmp=w/size.x;
        if(tmp<scale)scale=tmp;
    }

    //разрешим прозрачность
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    //отрисуем бэкграунд
    if(bg())
    {
        glColor4f(bg.rf(),bg.gf(),
                  bg.bf(),bg.af());
        glDisable(GL_TEXTURE_2D);

        glEnableClientState(GL_VERTEX_ARRAY);

        vertexes.clear();
        vertexes.append(box.leftUp().x).append(box.leftUp().y).append(box.leftUp().z);
        vertexes.append(box.rightUp().x).append(box.rightUp().y).append(box.rightUp().z);
        vertexes.append(box.leftDown().x).append(box.leftDown().y).append(box.leftDown().z);
        vertexes.append(box.rightDown().x).append(box.rightDown().y).append(box.rightDown().z);

        glVertexPointer(3, GL_FLOAT, 0, vertexes());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableClientState(GL_VERTEX_ARRAY);
    }

    switch(flags&VER_MASK)
    {
    case VER_UP:
        hline=hstep=scale*size.y/h/count;
        hstart=0.0;
        break;
    case VER_DOWN:
        hline=hstep=scale*size.y/h/count;
        hstart=1.0-scale*size.y/h;
        break;
    case VER_CENTER:
        hstep=hline=scale*size.y/h/count;
        hstart=(1.0-scale*size.y/h)/2.0;
        break;
    default:
        hstep=1.0/count;
        hline=scale*size.y/h/count;
        hstart=(hstep-hline)/2.0;
        break;
    };

    real xscale=1.0;
    if(w>size.x*scale)xscale=size.x*scale/w;

    for(int i=0;i<lines.size();i++)
    {
        alt::vec2d<real32> lsiz=printSize(lines[i])*txscale;

        alt::vec3d<real> lu=(hstart+hstep*i)*(box.leftDown()-box.leftUp())+box.leftUp();
        alt::vec3d<real> ld=(hstart+hstep*i+hline)*(box.leftDown()-box.leftUp())+box.leftUp();

        alt::vec3d<real> ru=(hstart+hstep*i)*(box.rightDown()-box.rightUp())+box.rightUp();
        alt::vec3d<real> rd=(hstart+hstep*i+hline)*(box.rightDown()-box.rightUp())+box.rightUp();

        switch(flags&HOR_MASK)
        {
        case HOR_LEFT:
            printInQuad(lines[i],
                        lu,
                        lu+(ru-lu)*(xscale*lsiz.x/size.x),
                        ld+(rd-ld)*(xscale*lsiz.x/size.x),
                        ld);
            break;
        case HOR_RIGHT:
            printInQuad(lines[i],
                        lu+(ru-lu)*(1.0-xscale*lsiz.x/size.x),
                        lu+(ru-lu),
                        ld+(rd-ld),
                        ld+(rd-ld)*(1.0-xscale*lsiz.x/size.x)
                        );
            break;
        case HOR_CENTER:
            printInQuad(lines[i],
                        lu+(ru-lu)*(0.5*(1.0-xscale*lsiz.x/size.x)),
                        lu+(ru-lu)*(xscale*lsiz.x/size.x+0.5*(1.0-xscale*lsiz.x/size.x)),
                        ld+(rd-ld)*(xscale*lsiz.x/size.x+0.5*(1.0-xscale*lsiz.x/size.x)),
                        ld+(rd-ld)*(0.5*(1.0-xscale*lsiz.x/size.x))
                                    );
            break;
        default:
            if(!format[i])
            {
                printInQuad(lines[i],
                            lu,
                            lu+(ru-lu)*(xscale*lsiz.x/size.x),
                            ld+(rd-ld)*(xscale*lsiz.x/size.x),
                            ld);
            }
            else
            {
                alt::array<alt::string> words=lines[i].split(' ',true);
                alt::vec2d<real32> spsiz=printSize(" ")*txscale;
                real xstart=0.0,xleft=1.0-xscale*lsiz.x/size.x+xscale*spsiz.x/size.x*(words.size()-1);
                for(int j=0;j<words.size();j++)
                {
                    alt::vec2d<real32> wsiz=printSize(words[j])*txscale;

                    printInQuad(words[j],
                                lu+(ru-lu)*(xstart),
                                lu+(ru-lu)*(xstart+xscale*wsiz.x/size.x),
                                ld+(rd-ld)*(xstart+xscale*wsiz.x/size.x),
                                ld+(rd-ld)*(xstart)
                                            );
                    xstart+=xscale*wsiz.x/size.x+xleft/(words.size()-1-j);
                    xleft-=xleft/(words.size()-1-j);
                }
            }
            break;
        };
    }

}

alt::vec2d<real32> AGLFont::printInWidth(const alt::string &val, real32 width, real32 fontSize,
                             bool onlyCalculate, alt::vec2d<real32> pnt, int flags)
{    
    alt::vec2d<real32> space_size=printSize(" ");
    real32 scale=fontSize/space_size.y, maxwidth=0.0;
    space_size*=scale;

    real32 rv=0.0, tab=((flags&TF_PARAGRAF) && !(flags&TF_NOTAB))
            ?((printSize("  ")*scale).x):0.0;

    if(val.isEmpty())return alt::vec2d<real32>(0,0);
    alt::array<alt::string> lines=val.split('\n');
    for(int i=0;i<lines.size();i++)
    {        
        alt::array<int> line_count;
        alt::array<real32> line_size;
        alt::array<alt::string> paragraf=lines[i].simplified().split(' ');
        line_size.append(tab);
        line_count.append(0);
        for(int j=0;j<paragraf.size();j++)
        {
            real32 ww=(printSize(paragraf[j])*scale).x;
            if(ww>width) //попытаемся разбить
            {
                int from = width/(ww/paragraf[j].size());
                for(;from>0;from--)
                {
                    if(((uint8)paragraf[j][from])<=127 &&
                              !(   (paragraf[j][from]>='a' && paragraf[j][from]<='z')
                                || (paragraf[j][from]>='A' && paragraf[j][from]<='Z')
                                || (paragraf[j][from]>='0' && paragraf[j][from]<='9')))
                    {
                        break;
                    }
                }
                if(from<=0)from = width/(ww/paragraf[j].size());
                if(from>0)
                {
                    paragraf.insert(j+1,paragraf[j].right(from));
                    paragraf[j] = paragraf[j].left(from);
                    ww=(printSize(paragraf[j])*scale).x;
                }
            }
            if(ww+line_size.last()+space_size.x>width && line_count.last())
            {
                line_size.append(0.0);
                line_count.append(0);
            }
            line_size.last()+=ww;
            if(line_count.last())line_size.last()+=space_size.x;
            line_count.last()++;            
        }

        if(line_size.size()>1)maxwidth=width;
        else if(maxwidth<line_size[0])maxwidth=line_size[0];

        if(!onlyCalculate)
        {
            int k=0;
            for(int i=0;i<line_count.size();i++)
            {
                real32 space=space_size.x;
                if(line_count[i]>1 && i!=line_count.size()-1 &&
                        (flags&TF_PARAGRAF))
                    space+=(width-line_size[i])/(line_count[i]-1);

                alt::vec2d<real32> pos=pnt+alt::vec2d<real32>(0.0,i*space_size.y+rv);
                if(!i)pos.x+=tab;
                if(flags&TF_CENTER)pos.x+=(width-line_size[i])/2.0;
                for(int j=0;j<line_count[i];j++,k++)
                {
                    pos.x+=print(paragraf[k],pos,scale);
                    pos.x+=space;
                }
            }
        }
        rv+=line_count.size()*space_size.y;
    }
    if(maxwidth>width)maxwidth=width;
    return alt::vec2d<real32>(maxwidth,rv);
}

void AGLFont::printInQuad(const alt::string &val,
                 alt::vec3d<real32> lt, alt::vec3d<real32> rt,
                 alt::vec3d<real32> rd, alt::vec3d<real32> ld, alt::colorRGBA bg)
{
    if(val.isEmpty())return;
    alt::vec2d<real32> size=printSize(val);
    int tind=-1;

    uint32 uval=0;
    val.unicode_at(0,uval);
    uint32 index=uval>>8;
    aglFontBlock *block;

    //создаем символьные блоки, если нужно
    if(!fonts.contains(curr_font))createBlock(index);
    if(!fonts[curr_font].contains(index))createBlock(index);
    block=&fonts[curr_font][index];
    cache.Update(block->cacheIndex);

    //разрешим прозрачность
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    //отрисуем бэкграунд
    if(bg())
    {
        glColor4f(bg.rf(),bg.gf(),
                  bg.bf(),bg.af());
        glDisable(GL_TEXTURE_2D);

        glEnableClientState(GL_VERTEX_ARRAY);

        vertexes.clear();
        vertexes.append(lt.x).append(lt.y).append(lt.z);
        vertexes.append(rt.x).append(rt.y).append(rt.z);
        vertexes.append(ld.x).append(ld.y).append(ld.z);
        vertexes.append(rd.x).append(rd.y).append(rd.z);

        glVertexPointer(3, GL_FLOAT, 0, vertexes());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableClientState(GL_VERTEX_ARRAY);
    }

    //настраиваем параметры отображения
    glColor4f(curr_color.rf(),curr_color.gf(),
              curr_color.bf(),curr_color.af());
    glEnable(GL_TEXTURE_2D);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    vertexes.clear();
    coords.clear();

    real32 xoff=0.0,yoff=0.0;
    for(int i=0;i<val.size();)
    {
        i+=val.unicode_at(i,uval);
        //формирование символьных блоков
        if((uval>>8)!=index)
        {
            index=(uval>>8);

            if(coords.size())
            {
                glVertexPointer(3, GL_FLOAT, 0, vertexes());
                glTexCoordPointer(2, GL_FLOAT, 0, coords());
                glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
            }

            if(!fonts.contains(curr_font))createBlock(index);
            if(!fonts[curr_font].contains(index))createBlock(index);
            block=&fonts[curr_font][index];
            cache.Update(block->cacheIndex);
            tind=-1;

            vertexes.clear();
            coords.clear();
        }

        //учет строк
        if(uval==uint8('\r'))continue;
        if(uval==uint8('\n'))
        {
            xoff=0.0;
            yoff+=block->height;
            continue;
        }

        //выбор текстуры
        int ind=uval&0xff;
        if(tind!=block->glyphs[ind].tindex)
        { //активируем текстуру
            tind=block->glyphs[ind].tindex;

            if(coords.size())
            {
                glVertexPointer(3, GL_FLOAT, 0, vertexes());
                glTexCoordPointer(2, GL_FLOAT, 0, coords());
                glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
            }

            glBindTexture(GL_TEXTURE_2D, block->textures[tind]->getID());

            vertexes.clear();
            coords.clear();
        }

        //рисуем символ
        aglFontGlyphInfo *inf=&block->glyphs[ind];
        real32 cwidth=inf->w*256.0;

        real32 yk=yoff/size.y;
        real32 ykk=(yoff+block->height)/size.y;
        real32 xk=xoff/size.x;
        real32 xkk=(xoff+cwidth)/size.x;

        alt::vec3d<real32> tmpr=(rt+(rd-rt)*yk);
        alt::vec3d<real32> tmpl=(lt+(ld-lt)*yk);
        alt::vec3d<real32> p_lt=(tmpr-tmpl)*xk+tmpl;
        alt::vec3d<real32> p_rt=(tmpr-tmpl)*xkk+tmpl;
        tmpr=(rt+(rd-rt)*ykk);
        tmpl=(lt+(ld-lt)*ykk);
        alt::vec3d<real32> p_ld=(tmpr-tmpl)*xk+tmpl;
        alt::vec3d<real32> p_rd=(tmpr-tmpl)*xkk+tmpl;

        coords.append(inf->x).append(inf->y);
        coords.append(inf->x+inf->w).append(inf->y);
        coords.append(inf->x+inf->w).append(inf->y+inf->h);
        coords.append(inf->x+inf->w).append(inf->y+inf->h);
        coords.append(inf->x).append(inf->y+inf->h);
        coords.append(inf->x).append(inf->y);

        vertexes.append(p_lt.x).append(p_lt.y).append(p_lt.z);
        vertexes.append(p_rt.x).append(p_rt.y).append(p_rt.z);
        vertexes.append(p_rd.x).append(p_rd.y).append(p_rd.z);
        vertexes.append(p_rd.x).append(p_rd.y).append(p_rd.z);
        vertexes.append(p_ld.x).append(p_ld.y).append(p_ld.z);
        vertexes.append(p_lt.x).append(p_lt.y).append(p_lt.z);

        //корректируем позицию
        real32 incsz=(block->spacing_is_abs?(block->spacing):(cwidth+block->spacing));
        if(uval==uint8('\t'))
        {
            int cnt=xoff/incsz;
            xoff=(cnt+1)*incsz;
        }
        else
        {
            xoff+=incsz;
        }
    }

    if(coords.size())
    {
        glVertexPointer(3, GL_FLOAT, 0, vertexes());
        glTexCoordPointer(2, GL_FLOAT, 0, coords());
        glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

real32 AGLFont::print(const alt::string &val, const alt::vec2d<real32> &pnt, real32 scale)
{
    if(val.isEmpty())return 0.0;
    uint32 uval=0;
    val.unicode_at(0,uval);
    uint32 index=uval>>8;
    int tind=-1;
    aglFontBlock *block;

    if(!fonts.contains(curr_font))createBlock(index);
    if(!fonts[curr_font].contains(index))createBlock(index);

    block=&fonts[curr_font][index];
    cache.Update(block->cacheIndex);

    glColor4f(curr_color.rf(),curr_color.gf(),
              curr_color.bf(),curr_color.af());

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    vertexes.clear();
    coords.clear();

    real32 xoff=0.0,yoff=0.0;
    for(int i=0;i<val.size();)
    {
        i+=val.unicode_at(i,uval);
        if((uval>>8)!=index)
        {
            index=(uval>>8);
            if(coords.size())
            {
                glVertexPointer(2, GL_FLOAT, 0, vertexes());
                glTexCoordPointer(2, GL_FLOAT, 0, coords());
                glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
            }
            if(!fonts.contains(curr_font))createBlock(index);
            if(!fonts[curr_font].contains(index))createBlock(index);
            block=&fonts[curr_font][index];
            cache.Update(block->cacheIndex);
            tind=-1;
            vertexes.clear();
            coords.clear();
        }
        if(uval==uint8('\r'))continue;
        if(uval==uint8('\n'))
        {
            xoff=0.0;
            yoff+=block->height*scale;
            continue;
        }
        ///////////////////////////////////////////////////////////////
        //рисуем букву
        int ind=uval&0xff;
        if(tind!=block->glyphs[ind].tindex)
        { //активируем текстуру
            tind=block->glyphs[ind].tindex;
            if(coords.size())
            {
                glVertexPointer(2, GL_FLOAT, 0, vertexes());
                glTexCoordPointer(2, GL_FLOAT, 0, coords());
                glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
            }
            glBindTexture(GL_TEXTURE_2D, block->textures[tind]->getID());
            vertexes.clear();
            coords.clear();
        }
        aglFontGlyphInfo *inf=&block->glyphs[ind];
        real32 cwidth=(inf->w*256.0)*scale;

        coords.append(inf->x).append(inf->y);
        coords.append(inf->x+inf->w).append(inf->y);
        coords.append(inf->x+inf->w).append(inf->y+inf->h);
        coords.append(inf->x+inf->w).append(inf->y+inf->h);
        coords.append(inf->x).append(inf->y+inf->h);
        coords.append(inf->x).append(inf->y);

        vertexes.append(xoff+pnt.x).append(yoff+pnt.y);
        vertexes.append(xoff+cwidth+pnt.x).append(yoff+pnt.y);
        vertexes.append(xoff+cwidth+pnt.x).append(yoff+block->height*scale+pnt.y);
        vertexes.append(xoff+cwidth+pnt.x).append(yoff+block->height*scale+pnt.y);
        vertexes.append(xoff+pnt.x).append(yoff+block->height*scale+pnt.y);
        vertexes.append(xoff+pnt.x).append(yoff+pnt.y);

        real32 incsz=(block->spacing_is_abs?(block->spacing*scale):(cwidth+block->spacing*scale));
        if(uval==uint8('\t'))
        {
            int cnt=xoff/incsz;
            xoff=(cnt+1)*incsz;
        }
        else
        {
            xoff+=incsz;
        }
    }

    if(coords.size())
    {
        glVertexPointer(2, GL_FLOAT, 0, vertexes());
        glTexCoordPointer(2, GL_FLOAT, 0, coords());
        glDrawArrays(GL_TRIANGLES, 0, coords.size()>>1);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    return xoff;
}

void AGLFont::checkBlocksLifeCicle()
{
    while(cache.Size()>blocklimit)
    {
        alt::pair<alt::string,int> el=cache.ForRemove();
        cache.Delete(fonts[el.left()][el.right()].cacheIndex);

        alt::array<alt::GLTexture*> texs=fonts[el.left()][el.right()].textures;
        for(int k=0;k<texs.size();k++)
        {
            delete texs[k];
        }
        fonts[el.left()].remove(el.right());
        if(!fonts[el.left()].size())
        {
            fonts.remove(el.left());
        }
    }
}

void AGLFont::createBlock(int index)
{
    onStartBlock();

    //блок
    aglFontBlock block;
    checkBlocksLifeCicle();
    block.height=0;
    block.spacing=getSpacing();
    block.spacing_is_abs=spacingAbs();

    //контроль объемов
    int curr_yp=1;
    int curr_xp=1;
    real32 str_height=1;

    //создадим полотно
    alt::image img(256,256);
    img.fill(0);

    for(int i=0;i<256;i++)
    {
        alt::image glph = createGlyph(index*256+i);
        if(glph.width()+curr_xp+1>256) //строка заполнена?
        {
            curr_yp+=str_height+1;
            curr_xp=1;
            str_height=1;
        }
        if(glph.height()+curr_yp+1>256) //текстура переполнена?
        {
            if(str_height>block.height)block.height=str_height;

            curr_yp=1;
            curr_xp=1;
            str_height=1;

            //генерим текстуру
            block.textures.append(new alt::GLTexture(img(),img.width(),img.height(),0x8888));

            img.fill(0);
        }

        img.insert(glph,curr_xp,curr_yp);

        //запишем глиф
        aglFontGlyphInfo inf;
        inf.tindex=block.textures.size();
        inf.w=glph.width()/256.0;
        inf.h=glph.height()/256.0;
        inf.x=curr_xp/256.0;
        inf.y=curr_yp/256.0;
        block.glyphs.append(inf);

        //скорректируем направляющие
        curr_xp+=glph.width()+1;
        if(glph.height()>str_height)str_height=glph.height();
        if(str_height>block.height)block.height=str_height;
    }

    block.textures.append(new alt::GLTexture(img(),img.width(),img.height(),0x8888));

    //запишем блок
    if(str_height>block.height)block.height=str_height;
    block.cacheIndex=cache.Push(alt::pair<alt::string,int>(curr_font,index));
    fonts[curr_font][index]=block;

    onEndBlock();
}
