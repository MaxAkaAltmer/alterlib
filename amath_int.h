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

#ifndef INTMATH_UTILS_HEADER_DEFINITION
#define INTMATH_UTILS_HEADER_DEFINITION

#include "atypes.h"

namespace alt {
namespace imath {

    __inline uint8 rev8(uint8 x)
    {
     x = (((x & 0xaa) >> 1) | ((x << 1) & 0xaa));
     x = (((x & 0xcc) >> 2) | ((x << 2) & 0xcc));
     x = ((x >> 4) | (x << 4));
     return x;
    }

    __inline uint32 rev32(uint32 x)
    {
     x = (((x & 0xaaaaaaaa) >> 1) | ((x << 1) & 0xaaaaaaaa));
     x = (((x & 0xcccccccc) >> 2) | ((x << 2) & 0xcccccccc));
     x = (((x & 0xf0f0f0f0) >> 4) | ((x << 4) & 0xf0f0f0f0));
     x = (((x & 0xff00ff00) >> 8) | ((x << 8) & 0xff00ff00));
     x = ((x >> 16) | (x << 16));
     return x;
    }

    template <class T>
    __inline T abs(T val)
    {
            if(val<0)return -val;
            return val;
    }

    template <class T>
    __inline T exign(T val, int byte_count)
    {
        if( val&(T(1)<<(8*byte_count-1)) )
        {
            val|=(~(T(0)))<<(8*byte_count-1);
        }
        return val;
    }

    template <class T>
    __inline void swap(T &v1, T &v2)
    {
        T tmp=v1;
        v1=v2;
        v2=tmp;
    }

    template <class T>
    __inline T max(const T &v1, const T &v2)
    {
        if(v1>v2)return v1;
        return v2;
    }

    template <class T>
    __inline T min(const T &v1, const T &v2)
    {
        if(v1<v2)return v1;
        return v2;
    }

    template <class I, class R>
    __inline I round(R val)
    {
        return val >= R(0.0) ? I(val + R(0.5))
                             : I(val - I(val-1) + R(0.5)) + I(val-1);
    }

    __inline uint32 bsr32(uint32 num)  //число бит необходимых на значение
    {
     uint32 retval;

            if(!num)return 1;
            //можно оптимизировать командой x86 - bsr
            if(num>>16){num>>=16;retval=16;}
            else retval=0;
            if(num>>8){num>>=8;retval+=8;}
            if(num>>4){num>>=4;retval+=4;}
            if(num>>2){num>>=2;retval+=2;}
            if(num>>1){num>>=1;retval+=2;}
            else if(num)retval++;

            return retval;
    }

    __inline uint32 bsr64(uint64 val)
    {
     uint32 retval;
            if(val>>32)
            {
                    retval=bsr32(val>>32);
                    val >>= 32;
                    retval += 32;
            }
            else retval=bsr32(uint32(val));

            return retval;
    }

    __inline uint32 max_div_p2(uint32 val)
    {
        uint32 rv = 0;
        if(!val) return 1;
        if(!(val&0xffff)) { rv += 16; val >>= 16; }
        if(!(val&0xff)) { rv += 8; val >>= 8; }
        if(!(val&0xf)) { rv += 4; val >>= 4; }
        if(!(val&0x3)) { rv += 2; val >>= 2; }
        if(!(val&0x1)) { rv += 1; }
        return uint32(1) << rv;
    }

    __inline uint64 max_div_p2(uint64 val)
    {
        uint64 rv = 0;
        if(!val) return 1;
        if(!(val&0xffffffff)) { rv += 32; val >>= 32; }
        if(!(val&0xffff)) { rv += 16; val >>= 16; }
        if(!(val&0xff)) { rv += 8; val >>= 8; }
        if(!(val&0xf)) { rv += 4; val >>= 4; }
        if(!(val&0x3)) { rv += 2; val >>= 2; }
        if(!(val&0x1)) { rv += 1; }
        return uint64(1) << rv;
    }

    template <class T>
    uint32 bsrT(T val)
    {
            return bsr64(uint64(val));
    }

    __inline uint32 hbc32(uint32 x)  //подсчет единичных бит
    {
            x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
            x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
            x = (x & 0xffff) + (x >> 16);
            x = (x & 0xf0f) + ((x >> 4) & 0xf0f);
            return (x&0xff)+ (x>>8);
    }

    __inline uint16 bswap16(uint16 x)
    {
        return uint16((x>>8) | (x<<8));
    }

    __inline uint32 bswap32by16(uint32 x)
    {
        return (x>>16) | (x<<16);
    }

    __inline uint64 bswap64by16(uint64 x)
    {
        return (x>>48) | ((x>>16)&0xffff0000) | ((x&0xffff0000)<<16) | (x<<48);
    }

    __inline uint32 bswap32(uint32 x)
    {
        return (x>>24) | ((x>>8)&0x0000FF00L) | ((x&0x0000FF00L)<<8) | (x<<24);
    }

    __inline int32 bswap32(int32 x)
    {
        return int32(bswap32(uint32(x)));
    }

    __inline uint64 bswap64(uint64 x)
    {
        return (x>>56) | ((x>>40)&0x0000FF00) | ((x>>24)&0x00FF0000) | ((x>>8)&0xFF000000) |
                ((x&0xFF000000)<<8) | ((x&0x00FF0000)<<24) | ((x&0x0000FF00)<<40) | (x<<56);
    }

    __inline uint32 rotr32(uint32 val, uint32 shift)
    {
        //на большинстве процессоров можно оптимизировать
        if(!shift)return val;  //бывают тупые компиляторы в которых сдвиг на ноль непредсказуем
        return (val>>shift)|(val<<(32-shift));
    }

    template <class T>
    __inline T rotr(T val, T shift)
    {
        shift&=(sizeof(T)*8-1);
        if(!shift)return val;
        return (val>>shift)|(val<<(sizeof(T)*8-shift));
    }

    template <class T>
    __inline T rotl(T val, T shift)
    {
        shift&=(sizeof(T)*8-1);
        if(!shift)return val;
        return (val<<shift)|(val>>(sizeof(T)*8-shift));
    }

    __inline uint32 sqrt32( uint32 val )
    {
        uint32 tmp, mask, root=0;

        if(!val)return root;
        //считаем необходимое число циклов
        //можно оптимизировать командой x86 - bsr
        mask=val;
        if(mask>>16){mask>>=16;tmp=16;}
        else tmp=0;
        if(mask>>8){mask>>=8;tmp+=8;}
        if(mask>>4){mask>>=4;tmp+=4;}
        if(mask>>2)tmp+=2;
        mask=1<<tmp;

        do   //цикл вычисления корня
        {
                tmp=root|mask;
                root>>=1;
                if(tmp<=val)
                {
                        val-=tmp;
                        root|=mask;
                }
                mask>>=2;
        }while(mask);

        if(root<val)root++;   //округление до ближайшего целого

        return root;
    }

    __inline int32 ln32(uint32 val, uint32 rezfix)
    {
        uint32 x, cnt;
        uint64 tmp, curr,dop;

        if(!val)return 0x80000000;
        if(val==1)return 0;
        //далее вычисления по ряду ln((1+x)/(1-x))=2*(x+x^3/3+x^5/5+x^7/7+....)
        //до тех пор пока x^n/n!=0
        //x=(val-1)/(val+1)
        rezfix++;
        x=((val-1)<<rezfix)/(val+1);
        curr=tmp=x;
        tmp*=tmp;
        tmp>>=rezfix;

        cnt=1;
        do
        {
                cnt+=2;
                curr*=tmp;
                dop=(curr/cnt)>>rezfix;
                x+=dop;
                curr>>=rezfix;
        }
        while(dop);

        return x;
    }

    template <class T>
    __inline T pow(T x, T p)
    {
        if (p < 0)
            return 1/pow(x,-p);

        if (p == 0) return 1;
        if (p == 1) return x;

        T tmp = pow(x, p/2);

        if (p%2 == 0) return tmp * tmp;
        else return x * tmp * tmp;
    }

} //namespace imath
} //namespace alt


#endif
