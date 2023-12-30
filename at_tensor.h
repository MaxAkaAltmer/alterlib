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

#ifndef AT_TENSOR_H
#define AT_TENSOR_H

#include "avariant.h"

namespace alt {

    template <class T>
    class dimensions
    {
    private:

        struct Internal
        {
            T size; //размер
            T refcount; //число пользователей
            T buff[2]; //буффер
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

    template <class T>
    class tensor
    {
    private:
        array<T> data;
        dimensions<uintz> dim, step;

        void makeSteps()
        {
            step.resize(dim.size());
            for(uintz i=0;i<step.size();i++)
            {
                step[i] = 1;
                for(uintz j=i+1;j<dim.size();j++)
                    step[i]*=dim[j];
            }
        }

    public:
        tensor(){}

        template <typename... I>
        tensor(I ...args):
            dim(args...)
        {
            data.resize(dim.rawSize(),false);
            makeSteps();
        }

        tensor(const tensor &val)
        {
            data = val.data;
            dim = val.dim;
            step = val.step;
        }

        tensor(const dimensions<uintz> &val)
            : dim(val)
        {
            data.resize(dim.rawSize(),false);
            makeSteps();
        }

        ~tensor() {}

        tensor& operator=(const tensor &val)
        {
            data = val.data;
            dim = val.dim;
            step = val.step;
            return *this;
        }

        const dimensions<uintz>& dims() const { return dim; }
        const dimensions<uintz>& steps() const { return step; }

        T* operator()() { return data();}
        const T* operator()() const { return data();}

        template <typename... I>
        T& operator[](I ...args)
        {
            uintz i = 0;
            uintz offset = 0;
            ([&]{
                offset += args * step[i];
                i ++;
            }(), ...);
            return data[offset];
        }

        T& operator[](const dimensions<uintz> &off)
        {
            uintz offset = 0;
            for(uintz i = 0;i<dim.size();i++)
            {
                offset += off[i]*step[i];
            }
            return data[offset];
        }

        const T& operator[](const dimensions<uintz> &off) const
        {
            uintz offset = 0;
            for(uintz i = 0;i<dim.size();i++)
            {
                offset += off[i]*step[i];
            }
            return data[offset];
        }

        void fill(T val) {data.fill(val);}

        void resize(const dimensions<uintz> &val)
        {
            if(dim==val)
                return;
            dim = val;
            data.resize(dim.rawSize(),false);
            makeSteps();
        }

        string toString(bool align = false,
                        dimensions<uintz> start = dimensions<uintz>(),
                        dimensions<uintz> end = dimensions<uintz>())
        {
            string rv;

            if(dim.empty()) return "[]";

            if(start.empty())
                start.resize(dim.size());
            if(end.empty())
                end = dim;

            dimensions<uintz> subspace = end;
            subspace -= start;
            dimensions<uintz> index;
            index.resize(dim.size());

            uintz elsize = 0;
            if(align)
            {
                do
                {
                    dimensions<uintz> point = start;
                    point += index;
                    T *ptr = &(*this)[point];
                    for(uintz i=0;i<subspace[subspace.size()-1];i++)
                    {
                        variant tmp(ptr[i]);
                        int n = tmp.toString().size();
                        if(elsize<n)
                            elsize = n;
                    }
                }
                while(index.inc(subspace,dim.size()-2));
                index.clear();
            }

            do
            {
                int left_count = 1;
                for(uintz i=index.size()-1;i>0;i--)
                {
                    if(index[i-1]) break;
                    left_count++;
                }
                rv += string::mono(' ', index.size()-left_count);
                rv += string::mono('[', left_count);

                dimensions<uintz> point = start;
                point += index;
                T *ptr = &(*this)[point];
                for(uintz i=0;i<subspace[subspace.size()-1];i++)
                {
                    variant tmp(ptr[i]);
                    if(i)rv+=", ";
                    if(align)
                    {
                        int n = elsize-tmp.toString().size();
                        if(n)
                            rv += string::mono(' ', n);
                    }
                    rv += tmp.toString();
                }
                int right_count = 1;
                for(uintz i=index.size()-1;i>0;i--)
                {
                    if(index[i-1]!=subspace[i-1]-1) break;
                    right_count++;
                }
                rv += string::mono(']', right_count)+"\n";
            }
            while(index.inc(subspace,dim.size()-2));

            return rv;
        }
    };


}

#endif // AT_TENSOR_H
