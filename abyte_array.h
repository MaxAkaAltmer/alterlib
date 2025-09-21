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

#ifndef ABYTE_ARRAY_H
#define ABYTE_ARRAY_H

#include "atypes.h"
#include "astring.h"
#include "at_array.h"

namespace alt {

    class byteArray
    {
    protected:

        //todo: make like string
        array<uint8> data;

    public:
        byteArray()
        {
        }

        byteArray(const byteArray &val)
        {
            data=val.data;
        }

        byteArray(const void *buff, int size)
            :data(size)
        {
            if(size)
                utils::memcpy(data(),(uint8*)buff,size);
        }

        byteArray(const void *buff, int off, int size, int endian)
            :data(size)
        {
            if(size)
            {
                for(int i=0;i<size;i++)
                    data[i]=((uint8*)buff)[(off+i)^endian];
            }
        }

        byteArray(int size)
            :data(size)
        {
        }

        ~byteArray()
        {
        }

        byteArray& clear(bool memfree = false)
        {
            data.clear(memfree);
            return *this;
        }

        byteArray& resize(int size)
        {
            data.resize(size);
            return *this;
        }

        byteArray& fill(uint8 val)
        {
            data.fill(val);
            return *this;
        }

        byteArray& reserve(int size)
        {
            data.reserve(size);
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////
        //операторы
        ////////////////////////////////////////////////////////////////////////
        byteArray& operator+=(const byteArray &val)
        {
            data.append(val.data);
            return *this;
        }

        byteArray& operator=(const byteArray &val)
        {
            data=val.data;
            return *this;
        }

        byteArray operator+(const byteArray &val) const;

        byteArray& prepend(uint8 val)
        {
            data.insert(0,val);
            return *this;
        }
        byteArray& prepend(const void *buff, int size)
        {
            data.insert(0,(uint8*)buff,size);
            return *this;
        }
        byteArray& prepend(const byteArray &val)
        {
            return prepend(val(),val.size());
        }
        template <class T> byteArray& prependT(const T &val)
        {
            return prepend(&val,sizeof(T));
        }

        byteArray& append(uint8 val)
        {
            data.append(val);
            return *this;
        }
        byteArray& append(const void *buff, int size)
        {
            data.append((uint8*)buff,size);
            return *this;
        }
        byteArray& append(const byteArray &val)
        {
            return append(val(),val.size());
        }
        template <class T> byteArray& appendT(const T &val)
        {
            return append(&val,sizeof(T));
        }

        template <class T> T getValue(int off) const
        {
            if(data.size()<off+sizeof(T))return T();
            T rv;
            utils::memcpy((uint8*)&rv,data()+off,sizeof(T));
            return rv;
        }

        byteArray& remove(int ind, int size)
        {
            data.cut(ind,size);
            return *this;
        }

        static string toHex(const void *data, uint size, bool up_case=false, int sep=0, string insert = " ");
        string toHex(bool up_case=false, int sep=0, string insert = " ");
        string toCPPArray(string name, bool up_case=false);
        string toAnsiView(const char replacer = '.');

        bool operator==(const byteArray &val) const
        {
            return data==val.data;
        }
        bool operator!=(const byteArray &val) const
        {
            return !(data==val.data);
        }

        const uint8* operator()() const //получить ссылку на буфер
        {
            return data();
        }
        uint8* operator()() //получить ссылку на буфер
        {
            return data();
        }

        uint8& operator[](int ind)
        {
            return data()[ind];
        }
        uint8 operator[](int ind) const
        {
            return data[ind];
        }

        bool isEmpty() const       //проверка на пустоту
        {
            return !data.size();
        }

        bool isZero() const
        {
            for(int i=0;i<data.size();i++)
                if(data[i])return false;
            return true;
        }

        int size() const
                { return data.size(); }
        int allocated() const      //размер выделенной памяти
                { return data.allocated(); }

        ////////////////////////////////////////////////////////////////////////////

        static byteArray fromHex(const char *buff, int size=-1);

    };

    __inline uint32 aHash(const alt::byteArray &key)
    {
        uint32 rv=0;
        for(int i=0;i<key.size();i++)
        {
            rv=(rv>>1)|(rv<<31);
            rv^=key[i];
        }
        return rv;
    }

} //namespace alt

#endif // ABYTE_ARRAY_H
