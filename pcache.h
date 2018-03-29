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

#ifndef PCACHE_H
#define PCACHE_H

#include "at_array.h"

template <class T>
struct PriorNode
{
    T data;
    int prev;
    int next;
};

template <class T>
class PriorityCache
{
private:

    ATArray< PriorNode<T> > list;
    ATArray<int> free;
    volatile int first,last;

public:
    PriorityCache(){last=first=-1;}

    int Push(T val);

    void Update(int ind);

    void Delete(int ind);

    T ForRemove();
    int ForRemoveIndex(){return first;}

    int Size() const;
    void Clear();
};

template <class T>
int PriorityCache<T>::Push(T val)
{
 int ind;
 PriorNode<T> tmp;

    if(free.size())
    {
        ind=free[free.size()-1];
        free.cut(free.size()-1);
    }
    else
    {
        ind=list.size();
        list.append(tmp);
    }

    list[ind].data=val;
    list[ind].next=-1;
    list[ind].prev=last;

    if(last>=0)list[last].next=ind;

    last=ind;
    if(first<0)first=ind;

    return ind;
}

template <class T>
void PriorityCache<T>::Update(int ind)
{
    if(list.size()<=ind || ind<0)return;
    //if(!list[ind].data)return;

    int next=list[ind].next;
    int prev=list[ind].prev;

    if(last==ind)return;

    list[ind].next=-1;
    list[ind].prev=last;

    if(prev>=0)list[prev].next=next;
    else first=next;

    list[next].prev=prev;
    list[last].next=ind;

    last=ind;
}

template <class T>
void PriorityCache<T>::Delete(int ind)
{
    if(list.size()<=ind || ind<0)return;
    //if(!list[ind].data)return;

    if(list[ind].next>=0)
    {
        list[list[ind].next].prev=list[ind].prev;
    }
    else
    {
        last=list[ind].prev;
    }
    if(list[ind].prev>=0)
    {
        list[list[ind].prev].next=list[ind].next;
    }
    else
    {
        first=list[ind].next;
    }
    free.append(ind);
    //list[ind].data=NULL;
}

template <class T>
T PriorityCache<T>::ForRemove()
{
    if(first<0)return T();
    return list[first].data;
}

template <class T>
int PriorityCache<T>::Size() const
{
    return list.size()-free.size();
}

template <class T>
void PriorityCache<T>::Clear()
{
    list.clear();free.clear();last=first=-1;
}


#endif // PCACHE_H

