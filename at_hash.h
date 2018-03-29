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

#ifndef AT_HASH_H
#define AT_HASH_H

#include "at_array.h"
#include "math_int.h"
#include "types.h"

#define ATHASH_MIN_TABCOUNT     __bsr32(sizeof(int))

//////////////////////////////////////////////////////////////////////////////////
//хэш-процедуры стандартных типов
//////////////////////////////////////////////////////////////////////////////////

__inline uint32 aHash(int32 key){ return key; }
__inline uint32 aHash(uint32 key){ return key; }
__inline uint32 aHash(int64 key){ return (key>>32)^key; }
__inline uint32 aHash(uint64 key){ return (key>>32)^key; }
__inline uint32 aHash(const char *key)
{
    if(!key)return 0;
    int i;
    uint32 rv=0;
    while(key[i])
    {
        rv=(rv>>1)|(rv<<31);
        rv^=key[i];
        i++;
    }
    return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

template <class T>
struct ATSetInternal
{
    ATArray<T>     Values;
    ATArray<int>	  *Hash;
    int tabp2p;
    int refcount;
};


template <class T>
class ATSet
{
private:

    ATSetInternal<T> *data;

    ATSetInternal<T>* newInternal(int p2p)
    {
        ATSetInternal<T> *rv;
        rv=new ATSetInternal<T>;
        rv->tabp2p=p2p;
        rv->Hash=new ATArray<int>[1<<(rv->tabp2p)];
        rv->refcount=1;
        return rv;
    }

    void deleteInternal()
    {
        data->refcount--;
        if(!data->refcount)
        {
            delete []data->Hash;
            delete data;
        }
    }

    void cloneInternal()
    {
        if(data->refcount<2)return;
        ATSetInternal<T> *tmp=newInternal(data->tabp2p);
        tmp->Values=data->Values;
        for(int i=0;i<(1<<data->tabp2p);i++)tmp->Hash[i]=data->Hash[i];
        deleteInternal();
        data=tmp;
    }

    int indexOf(const T &key) const
    {
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Values[ind]==key)return ind;
        }
        return -1;
    }

    void refactory()
    {
        int count=__bsr32(data->Values.size()/_ats_upsize(0));
        if(count<ATHASH_MIN_TABCOUNT)count=ATHASH_MIN_TABCOUNT;
        if(count!=data->tabp2p)
        {
            //проверим гестерезис
            bool mustReform=false;
            if( count<data->tabp2p && data->Values.size()<((1<<count)+(1<<count)/2) ) mustReform=true;
            if( count>data->tabp2p) mustReform=true;

            if(mustReform)
            {
                //перестраиваем таблицу
                delete []data->Hash;
                data->tabp2p=count;
                data->Hash=new ATArray<int>[(1<<data->tabp2p)];

                for(int i=0;i<data->Values.size();i++)
                {
                    unsigned int code=aHash(data->Values[i]);
                    int tab=code&((1<<data->tabp2p)-1);
                    data->Hash[tab].append(i);
                }
            }
        }
    }

    int makeEntry(const T &key)
    {
        refactory();

        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        int ind=data->Values.size();
        data->Hash[tab].append(ind);
        data->Values.append(key);
        return ind;
    }

    void removeEntry(int ind)
    {
        uint32 code=aHash(data->Values[ind]);
        int tab=code&((1<<data->tabp2p)-1);

        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int index=data->Hash[tab][i];
            if(ind==index)
            {
                data->Hash[tab].fastCut(i);
                break;
            }
        }

        //корректируем индекс переносимого элемента
        if(ind!=data->Values.size()-1)
        {
            code=aHash(data->Values.last());
            tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int index=data->Hash[tab][i];
                if((data->Values.size()-1)==index)
                {
                    data->Hash[tab][i]=ind;
                    break;
                }
            }
        }

        data->Values.fastCut(ind);

    }

public:

    ATSet()
    {
        data=newInternal(ATHASH_MIN_TABCOUNT);
    }
    ATSet(const ATSet<T> &val)
    {
        data=val.data;
        data->refcount++;
    }

    bool operator==(const ATSet<T> &val) const
    {
        if(size()!=val.size())return false;
        for(int i=0;i<val.size();i++)
        {
            if(!contains(val[i]))return false;
        }
        return true;
    }
    bool operator!=(const ATSet<T> &val) const
    {
        return !((*this)==val);
    }

    ATSet& operator=(const ATSet<T> &val)
    {
        deleteInternal();
        data=val.data;
        data->refcount++;
        return *this;
    }
    ~ATSet()
    {
        deleteInternal();
    }

    ATSet& clear()
    {
        deleteInternal();
        data=newInternal(ATHASH_MIN_TABCOUNT);
        return *this;
    }

    //операторы работы с множествами
    ATSet operator&(const ATSet<T> &val)
    {
        ATSet<T> rv;
        for(int i=0;i<size();i++)
        {
            if(val.contains(data->Values[i]))
                rv.insert(data->Values[i]);
        }
        return rv;
    }
    ATSet& operator&=(const ATSet<T> &val)
    {
        *this=(*this)&val;
        return *this;
    }

    ATSet operator-(const ATSet<T> &val)
    {
        ATSet<T> rv;
        for(int i=0;i<size();i++)
        {
            if(!val.contains(data->Values[i]))
                rv.insert(data->Values[i]);
        }
        return rv;
    }
    ATSet operator+(const ATSet<T> &val)
    { //TODO: оптимизировать по разимеру
        ATSet<T> rv=val;
        rv.insert(*this);
        return rv;
    }
    ATSet operator^(const ATSet<T> &val)
    {
        ATSet<T> rv=(*this)-val;
        for(int i=0;i<val.size();i++)
        {
            if(!contains(val[i]))
                rv.insert(val[i]);
        }
        return rv;
    }


    //работа с содержимым
    int insert(const T &val)
    {
        cloneInternal();
        int ind=indexOf(val);
        if(ind<0)ind=makeEntry(val);
        return ind;
    }

    void insert(const ATArray<T> &val)
    {
        cloneInternal();
        for(int i=0;i<val.size();i++)
        {
            int ind=indexOf(val[i]);
            if(ind<0)makeEntry(val[i]);
        }
    }

    void insert(const ATSet<T> &val)
    {
        cloneInternal();
        for(int i=0;i<val.size();i++)
        {
            int ind=indexOf(val[i]);
            if(ind<0)makeEntry(val[i]);
        }
    }

    ATSet& remove(const T &val)
    {
        cloneInternal();
        int ind=indexOf(val);
        if(ind<0)return *this;
        removeEntry(ind);
        refactory();
        return *this;
    }

    bool contains(const T &val) const
    {
        if(indexOf(val)<0)return false;
        return true;
    }

    bool isEmpty()
    {
        return !size();
    }

    //доступ к элементам
    int size() const
    {
        return data->Values.size();
    }

    ATArray<T> values() const
    {
        return data->Values;
    }

    T operator[](int ind) const
    {
        return data->Values[ind];
    }
    T& operator[](int ind)
    {
        cloneInternal();
        return data->Values[ind];
    }
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

template <class K, class V>
struct ATHashInternal
{
    ATArray<V>     Values;
    ATArray<K>	  Keys;
    ATArray<int>	  *Hash;
    int tabp2p;
    int refcount;
};

template <class K, class V>
class ATHash
{
private:

    ATHashInternal<K,V> *data;

    ATHashInternal<K,V>* newInternal(int p2p)
    {
        ATHashInternal<K,V> *rv;
        rv=new ATHashInternal<K,V>;
        rv->tabp2p=p2p;
        rv->Hash=new ATArray<int>[1<<(rv->tabp2p)];
        rv->refcount=1;
        return rv;
    }

    void deleteInternal()
    {
        data->refcount--;
        if(!data->refcount)
        {
            delete []data->Hash;
            delete data;
        }
    }

    void cloneInternal()
    {
        if(data->refcount<2)return;
        ATHashInternal<K,V> *tmp=newInternal(data->tabp2p);
        tmp->Values=data->Values;
        tmp->Keys=data->Keys;
        for(int i=0;i<(1<<data->tabp2p);i++)tmp->Hash[i]=data->Hash[i];
        deleteInternal();
        data=tmp;
    }

    void refactory()
    {
        int count=__bsr32(data->Keys.size()/_ats_upsize(0));
        if(count<ATHASH_MIN_TABCOUNT)count=ATHASH_MIN_TABCOUNT;
        if(count!=data->tabp2p)
        {
            //проверим гестерезис
            bool mustReform=false;
            if( count<data->tabp2p && data->Keys.size()<((1<<count)+(1<<count)/2) ) mustReform=true;
            if( count>data->tabp2p) mustReform=true;

            if(mustReform)
            {
                //перестраиваем таблицу
                delete []data->Hash;
                data->tabp2p=count;
                data->Hash=new ATArray<int>[(1<<data->tabp2p)];

                for(int i=0;i<data->Keys.size();i++)
                {
                    unsigned int code=aHash(data->Keys[i]);
                    int tab=code&((1<<data->tabp2p)-1);
                    data->Hash[tab].append(i);
                }
            }
        }
    }

    int makeEntry(const K &key)
    {
        refactory();

        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        int ind=data->Keys.size();
        data->Hash[tab].append(ind);
        data->Keys.append(key);
        data->Values.append(V());
        return ind;
    }

    void removeEntry(int ind)
    {
        uint32 code=aHash(data->Keys[ind]);
        int tab=code&((1<<data->tabp2p)-1);

        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int index=data->Hash[tab][i];
            if(ind==index)
            {
                data->Hash[tab].fastCut(i);
                break;
            }
        }

        //корректируем индекс переносимого элемента
        if(ind!=data->Keys.size()-1)
        {
            code=aHash(data->Keys.last());
            tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int index=data->Hash[tab][i];
                if((data->Keys.size()-1)==index)
                {
                    data->Hash[tab][i]=ind;
                    break;
                }
            }
        }

        data->Keys.fastCut(ind);
        data->Values.fastCut(ind);
    }

public:

    ATHash()
    {
        data=newInternal(ATHASH_MIN_TABCOUNT);
    }
    ATHash(const ATHash<K,V> &val)
    {
        data=val.data;
        data->refcount++;
    }
    ATHash& operator=(const ATHash<K,V> &val)
    {
        if(val.data==data)return *this;
        deleteInternal();
        data=val.data;
        data->refcount++;
        return *this;
    }
    ~ATHash()
    {
        deleteInternal();
    }

    ATHash& clear()
    {
        deleteInternal();
        data=newInternal(ATHASH_MIN_TABCOUNT);
        return *this;
    }

    bool operator==(const ATHash &val) const
    {
        if(data==val.data)return true;
        if(size()!=val.size())return false;
        for(int i=0;i<size();i++)
        {
            if(!val.contains(key(i),value(i)))return false;
        }
        return true;
    }

    bool operator!=(const ATHash &val) const
    {
        return !((*this)==val);
    }

    int indexOf(const K &key) const
    {
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==key)return ind;
        }
        return -1;
    }

    //работа с содержимым таблицы
    int insert(const K &key, const V &val)
    {
        cloneInternal();
        int ind=indexOf(key);
        if(ind<0)ind=makeEntry(key);
        data->Values[ind]=val;
        return ind;
    }

    ATHash& insert(const ATHash<K,V> &val)
    {
        cloneInternal();
        for(int i=0;i<val.size();i++)
        {
            int ind=indexOf(val.keys()[i]);
            if(ind<0)ind=makeEntry(val.keys()[i]);
            data->Values[ind]=val.values()[i];
        }
        return *this;
    }

    int insertMulty(const K &key, const V &val, bool evenFullClone=true)
    {
        int ind;
        cloneInternal();
        if(!evenFullClone)
        {
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                ind=data->Hash[tab][i];
                if(data->Keys[ind]==key && data->Values[ind]==val)return ind;
            }
        }
        ind=makeEntry(key);
        data->Values[ind]=val;
        return ind;
    }

    int insertAll(const K &key, const V &val)
    {
        int ind;
        cloneInternal();
        ind=makeEntry(key);
        data->Values[ind]=val;
        return ind;
    }

    ATHash& remove(const K &key)
    {
        cloneInternal();
        int ind=indexOf(key);
        if(ind<0)return *this;
        removeEntry(ind);
        refactory();
        return *this;
    }

    ATHash& removeByIndex(int ind)
    {
        cloneInternal();
        if(ind<size())
        {
            removeEntry(ind);
        }
        refactory();
        return *this;
    }

    ATHash& remove(const K &key, const V &val)
    {
        cloneInternal();
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==key && data->Values[ind]==val)
            {
                removeEntry(ind);
            }
        }
        refactory();
        return *this;
    }

    ATHash& removeMulty(const K &key)
    {
        cloneInternal();
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        int size=data->Hash[tab].size();
        for(int i=0;i<size;i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==key)
            {
                removeEntry(ind);
                i--;
                size--;
            }
        }
        refactory();
        return *this;
    }

    bool contains(const K &key) const
    {
        if(indexOf(key)<0)return false;
        return true;
    }

    bool contains(const ATArray<K> &keys) const
    {
        for(int i=0;i<keys.size();i++)
        {
            if(indexOf(keys[i])<0)return false;
        }
        return true;
    }

    bool contains(const K &key, const V &val) const
    {
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==key && data->Values[ind]==val)return true;
        }
        return false;
    }

    //доступ к элементам таблицы
    int size() const
    {
        return data->Keys.size();
    }
    K key(int ind) const
    {
        return data->Keys[ind];
    }
    V value(int ind) const
    {
        return data->Values[ind];
    }
    V& value_ref(int ind)
    {
        cloneInternal();
        return data->Values[ind];
    }
    V last() const
    {
        return data->Values.last();
    }

    ATArray<K> keys() const
    {
        return data->Keys;
    }
    ATArray<V> values() const
    {
        return data->Values;
    }

    ATArray<V> values(const K &key) const
    {
        ATArray<V> rv;
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==key)rv.append(data->Values[ind]);
        }
        return rv;
    }

    ATArray<int> index_list(const K &key) const
    {
        ATArray<V*> rv;
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        return data->Hash[tab];
    }

    const V& operator[](const K &key) const
    {
        int ind=indexOf(key);
        return data->Values[ind];
    }
    V& operator[](const K &key)
    {
        cloneInternal();
        int ind=indexOf(key);
        if(ind<0)ind=makeEntry(key);
        return data->Values[ind];
    }
};


//////////////////////////////////////////////////////////////////////////////////
// Хаос-таблица
//////////////////////////////////////////////////////////////////////////////////

template <class K, class V>
struct ATChaosInternal
{
    ATArray<V>     Values;
    ATArray< ATArray<K> >  Keys;

    ATArray<int>  *Hash;
    int tabp2p;
    int refcount;
};

template <class K, class V>
class ATChaos
{
private:

    ATChaosInternal<K,V> *data;
    ATArray<K> keyTemp;

    ATChaosInternal<K,V>* newInternal(int p2p)
    {
        ATChaosInternal<K,V> *rv;
        rv=new ATChaosInternal<K,V>;
        rv->tabp2p=p2p;
        rv->Hash=new ATArray<int>[((int)1)<<(rv->tabp2p)];
        rv->refcount=1;
        return rv;
    }

    void deleteInternal()
    {
        data->refcount--;
        if(!data->refcount)
        {
            delete []data->Hash;
            delete data;
        }
    }

    void cloneInternal()
    {
        if(data->refcount<2)return;
        ATChaosInternal<K,V> *tmp=newInternal(data->tabp2p);
        tmp->Values=data->Values;
        tmp->Keys=data->Keys;
        for(int i=0;i<(1<<data->tabp2p);i++)tmp->Hash[i]=data->Hash[i];
        deleteInternal();
        data=tmp;
    }

    int indexOf() const
    {
        uint32 code=aHash(keyTemp[0]);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind]==keyTemp)return ind;
        }
        return -1;
    }

    int indexOf(const K &k1, const K &k2) const
    {
        uint32 code=aHash(k1);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind].size()!=2)continue;
            if(data->Keys[ind][0]==k1 && data->Keys[ind][1]==k2)return ind;
        }
        return -1;
    }

    void refactory()
    {
        int count=__bsr32(data->Keys.size()/_ats_upsize(0));
        if(count<ATHASH_MIN_TABCOUNT)count=ATHASH_MIN_TABCOUNT;
        if(count!=data->tabp2p)
        {
            //проверим гестерезис
            bool mustReform=false;
            if( count<data->tabp2p && data->Keys.size()<((1<<count)+(1<<count)/2) ) mustReform=true;
            if( count>data->tabp2p) mustReform=true;

            if(mustReform)
            {
                //перестраиваем таблицу
                delete []data->Hash;
                data->tabp2p=count;
                data->Hash=new ATArray<int>[(1<<data->tabp2p)];

                for(int i=0;i<data->Keys.size();i++)
                {
                    for(int j=0;j<data->Keys[i].size();j++)
                    {
                        unsigned int code=aHash(data->Keys[i][j]);
                        int tab=code&((1<<data->tabp2p)-1);
                        data->Hash[tab].append(i);
                    }
                }
            }
        }
    }

    int makeEntry()
    {
        refactory();

        int ind=data->Keys.size();

        ATSet<int> tabs;
        for(int i=0;i<keyTemp.size();i++)
        {
            uint32 code=aHash(keyTemp[i]);
            int tab=code&((1<<data->tabp2p)-1);
            if(tabs.contains(tab))continue;
            tabs.insert(tab);
            data->Hash[tab].append(ind);
        }

        data->Keys.append(keyTemp);
        data->Values.append(V());
        return ind;
    }

    void removeEntry(int ind)
    {
        for(int j=0;j<data->Keys[ind].size();j++)
        {
            uint32 code=aHash(data->Keys[ind][j]);
            int tab=code&((1<<data->tabp2p)-1);

            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int index=data->Hash[tab][i];
                if(ind==index)
                {
                    data->Hash[tab].fastCut(i);
                    break;
                }
            }
        }

        //корректируем индекс переносимого элемента
        if(data->Keys.size() && ind!=data->Keys.size()-1)
        {
            for(int j=0;j<data->Keys.last().size();j++)
            {
                uint32 code=aHash(data->Keys.last()[j]);
                int tab=code&((1<<data->tabp2p)-1);
                for(int i=0;i<data->Hash[tab].size();i++)
                {
                    int index=data->Hash[tab][i];
                    if((data->Keys.size()-1)==index)
                    {
                        data->Hash[tab][i]=ind;
                        break;
                    }
                }
            }
        }

        data->Keys.fastCut(ind);
        data->Values.fastCut(ind);

    }


public:

    ATChaos()
    {
        data=newInternal(ATHASH_MIN_TABCOUNT);
    }
    ATChaos(const ATChaos<K,V> &val)
    {
        data=val.data;
        data->refcount++;
    }
    ATChaos& operator=(const ATChaos<K,V> &val)
    {
        if(val.data==data)return *this;
        deleteInternal();
        data=val.data;
        data->refcount++;
        return *this;
    }
    ~ATChaos()
    {
        deleteInternal();
    }

    ATChaos& clear()
    {
        deleteInternal();
        data=newInternal(ATHASH_MIN_TABCOUNT);
        return *this;
    }

    //работа с содержимым таблицы
    int insert(const K &key, const V &val)
    {
        cloneInternal();
        keyTemp.clear();
        keyTemp.append(key);

        int ind=indexOf();
        if(ind<0)ind=makeEntry();
        data->Values[ind]=val;
        return ind;
    }
    int insert(const K &k1, const K &k2, const V &val)
    {
        cloneInternal();
        keyTemp.clear();
        keyTemp.append(k1);
        keyTemp.append(k2);

        int ind=indexOf();
        if(ind<0)ind=makeEntry();
        data->Values[ind]=val;
        return ind;
    }

    bool contains(const K &k1, const K &k2) const
    {
        if(indexOf(k1,k2)<0)return false;
        return true;
    }

    ATArray<K> key(int ind) const
    {
        return data->Keys[ind];
    }

    V value(int ind) const
    {
        return data->Values[ind];
    }

    V& value(const K &k1, const K &k2)
    {
        cloneInternal();
        keyTemp.clear();
        keyTemp.append(k1);
        keyTemp.append(k2);

        int ind=indexOf();
        if(ind<0)ind=makeEntry();
        return data->Values[ind];
    }

    V& operator[](int ind)
    {
        cloneInternal();
        return data->Values[ind];
    }

    ATArray< ATArray<K> > keys() const
    {
        return data->Keys;
    }
    ATArray<V> values() const
    {
        return data->Values;
    }

    ATSet<K> keysWith(const K &key) const
    {
        ATSet<K> rv;
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind].contains(key))
            {
                rv.insert(data->Keys[ind]);
            }
        }
        rv.remove(key);
        return rv;
    }

    ATArray<V> valuesWith(const K &key) const
    {
        ATArray<V> rv;
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind].contains(key))
                rv.append(data->Values[ind]);
        }
        return rv;
    }

    ATArray<int> indexesWith(const K &key) const
    {
        ATArray<int> rv;
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        for(int i=0;i<data->Hash[tab].size();i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind].contains(key))rv.append(ind);
        }
        return rv;
    }

    ATChaos& removeMulty(const K &key)
    {
        uint32 code=aHash(key);
        int tab=code&((1<<data->tabp2p)-1);
        int size=data->Hash[tab].size();
        for(int i=0;i<size;i++)
        {
            int ind=data->Hash[tab][i];
            if(data->Keys[ind].contains(key))
            {
                removeEntry(ind);
                i--;
                size=data->Hash[tab].size();
            }
        }
        refactory();
        return *this;
    }

    int size() const
    {
        return data->Keys.size();
    }

};

#endif // AT_HASH_H
