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

#ifndef ADATA_H
#define ADATA_H

#include "types.h"
#include "at_string_utils.h"
#include "astring.h"

struct ADataInternal
{
    int size; //размер
    int alloc; //объем выделенной памяти (если меньше 0 - то эту память можно менять без учета ссылок)
    int refcount; //число пользователей данных
    uint8 buff[1]; //буффер
};

class AString;
class AData
{
protected:

    ADataInternal *data;
    ADataInternal* newInternal(int size)
    {
        ADataInternal *rv;
        int alloc=_ats_upsize((uint32)size);
        rv=(ADataInternal*)new char[sizeof(ADataInternal)+alloc];
        rv->alloc=alloc;
        rv->refcount=1;
        rv->size=size;
        return rv;
    }

    void deleteInternal()
    {
        data->refcount--;
        if(data==&empty)return;
        if( !data->refcount )
            delete []((char*)data);
    }

    void cloneInternal()
    {
        if( data!=(&empty) && data->refcount<2)return;
        ADataInternal *tmp=newInternal(data->size);
        if(data->size)_ats_memcpy(tmp->buff,data->buff,data->size);
        deleteInternal();
        data=tmp;
    }

    static ADataInternal empty;

public:
    AData()
    {
        data=&empty;
        data->size=0;
        data->alloc=0;
        data->refcount++;
    }

    AData(const AData &val)
    {
        //добвляем референс
        data=val.data;
        data->refcount++;
    }

    AData(const void *buff, int size)
    {
        data=newInternal(size);
        if(size)_ats_memcpy(data->buff,(uint8*)buff,size);
    }

    AData(const void *buff, int off, int size, int endian)
    {
        data=newInternal(size);
        if(size)
        {
            for(int i=0;i<size;i++)
                data->buff[i]=((uint8*)buff)[(off+i)^endian];
        }
    }

    AData(int size)
    {
        data=newInternal(size);
    }

    ~AData()
    {
        deleteInternal();
    }

    AData& clear()
    {
        if(!data->size)return *this;
        cloneInternal();
        data->size=0;
        return *this;
    }
    AData& clean()
    {
        deleteInternal();
        data=&empty;
        return *this;
    }

    AData& resize(int size)
    {
        if(data->size==size)return *this;

        if(data->alloc<size)
        {
            ADataInternal *tmp=newInternal(size);
            _ats_memcpy(tmp->buff,data->buff,data->size);
            deleteInternal();
            data=tmp;
            return *this;
        }

        cloneInternal();
        data->size=size;
        return *this;
    }

    AData& reserve(int size)
    {
        if(data->alloc>=size)return *this;
        ADataInternal *tmp=newInternal(size);
        if(data->size)_ats_memcpy(tmp->buff,data->buff,data->size);
        tmp->size=data->size;
        deleteInternal();
        data=tmp;
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////
    //операторы
    ////////////////////////////////////////////////////////////////////////
    AData& operator+=(const AData &val);

    AData& operator=(const AData &val);

    AData operator+(const AData &val) const;

    AData& prepend(uint8 val);
    AData& prepend(const void *buff, int size);
    AData& prepend(const AData &val)
    {
        return prepend(val(),val.size());
    }
    template <class T> AData& prependT(const T &val)
    {
        return prepend(&val,sizeof(T));
    }

    AData& append(uint8 val);
    AData& append(const void *buff, int size);
    AData& append(const AData &val)
    {
        return append(val(),val.size());
    }
    template <class T> AData& appendT(const T &val)
    {
        return append(&val,sizeof(T));
    }

    template <class T> T getValue(int off) const
    {
        if(data->size<off+sizeof(T))return T();
        T rv;
        _ats_memcpy((uint8*)&rv,data->buff+off,sizeof(T));
        return rv;
    }

    AData& remove(int ind, int size);

    AString toHex(bool up_case=false, bool sep=false);
    AString toCPPArray(AString name, bool up_case=false);

    bool operator==(const AData &Str) const
    {
        if(data->size!=Str.data->size)return false;
        if(data->size==0)return true;
        if(!_ats_memcmp(data->buff,Str.data->buff,data->size))return true;
        return false;
    }
    bool operator!=(const AData &Str) const
    {
        return !((*this)==Str);
    }

    const uint8* operator()() const //получить ссылку на буфер
    {
        return data->buff;
    }
    uint8* operator()() //получить ссылку на буфер
    {
        cloneInternal();
        return data->buff;
    }

    uint8& operator[](int ind)
    {
        cloneInternal();
        return data->buff[ind];
    }
    uint8 operator[](int ind) const
    {
        return data->buff[ind];
    }

    bool isEmpty() const       //проверка на пустоту
    {
        return !data->size;
    }

    bool isZero() const
    {
        for(int i=0;i<data->size;i++)
            if(data->buff[i])return false;
        return true;
    }

    int size() const
            { return data->size; }
    int Allocated() const      //размер выделенной памяти
            { return data->alloc; }

    ////////////////////////////////////////////////////////////////////////////

    static AData fromHex(const char *buff, int size=-1);

};

__inline uint32 aHash(const AData &key)
{
    uint32 rv=0;
    for(int i=0;i<key.size();i++)
    {
        rv=(rv>>1)|(rv<<31);
        rv^=key[i];
        i++;
    }
    return rv;
}

#endif // ADATA_H
