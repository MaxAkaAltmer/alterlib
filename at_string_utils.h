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

#ifndef AT_STRING_UTILS_HEADER_DEFINITION
#define AT_STRING_UTILS_HEADER_DEFINITION  "A... Template String Utils v.1.0.7"

#define ATS_MINSIZE (sizeof(int)*8)
#define _ats_upsize(x) ( ((x+1)+((x)>>1))<ATS_MINSIZE ? ATS_MINSIZE : ((x+1)+((x)>>1)) )

template <class T>
unsigned int _ats_strlen(const T *Str)
{
    if(!Str)return 0;

    unsigned int i=0;
    while(Str[i]){i++;};
    return i;
}

template <class T>
void _ats_memcpy(T *dst, const T *src,int num)
{
    int i;
    if(dst<src)  //в случае перекрытий следует копировать с нужной стороны
    {
            for(i=0;i<num;i++)dst[i]=src[i];
    }
    else if(dst>src)
    {
            for(i=num-1;i>=0;i--)dst[i]=src[i];
    }
}

template <class T>
int _ats_memcmp(const T *dst, const T *src,int num)
{
    int i;
    for(i=0;i<num;i++)
    {
            if(dst[i]<src[i])return -1;
            if(dst[i]>src[i])return 1;
    }
    return 0;
}

template <class T>
int _ats_strcmp(const T *dst, const T *src)
{
    int i=0;
    while(dst[i] && src[i])
    {
            if(dst[i]<src[i])return -1;
            if(dst[i]>src[i])return 1;
            i++;
    }
    if(dst[i]<src[i])return -1;
    if(dst[i]>src[i])return 1;
    return 0;
}

template <class T>
void _ats_memset(T *dst, T c, int num)
{
    for(int i=0;i<num;i++)dst[i]=c;
}


#endif

