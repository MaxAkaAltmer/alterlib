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

#ifndef AIMAGE_H
#define AIMAGE_H

#include "atypes.h"
#include "abyte_array.h"

namespace alt {

    class image
    {
    private:

        struct Internal
        {
            int refcount;
            int w,h,d;
            uint32 buff[1];
        };

        Internal *data;

        Internal *CreateInternal(int w, int h, int d, const uint32 *buff);
        void DeleteInternal();
        void CloneInternal();

        float get_subpixel(int d, int y, int x, int ic);

    public:
        image();
        image(const image &img);
        image(int w, int h, const void *buff = nullptr, int d = 1);
        ~image();

        void deepCopy(const image &img)
        {
            if(!img.data)return;
            *this = image(img.data->w,img.data->h,img.data->buff,img.data->d);
        }

        bool save(const string &fname);

        byteArray toData(int compression=-1, bool just_alpha=false);
        static image fromData(byteArray &val);

        image& operator=(const image &img);

        bool operator==(const image &img)
        {
            if(!data && !img.data)return true;
            if(!data || !img.data)return false;
            if(data->d!=img.data->d)return false;
            if(data->w!=img.data->w)return false;
            if(data->h!=img.data->h)return false;
            for(int i=0;i<data->w*data->h*data->d;i++)
            {
                if(data->buff[i]!=img.data->buff[i])return false;
            }
            return true;
        }

        uint32 *operator()()
        {
            CloneInternal();
            if(!data)return NULL;
            return data->buff;
        }

        uint32& operator[](int ind)
        {
            return data->buff[ind];
        }

        uint32 at(int ind) const
        {
            if(!data)return 0;
            return data->buff[ind];
        }

        uint32 at(int x, int y, int l=0) const
        {
            if(!data)return 0;
            return data->buff[x+y*data->w+l*data->h*data->w];
        }

        int countNonTransp() const;

        static image xorResult(const image& v1, const image& v2);

        bool isValid() const {return data?true:false;}

        void fill(uint32 val)
        {
            if(data)
            {
                for(int i=0;i<data->w*data->h*data->d;i++)
                    data->buff[i]=val;
            }
        }

        void swapRGB()
        {
            if(data)
            {
                for(int i=0;i<data->w*data->h*data->d;i++)
                {
                    uint32 col=data->buff[i]&0xff00ff00;
                    col|=(data->buff[i]>>16)&0xff;
                    col|=(data->buff[i]<<16)&0xff0000;
                    data->buff[i]=col;
                }
            }
        }

        void makeGreyScale(uint8 *buff)
        {
            if(data)
            {
                for(int i=0;i<data->w*data->h*data->d;i++)
                {
                    int gray=((data->buff[i]&0xff)+
                            ((data->buff[i]>>8)&0xff)+
                            ((data->buff[i]>>16)&0xff))/3;
                    buff[i]=gray;
                }
            }
        }

        void makeGreyScale()
        {
            if(data)
            {
                for(int i=0;i<data->w*data->h*data->d;i++)
                {
                    int gray=((data->buff[i]&0xff)+
                            ((data->buff[i]>>8)&0xff)+
                            ((data->buff[i]>>16)&0xff))/3;
                    uint32 col=data->buff[i]&0xff000000;
                    col|=gray|(gray<<8)|(gray<<16);
                    data->buff[i]=col;
                }
            }
        }

        void insert(const image &img, int x, int y, int z=0);

        const uint32* operator()() const
        {
            if(!data)return NULL;
            return data->buff;
        }

        int width() const
        {
            if(!data)return 0;
            return data->w;
        }

        int height() const
        {
            if(!data)return 0;
            return data->h;
        }

        int depth() const
        {
            if(!data)return 0;
            return data->d;
        }

        void setPixel(uint32 val, int x, int y, int z=0)
        {
            CloneInternal();
            if(!data)return;
            if(x>=data->w || y>=data->h || z>=data->d)return;
            data->buff[z*data->h*data->w+y*data->w+x]=val;
        }

        image resize(int w, int h);

    };

    uint32 aHash(const alt::image &val);

} //namespace alt

#endif // AIMAGE_H
