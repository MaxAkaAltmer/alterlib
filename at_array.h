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

#ifndef AT_LIST_H
#define AT_LIST_H

#include "at_string_utils.h"
#include "types.h"
#include "math_int.h"
#include <assert.h>

template <class T>
struct ATArrayInternal
{
    int size;
    int alloc;
    int refcount;
    T *buff;
};

template <class T>
class ATArray
{
private:
    ATArrayInternal<T> *data;

    ATArrayInternal<T>* newInternal(int size)
    {
        ATArrayInternal<T> *rv;
        int alloc=_ats_upsize((uint32)size);
        rv=new ATArrayInternal<T>;
        rv->buff=new T[alloc];
        rv->alloc=alloc;
        rv->refcount=1;
        rv->size=size;
        return rv;
    }

    void deleteInternal()
    {
        if(!data)return;
        data->refcount--;
        if(!data->refcount)
        {
            delete []data->buff;
            delete data;            
        }
        data=NULL;
    }

    void cloneInternal()
    {
        if(!data)return;
        if(data->refcount<2)return;
        ATArrayInternal<T> *tmp=newInternal(data->size);
        if(data->size)
            _ats_memcpy(tmp->buff,data->buff,data->size);
        deleteInternal();
        data=tmp;
    }

public:
    ATArray()
    {
        data=NULL;
    }
    ATArray(const ATArray<T> &val)
    {
        data=val.data;
        if(data)data->refcount++;
    }
    ATArray(int size)
    {
        data=newInternal(size);
    }
    ATArray(const T &v1, const T &v2)
    {
        data=newInternal(2);
        data->buff[0]=v1;
        data->buff[1]=v2;
    }
    ATArray& operator=(const ATArray<T> &val)
    {
        if(data==val.data)return *this;
        deleteInternal();
        data=val.data;
        if(data)data->refcount++;
        return *this;
    }
    ~ATArray()
    {
        deleteInternal();
    }
    ATArray<T>& clear(bool memfree=false)
    {
        if(!data)return *this;
        if(!data->size)return *this;
        if(data->refcount>1) //не только наше?
        {
            data->refcount--;
            data=NULL;
            return *this;
        }
        if(memfree)
        {
            deleteInternal();
        }
        else
        {
            data->size=0;
        }
        return *this;
    }

    int size() const
    {
        if(!data)return 0;
        return data->size;
    }


    ATArray<T>& append(const T &val)
    {
        if(!data)data=newInternal(0);
        if(data->refcount<2 && data->alloc>data->size)
        {
            data->buff[data->size]=val;
            data->size++;
            return *this;
        }
        ATArrayInternal<T> *tmp=newInternal(data->size+1);
        if(data->size)
            _ats_memcpy(tmp->buff,data->buff,data->size);
        tmp->buff[data->size]=val;
        deleteInternal();
        data=tmp;
        return *this;
    }

    ATArray<T>& reserve(int size)
    {
        if(data && size<=data->size)return *this;
        ATArrayInternal<T> *tmp=newInternal(size);
        if(data && data->size)
        {
            _ats_memcpy(tmp->buff,data->buff,data->size);
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

    ATArray<T>& resize(int size)
    {
        reserve(size);
        if(data)
        {
            data->size=size;
        }
        return *this;
    }

    ATArray<T>& fill(const T &val)
    {
        cloneInternal();
        if(data)
        {
            for(int i=0;i<data->size;i++)
                data->buff[i]=val;
        }
        return *this;
    }

    ATArray<T>& append(const ATArray<T> &list)
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
            _ats_memcpy(data->buff+data->size,list.data->buff,list.data->size);
            data->size+=list.data->size;
            return *this;
        }
        ATArrayInternal<T> *tmp=newInternal(data->size+list.data->size);
        if(data->size)
            _ats_memcpy(tmp->buff,data->buff,data->size);
        _ats_memcpy(tmp->buff+data->size,list.data->buff,list.data->size);
        deleteInternal();
        data=tmp;
        return *this;
    }

    ATArray<T>& insert(int pos, const T &val)
    {
        if(!data)data=newInternal(0);

        if(data->size<pos)pos=data->size;
        else if(pos<0) pos=0;

        if(data->refcount<2 && data->alloc>data->size)
        {
            if(pos<data->size)
                _ats_memcpy(&data->buff[pos+1],&data->buff[pos],data->size-pos);
            data->buff[pos]=val;
            data->size++;
            return *this;
        }

        ATArrayInternal<T> *tmp=newInternal(data->size+1);
        if(pos)
            _ats_memcpy(tmp->buff,data->buff,pos);
        tmp->buff[pos]=val;
        if(pos<data->size)
            _ats_memcpy(&tmp->buff[pos+1],&data->buff[pos],data->size-pos);
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
        return data->buff[data->size-1];
    }

    const T* operator()() const
    {
        if(!data || !data->size)return NULL;
        return data->buff;
    }

    T* operator()()
    {
        cloneInternal();
        if(!data || !data->size)return NULL;
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
        for(int i=0;i<data->size;i++)
            if(data->buff[i]==val)return i;
        return -1;
    }

    void removeValue(const T &val)
    {
        if(!data)return;
        int ind=0;
        for(int i=0;i<data->size;i++)
        {
            if(data->buff[i]==val)continue;
            data->buff[ind]=data->buff[i];
            ind++;
        }
        data->size=ind;
    }

    ATArray<T>& cut(int ind, int size=1)
    {
        if(!data)return *this;
        if(!size || ind>=data->size)return *this;
        cloneInternal();
        if(ind+size>data->size)size=data->size-ind;
        data->size-=size;
        for(int i=ind;i<data->size;i++)
            data->buff[i]=data->buff[i+size];
        return *this;
    }

    ATArray<T>& fastCut(int ind)
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

    bool operator==(const ATArray<T> &val) const
    {
        if(val.size()!=size())return false;
        if(!size())return true;
        for(int i=0;i<data->size;i++)
        {
            if(data->buff[i]!=val.data->buff[i])return false;
        }
        return true;
    }

    bool operator<(const ATArray<T> &val) const
    {
        if(size()<val.size())return true;
        if(size()>val.size())return false;
        for(int i=0;i<size();i++)
        {
            if(data->buff[i]<val[i])return true;
            if(data->buff[i]>val[i])return false;
        }
        return false;
    }

    bool contains(const T &val) const
    {
        for(int i=0;i<size();i++)
        {
            if(data->buff[i]==val)return true;
        }
        return false;
    }

    ATArray<int> sort(bool toBigger=false) const
    {
        ATArray<int> rv;
        int off, siz=size();

        if(!siz)return rv;
        if(siz==1)
        {
            rv.append(0);
            return rv;
        }

        rv.resize(siz*2);
        int p2p=__bsr32(siz-1);
        bool dest=p2p&1;

        //сортируем от начала
        if(dest)off=0;
        else off=siz;
        for(int i=0;i<siz;i+=2)
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

        int old_off;
        for(int i=1;i<p2p;i++)
        {
            dest=!dest;
            old_off=off;
            if(dest)off=0;
            else off=siz;

            int step=1<<i;
            for(int j=0;j<siz;j+=step*2)
            {
                if((j+step)>=siz) //без слияния
                {
                    for(int k=0;k<(siz-j);k++)
                    {
                        rv[off+j+k]=rv[old_off+j+k];
                    }
                }
                else
                {
                    int k1=0,n1=step;
                    int k2=0,n2=((j+step*2)>siz)?siz-j-step:step;
                    for(int k=0;k<n1+n2;k++)
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
    ATArray<int> sort(bool (*cproc_big)(const T&,const T&), bool toBigger=false) const
    {
        ATArray<int> rv;
        int off, siz=size();

        if(!siz)return rv;
        if(siz==1)
        {
            rv.append(0);
            return rv;
        }

        rv.resize(siz*2);
        int p2p=__bsr32(siz-1);
        bool dest=p2p&1;

        //сортируем от начала
        if(dest)off=0;
        else off=siz;
        for(int i=0;i<siz;i+=2)
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

        int old_off;
        for(int i=1;i<p2p;i++)
        {
            dest=!dest;
            old_off=off;
            if(dest)off=0;
            else off=siz;

            int step=1<<i;
            for(int j=0;j<siz;j+=step*2)
            {
                if((j+step)>=siz) //без слияния
                {
                    for(int k=0;k<(siz-j);k++)
                    {
                        rv[off+j+k]=rv[old_off+j+k];
                    }
                }
                else
                {
                    int k1=0,n1=step;
                    int k2=0,n2=((j+step*2)>siz)?siz-j-step:step;
                    for(int k=0;k<n1+n2;k++)
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

};


#endif // AT_LIST_H
