/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2025 Maxim L. Grishin  (altmer@arts-union.ru)

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

#ifndef AT_DIM_H
#define AT_DIM_H

#include "atypes.h"
#include "at_hash.h"
#include "astring.h"

namespace alt {

    template <class T>
    class dimensions
    {
    private:

        struct Internal
        {
            T size; //размер
            T refcount; //число пользователей
            T buff[1]; //буффер
        };

        Internal *data = nullptr;

        Internal* newInternal(T size)
        {
            Internal *rv;
            if(!size)
                return nullptr;
            rv=(Internal*)new T[sizeof(Internal)/sizeof(T)+size-1];
            rv->refcount=1;
            rv->size=size;
            alt::utils::memset(rv->buff,T(0),size);
            return rv;
        }

        void deleteInternal()
        {
            if(!data)return;
            data->refcount--;
            if( !data->refcount )
                delete []((T*)data);
        }

        void cloneInternal()
        {
            if( !data || data->refcount<2)return;
            Internal *tmp=newInternal(data->size);
            alt::utils::memcpy(tmp->buff,data->buff,data->size);
            deleteInternal();
            data=tmp;
        }

    public:
        dimensions() {}
        dimensions(const dimensions &val)
        {
            if(!val.data) return;
            data = val.data;
            data->refcount++;
        }

        template<class X>
        dimensions(const dimensions<X> &val)
        {
            data = newInternal(val.size());
            for(T i=1;i<data->size;i++)
                data->buff[i] = val[i];
        }

        template <typename... I>
        dimensions(I ...args)
        {
            data = newInternal(sizeof...(I));
            T i = 0;
            ([&]{
                data->buff[i++] = args;
            }(), ...);
        }

        dimensions& operator=(const dimensions &val)
        {
            if(data==val.data)return *this;
            if(data && (data->refcount<2) && (data->size == val.data->size) )
            {
                alt::utils::memcpy(data->buff,val.data->buff,val.data->size);
                return *this;
            }
            deleteInternal();
            data=val.data;
            if(data)
                data->refcount++;
            return *this;
        }

        template<class X>
        dimensions& operator=(const dimensions<X> &val)
        {
            deleteInternal();
            data = newInternal(val.size());
            for(T i=1;i<data->size;i++)
                data->buff[i] = val.data->buff[i];
            return *this;
        }

        bool empty() const
        {
            return !data;
        }

        void resize(T size)
        {
            if(data && size == data->size)
            {
                cloneInternal();
                if(data)
                    alt::utils::memset(data->buff,T(0),size);
            }
            else
            {
                deleteInternal();
                data = newInternal(size);
            }
        }

        T encodePermutation()
        {
            set<T> check;
            if(!data->size)
                return -1;
            for(T i=0;i<data->size; i++)
            {
                if(data->buff[i] >= data->size)
                    return -1;
                check.insert(data->buff[i]);
            }
            if(check.size()!=data->size)
                return -1;

            T rv = 0;
            for(T i=0;i<data->size-1; i++)
            {
                rv *= data->size-i;
                int k=0;
                for(T j=0;j<data->size; j++)
                {
                    if(i==data->buff[j])
                    {
                        rv += k;
                        break;
                    }
                    else if(data->buff[j] > i)
                    {
                        k++;
                    }
                }
            }
            return rv;
        }

        bool decodePermutation(T val)
        {
            T k = 1;
            for(T i=0;i<data->size; i++)
            {
                data->buff[i] = data->size-1;
                if(i)
                    k *= i;
            }

            for(T i=0;i<data->size-1; i++)
            {
                T ind = val/k;
                if(ind>data->size-1-i)
                    return false;
                val %=k;
                k /= data->size-i-1;
                int pos = 0;
                for(int j=0;j<data->size;j++)
                {
                    if(data->buff[j] != data->size-1)
                        continue;
                    if(pos == ind)
                    {
                        data->buff[j] = i;
                        break;
                    }
                    pos++;
                }
            }
            return true;
        }

        static string checkPermutation(T size)
        {
            dimensions<T> test;
            test.resize(size);

            T count = 1;
            for(T i=2; i<=size; i++)
                count *= i;

            for(T i=0;i<count;i++)
            {
                test.decodePermutation(i);
                if(test.encodePermutation() != i)
                    return "checkPermutation error on " + string::fromInt(i) + " " + test.toString();
                if(!i)
                {
                    for(T j=0; j<size; j++)
                    {
                        if(test[j]!=j)
                            return "checkPermutation error on first " + test.toString();
                    }
                }
                else if(i==count-1)
                {
                    for(T j=0; j<size; j++)
                    {
                        if(test[size-1-j]!=j)
                            return "checkPermutation error on last " + test.toString();
                    }
                }
            }

            return "checkPermutation is Ok";
        }

        ~dimensions()
        {
            deleteInternal();
        }

        T rawSize() const
        {
            if(!data) return 0;
            T rv = data->buff[0];
            for(T i=1;i<data->size;i++)
                rv *= data->buff[i];
            return rv;
        }

        void roundup(dimensions div)
        {
            for(T i=1;i<data->size && i<div.data->size;i++)
                data->buff[i] = alt::roundup(data->buff[i],div.data->buff[i]);
        }

        T size() const
        {
            if(!data) return 0;
            return data->size;
        }

        T& operator[](T ind)
        {
            cloneInternal();
#ifdef ENABLE_BUGEATER
            assert(!(ind<0 || !data || ind>=data->size));
#endif
            return data->buff[ind];
        }
        const T& operator[](T ind) const
        {
#ifdef ENABLE_BUGEATER
            assert(!(ind<0 || !data || ind>=data->size));
#endif
            return data->buff[ind];
        }

        T* operator()()
        {
            cloneInternal();
            if(!data) return nullptr;
            return data->buff;
        }
        const T* operator()() const
        {
            if(!data) return nullptr;
            return data->buff;
        }

        T toLinearIndex(const dimensions &ref) const
        {
            T rv = 0;
            for(uintz i=0;i<data->size;i++)
            {
                T tmp = data->buff[i];
                for(uintz j=i+1;j<data->size;j++)
                    tmp *= ref[j];
                rv += tmp;
            }
            return rv;
        }

        dimensions makeIndexDim(T linear) const
        {
            dimensions rv; rv.resize(data->size);
            for(uintz i=data->size-1;i>0;i--)
            {
                rv[i] = linear%data->buff[i];
                linear/=data->buff[i];
            }
            rv[0] = linear;
            return rv;
        }

        bool inc(const dimensions &ref, uintz axis, uintz delta = 1)
        {
            cloneInternal();

#ifdef ENABLE_BUGEATER
            assert(data && ref.data && ref.data->size==data->size);
#endif

            if(axis>=data->size) //для упрощения циклов с хвостами
                return false;

            data->buff[axis] += delta;
            while(axis > 0 && data->buff[axis] >= ref[axis])
            {
                if(axis)
                {
                    data->buff[axis-1]+=data->buff[axis]/ref[axis];
                    data->buff[axis]%=ref[axis];
                }
                axis--;
            }
            if(axis==0 && data->buff[axis] >= ref[axis])
            {
                return false;
            }
            return true;
        }

        dimensions& operator += (const dimensions &val)
        {
            cloneInternal();
#ifdef ENABLE_BUGEATER
            assert(data && val.data && val.data->size==data->size);
#endif
            for(uintz i=0;i<data->size;i++)
                data->buff[i]+=val.data->buff[i];
            return *this;
        }

        dimensions& operator -= (const dimensions &val)
        {
            cloneInternal();
#ifdef ENABLE_BUGEATER
            assert(data && val.data && val.data->size==data->size);
#endif
            for(uintz i=0;i<data->size;i++)
                data->buff[i]-=val.data->buff[i];
            return *this;
        }

        dimensions& operator *= (const dimensions &val)
        {
            cloneInternal();
#ifdef ENABLE_BUGEATER
            assert(data && val.data && val.data->size==data->size);
#endif
            for(uintz i=0;i<data->size;i++)
                data->buff[i]*=val.data->buff[i];
            return *this;
        }

        bool operator==(const dimensions &val) const
        {
            if(val.data == data)
                return true;
            if(!data || !val.data || data->size != val.data->size)
                return false;
            for(uintz i=0;i<data->size;i++)
            {
                if(data->buff[i]!=val.data->buff[i])
                    return false;
            }
            return true;
        }

        void clear()
        {
            cloneInternal();
            if(data)
                alt::utils::memset(data->buff,T(0),data->size);
        }

        string toString(string sep="x") const
        {
            if(!data) return string();
            if(sep.size()==1)
                return string::join(data->buff,data->size,sep[0]);
            string rv;
            for(uintz i =0; i<data->size; i++)
            {
                rv += string::fromInt(data->buff[i]);
                rv.append(sep[i%sep.size()]);
            }
            return rv;
        }
    };
}

#endif // AT_DIM_H
