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

#ifndef AT_LIST_H
#define AT_LIST_H

#include "atypes.h"
#include "amath_int.h"
#include <assert.h>

namespace alt {

    template<class T, uintz SIZE>
    class fixedArray
    {
    public:
        fixedArray(){ for(uintz i=0;i<SIZE;i++) data[i] = 0; }
        fixedArray(const fixedArray &val)
        {
            *this = val;
        }
        fixedArray(T fill){ for(uintz i=0;i<SIZE;i++) data[i] = fill;}
        fixedArray(T x, T y){ if(SIZE>0) data[0] = x; if(SIZE>1) data[1] = y;}
        fixedArray(T x, T y, T z){ if(SIZE>0) data[0] = x; if(SIZE>1) data[1] = y; if(SIZE>2) data[2] = z;}

        fixedArray& operator=(const fixedArray &val)
        {
            for(uintz i=0;i<SIZE;i++) data[i] = val.data[i];
            return *this;
        }

        uintz size() const { return SIZE; }

        T& operator[](uintz ind) {return data[ind];}
        T* operator()() {return data;}

        const T& operator[](uintz ind) const {return data[ind];}
        const T* operator()() const {return data;}

    private:
        T data[SIZE];
    };

    template <class T>
    class array
    {
    private:

        struct Internal
        {
            intz size;
            intz alloc;
            uintz refcount;
            T *buff;
        };

        Internal *data;

        Internal* newInternal(intz size, bool overhead = true)
        {
            Internal *rv;
            intz alloc=overhead ? alt::utils::upsize((uintz)size) : size;
            rv=new Internal;
            rv->buff=new T[alloc];
            rv->alloc=alloc;
            rv->refcount=1;
            rv->size=size;
            return rv;
        }

        void deleteInternal()
        {
            if(!data)return;
            int ref = --data->refcount;
            if(!ref)
            {
                delete []data->buff;
                delete data;
            }
            data = nullptr;
        }

        void cloneInternal()
        {
            if(!data)return;
            if(data->refcount<2)return;
            Internal *tmp=newInternal(data->size);
            if(data->size)
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
            deleteInternal();
            data=tmp;
        }

    public:
        array()
        {
            data=NULL;
        }
        array(const array &val)
        {
            data=val.data;
            if(data)data->refcount++;
        }
        explicit array(intz size)
        {
            data=newInternal(size);
        }
        array(const T &v1, const T &v2)
        {
            data=newInternal(2);
            data->buff[0]=v1;
            data->buff[1]=v2;
        }

        int refCount() const
        {
            if(!data) return 1;
            return data->refcount;
        }

        array& operator=(const array &val)
        {
            if(data==val.data)return *this;
            deleteInternal();
            data=val.data;
            if(data)data->refcount++;
            return *this;
        }
        ~array()
        {
            deleteInternal();
        }
        array& clear(bool memfree=false)
        {
            if(!data)return *this;
            if(!data->size)return *this;
            if(data->refcount>1 || memfree)
            {
                deleteInternal();
            }
            else
            {
                data->size=0;
            }
            return *this;
        }

        intz size() const
        {
            if(!data)return 0;
            return data->size;
        }

        intz allocated() const
        {
            if(!data)return 0;
            return data->alloc;
        }

        array& append(const T &val)
        {
            if(!data)data=newInternal(0);
            if(data->refcount<2 && data->alloc>data->size)
            {
                data->buff[data->size]=val;
                data->size++;
                return *this;
            }
            Internal *tmp=newInternal(data->size+1);
            if(data->size)
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
            tmp->buff[data->size]=val;
            deleteInternal();
            data=tmp;
            return *this;
        }

        array& append(const T *buff, intz count)
        {
            if(!count) return *this;
            if(!data)
            {
                data=newInternal(count);
                data->size = 0;
            }
            if(data->refcount<2 && data->alloc>=data->size+count)
            {
                alt::utils::memcpy(&data->buff[data->size],buff,count);
                data->size+=count;
                return *this;
            }
            Internal *tmp=newInternal(data->size+count);
            if(data->size)
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
            alt::utils::memcpy(&tmp->buff[data->size],buff,count);
            deleteInternal();
            data=tmp;
            return *this;
        }

        array& reserve(intz size, bool overhead = true)
        {
            if(data && size<=data->alloc)
                return *this;
            Internal *tmp=newInternal(size, overhead);
            if(data && data->size)
            {
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
                tmp->size=data->size;
            }
            else
            {
                tmp->size=0;
            }
            deleteInternal();
            data=tmp;
            return *this;
        }

        array& resize(intz size, bool overhead = true)
        {
            reserve(size, overhead);
            if(data)
            {
                data->size=size;
            }
            return *this;
        }

        array& fill(const T &val)
        {
            cloneInternal();
            if(data)
            {
                for(intz i=0;i<data->size;i++)
                    data->buff[i]=val;
            }
            return *this;
        }

        array& append(const array &list)
        {
            if(!list.data)return *this;
            if(!list.data->size)return *this;
            if(!data)
            {
                data=newInternal(list.data->size);
                data->size=0;
            }
            if(data->refcount<2 && data->alloc>=(data->size+list.data->size))
            {
                alt::utils::memcpy(data->buff+data->size,list.data->buff,list.data->size);
                data->size+=list.data->size;
                return *this;
            }
            Internal *tmp=newInternal(data->size+list.data->size);
            if(data->size)
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
            alt::utils::memcpy(tmp->buff+data->size,list.data->buff,list.data->size);
            deleteInternal();
            data=tmp;
            return *this;
        }

        array& insert(intz pos, const T &val)
        {
            if(!data)data=newInternal(0);

            if(data->size<pos)pos=data->size;
            else if(pos<0) pos=0;

            if(data->refcount<2 && data->alloc>data->size)
            {
                if(pos<data->size)
                    alt::utils::memcpy(&data->buff[pos+1],&data->buff[pos],data->size-pos);
                data->buff[pos]=val;
                data->size++;
                return *this;
            }

            Internal *tmp=newInternal(data->size+1);
            if(pos)
                alt::utils::memcpy(tmp->buff,data->buff,pos);
            tmp->buff[pos]=val;
            if(pos<data->size)
                alt::utils::memcpy(&tmp->buff[pos+1],&data->buff[pos],data->size-pos);
            deleteInternal();
            data=tmp;
            return *this;
        }

        array& insert(intz pos, const T *buff, intz count)
        {
            if(!count) return *this;
            if(!data)data=newInternal(count);

            if(data->size<pos)pos=data->size;
            else if(pos<0) pos=0;

            if(data->refcount<2 && data->alloc>=data->size+count)
            {
                if(pos<data->size)
                    alt::utils::memcpy(&data->buff[pos+count],&data->buff[pos],data->size-pos);
                alt::utils::memcpy(&data->buff[pos],buff,count);
                data->size+=count;
                return *this;
            }

            Internal *tmp=newInternal(data->size+count);
            if(pos)
                alt::utils::memcpy(tmp->buff,data->buff,pos);
            alt::utils::memcpy(&data->buff[pos],buff,count);
            if(pos<data->size)
                alt::utils::memcpy(&tmp->buff[pos+count],&data->buff[pos],data->size-pos);
            deleteInternal();
            data=tmp;
            return *this;
        }

        T pop()
        {
            if(!data || !data->size)return T();
            cloneInternal();
            data->size--;
            return data->buff[data->size];
        }

        T last() const
        {
    #ifdef ENABLE_BUGEATER
            assert(!(!data || !data->size));
    #endif
            if(!data || !data->size) return T();
            return data->buff[data->size-1];
        }

        const T* operator()() const
        {
            if(!data)return NULL;
            return data->buff;
        }

        T* operator()()
        {
            cloneInternal();
            if(!data)return NULL;
            return data->buff;
        }

        T& last()
        {
            cloneInternal();
    #ifdef ENABLE_BUGEATER
            assert(!(!data || !data->size));
    #endif
            return data->buff[data->size-1];
        }

        int indexOf(const T &val)
        {
            if(!data)return -1;
            for(intz i=0;i<data->size;i++)
                if(data->buff[i]==val)return i;
            return -1;
        }

        void removeValue(const T &val)
        {
            if(!data)
                return;
            if(indexOf(val)<0)
                return;
            cloneInternal();
            intz ind=0;
            for(intz i=0;i<data->size;i++)
            {
                if(data->buff[i]==val)
                    continue;
                data->buff[ind]=data->buff[i];
                ind++;
            }
            data->size=ind;
        }

        array& cut(intz ind, intz size=1)
        {
            if(!data)return *this;
            if(!size || ind>=data->size)return *this;
            cloneInternal();
            if(ind+size>data->size)size=data->size-ind;
            data->size-=size;
            for(intz i=ind;i<data->size;i++)
                data->buff[i]=data->buff[i+size];
            return *this;
        }

        array& fastCut(int ind)
        {
            if(!data)return *this;
            if(ind>=data->size)return *this;
            cloneInternal();
            if(data->size==ind+1)
            {
                data->size--;
                return *this;
            }
            data->buff[ind]=data->buff[data->size-1];
            data->size--;
            return *this;
        }

        array left(int size) const
            {return mid(0,size);}
        array right(int from) const
            {return mid(from,data->size-from);}
        array mid(int from, int size) const
        {
            array retval;

            if(from<0){size+=from;from=0;}
            if(from>=data->size || size<=0)return retval;
            if(from+size>data->size)size=data->size-from;

            retval.resize(size);
            for(int i=0;i<size;i++)
            {
                retval.data->buff[i] = data->buff[from+i];
            }
            return retval;
        }

        const T& operator[](int ind) const
        {
    #ifdef ENABLE_BUGEATER
            assert(!(ind<0 || !data || ind>=data->size));
    #endif
            return data->buff[ind];
        }
        T& operator[](int ind)
        {
            cloneInternal();
    #ifdef ENABLE_BUGEATER
            assert(!(ind<0 || !data || ind>=data->size));
    #endif
            return data->buff[ind];
        }

        bool operator==(const array &val) const
        {
            if(val.size()!=size())return false;
            if(!size())return true;
            for(intz i=0;i<data->size;i++)
            {
                if(data->buff[i]!=val.data->buff[i])return false;
            }
            return true;
        }

        bool operator<(const array &val) const
        {
            if(size()<val.size())return true;
            if(size()>val.size())return false;
            for(intz i=0;i<size();i++)
            {
                if(data->buff[i]<val[i])return true;
                if(data->buff[i]>val[i])return false;
            }
            return false;
        }

        bool contains(const T &val) const
        {
            for(intz i=0;i<size();i++)
            {
                if(data->buff[i]==val)return true;
            }
            return false;
        }

        array<intz> sort(bool toBigger=false) const
        {
            array<intz> rv;
            intz off, siz=size();

            if(!siz)return rv;
            if(siz==1)
            {
                rv.append(0);
                return rv;
            }

            rv.resize(siz*2);
            intz p2p=alt::imath::bsrT(siz-1);
            bool dest=p2p&1;

            //сортируем от начала
            if(dest)off=0;
            else off=siz;
            for(intz i=0;i<siz;i+=2)
            {
                if(i+1==siz)
                {
                    rv[off+i]=i;
                }
                else
                {
                    bool test=data->buff[i]>data->buff[i+1];
                    if(test == toBigger)
                    {
                        rv[off+i]=i+1;
                        rv[off+i+1]=i;
                    }
                    else
                    {
                        rv[off+i]=i;
                        rv[off+i+1]=i+1;
                    }
                }
            }

            intz old_off;
            for(intz i=1;i<p2p;i++)
            {
                dest=!dest;
                old_off=off;
                if(dest)off=0;
                else off=siz;

                intz step=1<<i;
                for(intz j=0;j<siz;j+=step*2)
                {
                    if((j+step)>=siz) //без слияния
                    {
                        for(intz k=0;k<(siz-j);k++)
                        {
                            rv[off+j+k]=rv[old_off+j+k];
                        }
                    }
                    else
                    {
                        intz k1=0,n1=step;
                        intz k2=0,n2=((j+step*2)>siz)?siz-j-step:step;
                        for(intz k=0;k<n1+n2;k++)
                        {
                            bool test = data->buff[rv[old_off+j+k1]]>data->buff[rv[old_off+j+step+k2]];
                            if(test == toBigger)
                            {
                                rv[off+j+k]=rv[old_off+j+step+k2];
                                k2++;
                                if(k2==n2) //докопируем
                                {
                                    for(k++;k<n1+n2;k++,k1++)
                                    {
                                        rv[off+j+k]=rv[old_off+j+k1];
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                rv[off+j+k]=rv[old_off+j+k1];
                                k1++;
                                if(k1==n1) //докопируем
                                {
                                    for(k++;k<n1+n2;k++,k2++)
                                    {
                                        rv[off+j+k]=rv[old_off+j+step+k2];
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            rv.resize(siz);
            return rv;
        }
        //cproc_big ~ v1 > v2 -> true
        array<intz> sort(bool (*cproc_big)(const T&,const T&), bool toBigger=false) const
        {
            array<intz> rv;
            intz off, siz=size();

            if(!siz)return rv;
            if(siz==1)
            {
                rv.append(0);
                return rv;
            }

            rv.resize(siz*2);
            intz p2p=alt::imath::bsrT(siz-1);
            bool dest=p2p&1;

            //сортируем от начала
            if(dest)off=0;
            else off=siz;
            for(intz i=0;i<siz;i+=2)
            {
                if(i+1==siz)
                {
                    rv[off+i]=i;
                }
                else
                {
                    bool test=(*cproc_big)(data->buff[i],data->buff[i+1]);
                    if(test == toBigger)
                    {
                        rv[off+i]=i+1;
                        rv[off+i+1]=i;
                    }
                    else
                    {
                        rv[off+i]=i;
                        rv[off+i+1]=i+1;
                    }
                }
            }

            intz old_off;
            for(intz i=1;i<p2p;i++)
            {
                dest=!dest;
                old_off=off;
                if(dest)off=0;
                else off=siz;

                intz step=1<<i;
                for(intz j=0;j<siz;j+=step*2)
                {
                    if((j+step)>=siz) //без слияния
                    {
                        for(intz k=0;k<(siz-j);k++)
                        {
                            rv[off+j+k]=rv[old_off+j+k];
                        }
                    }
                    else
                    {
                        intz k1=0,n1=step;
                        intz k2=0,n2=((j+step*2)>siz)?siz-j-step:step;
                        for(intz k=0;k<n1+n2;k++)
                        {
                            bool test = (*cproc_big)(data->buff[rv[old_off+j+k1]],data->buff[rv[old_off+j+step+k2]]);
                            if(test == toBigger)
                            {
                                rv[off+j+k]=rv[old_off+j+step+k2];
                                k2++;
                                if(k2==n2) //докопируем
                                {
                                    for(k++;k<n1+n2;k++,k1++)
                                    {
                                        rv[off+j+k]=rv[old_off+j+k1];
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                rv[off+j+k]=rv[old_off+j+k1];
                                k1++;
                                if(k1==n1) //докопируем
                                {
                                    for(k++;k<n1+n2;k++,k2++)
                                    {
                                        rv[off+j+k]=rv[old_off+j+step+k2];
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            rv.resize(siz);
            return rv;
        }

        static array poly_evclid_gcd(array a, array b)
        {
            if(b.size())
            {
                a.poly_div(b);
                return poly_evclid_gcd(b, a);
            }
            return a;
        }

        //poly division - numerator in *this and reminder will be in *this
        array poly_div(const array& den)
        {
            if(size() < den.size())
                return array();
            if(!den.size()) //какой аналог бесконечности???
                return *this;

            array rv(size()-den.size()+1);

            for (intz i=size()-den.size(); i>=0; i--)
            {
                rv[i] = (*this)[i+den.size()-1] / den[den.size()-1];

                for (intz j=i+den.size()-1; j>=i; j--)
                {
                        (*this)[j] -= den[j-i] * rv[i];
                }
            }

            intz k = rv.size()-1;
            while(k>=0 && !rv[k]) k--;
            rv.resize(k+1);

            k = size()-1;
            while(k>=0 && !(*this)[k]) k--;
            resize(k+1);

            return rv;
        }


        array poly_mul(const array& val)
        {
            if(!val.size() || !size())
                return array();

            array rv(val.size()+size()-1);

            rv.fill(0);
            for(intz i=0;i<size();i++)
                for(intz j=0;j<val.size();j++)
                    rv[i+j] += (*this)[i]*val[j];

            return rv;
        }

        static array poly_primitive(uint n)
        {
            if(!n)
                return array();
            array rv(n+1);
            rv[0] = -1;

            for(uint i=1;i<n;i++)
            {
                rv[i] = 0;
            }
            rv[n] = 1;

            return rv;
        }

    };

} // namespace alt


#endif // AT_LIST_H
