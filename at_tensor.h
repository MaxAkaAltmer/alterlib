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
