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

#include "abyte_array.h"
#include <ctype.h>

using namespace alt;

byteArray byteArray::operator+(const byteArray &val) const
{
    if(!val.data.size())return *this;
    if(!data.size())return val;
    int nsiz=val.data.size()+data.size();

    byteArray tmp(nsiz);
    utils::memcpy(tmp.data(),data(),data.size());
    utils::memcpy(&tmp.data[data.size()],val.data(),val.data.size());
    return tmp;
}

uint8 _adata_make_half_hex(uint8 val, bool up)
{
    if(val<10)return '0'+val;
    if(up)return 'A'+val-10;
    return 'a'+val-10;
}

string byteArray::toAnsiView(const char replacer)
{
    string rv;
    for(int i=0;i<data.size();i++)
    {
        if(isprint(data[i]))
            rv.append(data[i]);
        else
            rv.append(replacer);
    }
    return rv;
}

string byteArray::toCPPArray(string name, bool up_case)
{
    string rv="static const unsigned char "+name+"[] =\r\n{";

    for(int i=0;i<data.size();i++)
    {
        if(!(i&0xf))rv+="\r\n\t";
        rv+="0x";
        uint8 val=data[i];
        rv.append(_adata_make_half_hex(val>>4,up_case));
        rv.append(_adata_make_half_hex(val&0xf,up_case));
        if(i<data.size()-1)rv+=",";
    }

    return rv+"\r\n};\r\n";
}

string byteArray::binToHex(const void *data, uint size, bool up_case, int sep, string insert)
{
    string rv;
    if(!data || !size)return rv;

    rv.reserve(size*2+(sep?(size/sep)*insert.size():0));
    for(uint i=0;i<size;i++)
    {
        uint8 val=((const uint8*)data)[i];
        if(sep && !(i%sep) && i)
        {
            rv+=insert;
        }
        rv.append(_adata_make_half_hex(val>>4,up_case));
        rv.append(_adata_make_half_hex(val&0xf,up_case));
    }
    return rv;
}

string byteArray::toHex(bool up_case, int sep, string insert)
{
    string rv;
    if(!data.size())return rv;

    rv.reserve(data.size()*2+(sep?(data.size()/sep)*insert.size():0));
    for(int i=0;i<data.size();i++)
    {
        uint8 val=data[i];
        if(sep && !(i%sep) && i)
        {
            rv+=insert;
        }
        rv.append(_adata_make_half_hex(val>>4,up_case));
        rv.append(_adata_make_half_hex(val&0xf,up_case));
    }
    return rv;
}

byteArray byteArray::fromHex(const char *buff, int size)
{
    byteArray rv;

    if(size<0)size=utils::strlen(buff);
    rv.resize((size+1)>>1);

    for(int i=size-1;i>=0;i--)
    {
        uint8 half=0;
        if(buff[i]>='0' && buff[i]<='9')
        {
            half=buff[i]-'0';
        }
        else if(buff[i]>='A' && buff[i]<='F')
        {
            half=buff[i]-'A'+10;
        }
        else if(buff[i]>='a' && buff[i]<='f')
        {
            half=buff[i]-'a'+10;
        }

        if((i^size)&1)rv.data[i>>1]=half;
        else rv.data[i>>1]|=half<<4;
    }

    return rv;
}
