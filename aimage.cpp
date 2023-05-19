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

#include "aimage.h"
#include "compress/arch_diction.h"
#include "compress/arch_prefix.h"

using namespace alt;

image::image()
{
    data=NULL;
}

image::Internal* image::CreateInternal(int w, int h, int d, const uint32 *buff)
{
    Internal *dat = (Internal*) new uint8[sizeof(Internal)+w*h*d*sizeof(uint32)];
    dat->refcount=1;
    dat->w=w;
    dat->h=h;
    dat->d=d;
    if(buff)utils::memcpy(dat->buff,buff,w*h*d);
    else utils::memset<uint32>(dat->buff,0,w*h*d);
    return dat;
}

image::image(const image &img)
{
    data=img.data;
    if(data)data->refcount++;
}

image::image(int w, int h, const void *buff, int d)
{
    data=CreateInternal(w,h,d,(const uint32*)buff);
}

image::~image()
{
    DeleteInternal();
}

void image::DeleteInternal()
{
    if(data)
    {
        data->refcount--;
        if(!data->refcount)
        {
            delete []((uint8*)data);
        }
        data=NULL;
    }
}

image& image::operator=(const image &img)
{
    if(data==img.data)return *this;
    DeleteInternal();
    data=img.data;
    if(data)data->refcount++;
    return *this;
}

int image::countNonTransp() const
{
    int rv=0;
    for(int i=0;i<data->w*data->h*data->d;i++)
        if(data->buff[i]>>24)rv++;
    return rv;
}

image image::xorResult(const image& v1, const image& v2)
{
    if(!v1.isValid() || !v2.isValid())return image();

    int w=imath::min(v1.data->w,v2.data->w);
    int h=imath::min(v1.data->h,v2.data->h);
    int d=imath::min(v1.data->d,v2.data->d);

    image rv(w,h,NULL,d);

    for(int k=0;k<d;k++)
    {
        for(int i=0;i<h;i++)
        {
            for(int j=0;j<w;j++)
            {
                rv.data->buff[k*w*h+i*w+j]=v1.at(j,i,k)^v2.at(j,i,k);
            }
        }
    }
    return rv;
}

byteArray image::toData(int compression, bool just_alpha)
{
    byteArray rv;
    if(!data)return rv;
    uint32 tmp=data->w;
    _pref_p1qX_encoder(tmp,7,rv);
    tmp=data->h;
    _pref_p1qX_encoder(tmp,7,rv);
    tmp=data->d;
    _pref_p1qX_encoder(tmp,7,rv);
    byteArray im;
    if(just_alpha)
    {
        uint8 *buff=new uint8[data->w*data->h*data->d];
        for(int i=0;i<data->w*data->h*data->d;i++)buff[i]=data->buff[i]>>24;
        _arch_diction_encode(im,buff,data->w*data->h*data->d,compression);
        delete []buff;
    }
    else
    {
        _arch_diction_encode(im,(uint8*)data->buff,data->w*data->h*data->d*4,compression);
    }
    rv.append(im);
    return rv;
}

image image::fromData(byteArray &val)
{
    uint32 w;
    int cnt=_pref_p1q7_decoder(&w,val(),val.size());
    if(cnt<0)return image();
    uint32 h;
    int n=_pref_p1q7_decoder(&h,val()+cnt,val.size()-cnt);
    if(n<0)return image();
    cnt+=n;
    uint32 d;
    n=_pref_p1q7_decoder(&d,val()+cnt,val.size()-cnt);
    if(n<0)return image();
    cnt+=n;
    byteArray im;
    _arch_diction_decode(im,val()+cnt,val.size()-cnt);
    if((uint32)im.size()!=w*h*d*4)return image();
    return image(w,h,im(),d);
}

void image::CloneInternal()
{
    if(!data)return;
    if(data->refcount==1)return;
    Internal *dat=CreateInternal(data->w,data->h,data->d,data->buff);
    DeleteInternal();
    data=dat;
}

void image::insert(const image &img, int x, int y, int z)
{
    if(!data)return;
    if(!img.isValid())return;

    for(int k=0;k<img.depth();k++)
    {
        int pz=z+k;
        if(pz<0) continue;
        if(pz>=data->d) break;
        for(int i=0;i<img.height();i++)
        {
            int py=y+i;
            if(py<0) continue;
            if(py>=data->h) break;
            for(int j=0;j<img.width();j++)
            {
                int px=x+j;
                if(px<0) continue;
                if(px>=data->w) break;
                data->buff[pz*data->w*data->h+py*data->w+px]=img()[k*img.width()*img.height()+i*img.width()+j];
            }
        }
    }
}

inline uint8 _aimg_saturate( float x )
{
    int rv=x;
    if(rv>255)return 255;
    if(rv<0)return 0;
    return rv;
}

float image::get_subpixel(int d, int y, int x, int ic)
{
    if(x>=data->w)x=data->w-1;
    if(y>=data->h)y=data->h-1;
    if(x<0)x=0;
    if(y<0)y=0;
    int ind=d*data->w*data->h*4+y*data->w*4+x*4+ic;
    if(ind>=data->w*data->h*data->d*4)return 0;
    if(ind<0)return 0;
    return ((uint8*)data->buff)[d*data->w*data->h*4+y*data->w*4+x*4+ic];
}

image image::resize(int w, int h)
{
    if(!data)return image();
    image rv(w,h,NULL,data->d);

    uint8 *out=(uint8*)rv.data->buff;

    int channels = 4;

    const double tx = double(width()) / w;
    const double ty = double(height()) / h;
    int row_stride = w * channels;

    float C[5] = { 0 };

    for (int d = 0; d < data->d; ++d)
    {
        for (int i = 0; i < h; ++i)
        {
            for (int j = 0; j < w; ++j)
            {
                const float x = float(tx * j);
                const float y = float(ty * i);
                const float dx = tx * j - x, dx2 = dx * dx, dx3 = dx2 * dx;
                const float dy = ty * i - y, dy2 = dy * dy, dy3 = dy2 * dy;

                for (int k = 0; k < channels; ++k)
                {
                    for (int jj = 0; jj < 4; ++jj)
                    {
                        const int idx = y - 1 + jj;
                        float a0 = get_subpixel(d,idx, x, k);
                        float d0 = get_subpixel(d,idx, x - 1, k) - a0;
                        float d2 = get_subpixel(d,idx, x + 1, k) - a0;
                        float d3 = get_subpixel(d,idx, x + 2, k) - a0;
                        float a1 = -(1.0f / 3.0f) * d0 + d2 - (1.0f / 6.0f) * d3;
                        float a2 =          0.5f  * d0 +              0.5f *  d2;
                        float a3 = -(1.0f / 6.0f) * d0 - 0.5f * d2 + (1.0f / 6.0f) * d3;
                        C[jj] = a0 + a1 * dx + a2 * dx2 + a3 * dx3;

                        d0 = C[0] - C[1];
                        d2 = C[2] - C[1];
                        d3 = C[3] - C[1];
                        a0 = C[1];
                        a1 = -(1.0f / 3.0f) * d0 + d2 -(1.0f / 6.0f) * d3;
                        a2 =          0.5f  * d0 +             0.5f  * d2;
                        a3 = -(1.0f / 6.0f) * d0 - 0.5f * d2 + (1.0f / 6.0f) * d3;
                        out[d*row_stride*h + i * row_stride + j * channels + k] = _aimg_saturate( a0 + a1 * dy + a2 * dy2 + a3 * dy3 );
                    }
                }
            }
        }
    }

    return rv;
}

uint32 alt::aHash(const image &val)
{
    uint32 rv=val.depth()^val.height()^val.width();
    for(int i=0;i<val.depth()*val.height()*val.width();i++)
    {
        rv^=val.at(i);
    }
    return rv;
}


