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

#include "arch_ppm.h"

#define MAXSCALE 16383

PPM::PPM()
{
    Reset(PPM_MODEL_SIMPLE);
}
PPM::~PPM()
{

}

uint8 PPM::getModel()
{
    return model;
}

bool PPM::Reset(int mod)
{
    model=mod;

    switch(model)
    {
     case PPM_MODEL_SIMPLE:
        for(int i=0;i<257;i++)
        {
            buff[i]=i;
        }
        return true;
    };
    return false;
}

void PPM::Update(uint32 index)
{
    switch(model)
    {
     case PPM_MODEL_SIMPLE:
        for(;index<256;index++) buff[index+1]++;
        if(buff[256]>=MAXSCALE)
        {
            for ( index = 1 ; index < 257 ; index++ )
            {
                buff[ index ] /= 2;
                if ( buff[ index ] <= buff[ index-1 ] )
                        buff[ index ] = buff[ index-1 ] + 1;
            }
        }
        break;
     default:
        return;
    };
}
int32 PPM::Scale()
{
    switch(model)
    {
    case PPM_MODEL_SIMPLE:
        return buff[256];
    default:
        return 0;
    };
}
uint32 PPM::Hight(uint32 index)
{
    switch(model)
    {
    case PPM_MODEL_SIMPLE:
        return buff[index+1];
    default:
        return 0;
    };
}
uint32 PPM::Low(uint32 index)
{
    switch(model)
    {
    case PPM_MODEL_SIMPLE:
        return buff[index];
    default:
        return 0;
    };
}

//процедура ускоренного поиска интервала
__inline uint32 _ppm_SearchInterval(uint32 count, uint32 *ch, uint32 asize)
{
 int ind=(asize>>1), up=(asize-1), down=0;

        if(ch[asize-1]<=count)return asize-1;
        while(!(count>=ch[ind] && count<ch[ind+1]))
        {
                if(count<ch[ind])
                {
                        up=ind-1;
                }
                else
                {
                        down=ind+1;
                }
                ind=down+((up-down)>>1);
        }
        return ind;
}

int PPM::Index(uint32 count)
{
    switch(model)
    {
    case PPM_MODEL_SIMPLE:
        return _ppm_SearchInterval(count, buff, 256);
    default:
        return 0;
    };
}
