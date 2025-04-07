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

#ifndef AT_HASH_H
#define AT_HASH_H

#include "at_array.h"
#include "amath_int.h"
#include "atypes.h"

#include <initializer_list>

#ifdef ALT_DEBUG_ENABLE
    #include <iostream>
#endif

namespace alt {

    const static int ATHASH_MIN_TABCOUNT = alt::imath::bsr32(sizeof(int));

//////////////////////////////////////////////////////////////////////////////////
//хэш-процедуры стандартных типов
//////////////////////////////////////////////////////////////////////////////////

    __inline uint32 aHash(int32 key){ return key; }
    __inline uint32 aHash(uint32 key){ return key; }
    __inline uint32 aHash(int64 key){ return uint32((key>>32)^key); }
    __inline uint32 aHash(uint64 key){ return uint32((key>>32)^key); }
    __inline uint32 aHash(void *key)
    {
        uintz tmp = unaligned_read<uintz>(&key);
        return uint32((tmp>>32)^tmp);
    }
    __inline uint32 aHash(const char *key)
    {
        if(!key)return 0;
        int i = 0;
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
    class set
    {
    private:

        struct Internal
        {
            array<T>     Values;
            array<int>	  *Hash;
            int tabp2p;
            int refcount;
        };

        Internal *data;

        Internal* newInternal(int p2p)
        {
            Internal *rv;
            rv=new Internal;
            rv->tabp2p=p2p;
            rv->Hash=new array<int>[1<<(rv->tabp2p)];
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
            Internal *tmp=newInternal(data->tabp2p);
            tmp->Values=data->Values;
            for(int i=0;i<(1<<data->tabp2p);i++)
                tmp->Hash[i]=data->Hash[i];
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
            int count=alt::imath::bsr32(data->Values.size()/alt::utils::upsize(0));
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
                    data->Hash=new array<int>[(1<<data->tabp2p)];

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

        set()
        {
            data=newInternal(ATHASH_MIN_TABCOUNT);
        }
        set(const set<T> &val)
        {
            data=val.data;
            data->refcount++;
        }

        explicit set(const array<T> &val)
        {
            data=newInternal(ATHASH_MIN_TABCOUNT);
            for(int i=0;i<val.size();i++)
            {
                int ind=indexOf(val[i]);
                if(ind<0)makeEntry(val[i]);
            }
        }

        bool operator==(const set<T> &val) const
        {
            if(size()!=val.size())return false;
            for(int i=0;i<val.size();i++)
            {
                if(!contains(val[i]))return false;
            }
            return true;
        }
        bool operator!=(const set<T> &val) const
        {
            return !((*this)==val);
        }

        set& operator=(const set<T> &val)
        {
            deleteInternal();
            data=val.data;
            data->refcount++;
            return *this;
        }
        ~set()
        {
            deleteInternal();
        }

        set& clear()
        {
            deleteInternal();
            data=newInternal(ATHASH_MIN_TABCOUNT);
            return *this;
        }

        //операторы работы с множествами
        set operator&(const set<T> &val)
        {
            set<T> rv;
            for(int i=0;i<size();i++)
            {
                if(val.contains(data->Values[i]))
                    rv.insert(data->Values[i]);
            }
            return rv;
        }

        bool cross(const set<T> &val)
        {
            for(int i=0;i<size();i++)
            {
                if(val.contains(data->Values[i]))
                    return true;
            }
            return false;
        }

        set& operator&=(const set<T> &val)
        {
            *this=(*this)&val;
            return *this;
        }

        set operator-(const set<T> &val)
        {
            set<T> rv;
            for(int i=0;i<size();i++)
            {
                if(!val.contains(data->Values[i]))
                    rv.insert(data->Values[i]);
            }
            return rv;
        }
        set operator+(const set<T> &val)
        {
            if(val.size()<size())
            {
                set<T> rv=*this;
                rv.insert(val);
                return rv;
            }
            set<T> rv=val;
            rv.insert(*this);
            return rv;
        }
        set operator^(const set<T> &val)
        {
            set<T> rv=(*this)-val;
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

        void insert(const array<T> &val)
        {
            cloneInternal();
            for(int i=0;i<val.size();i++)
            {
                int ind=indexOf(val[i]);
                if(ind<0)makeEntry(val[i]);
            }
        }

        void insert(const set<T> &val)
        {
            cloneInternal();
            for(int i=0;i<val.size();i++)
            {
                int ind=indexOf(val[i]);
                if(ind<0)makeEntry(val[i]);
            }
        }

        set& remove(const T &val)
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

        array<T> values() const
        {
            return data->Values;
        }

        const T& operator[](int ind) const
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

    template <class K>
    class order //based on AVL tree
    {
    private:

        struct Node
        {
            K	  key;
            uintz height = 1;
            uintz count = 1;
            Node *parent = nullptr;
            Node *left = nullptr;
            Node *right = nullptr;
        };

        struct Internal
        {
            Node *top = nullptr;
            uintz refcount = 0;
        };

        Internal *data = nullptr;

        void deleteNodes(Node *n)
        {
            if(!n)
                return;
            deleteNodes(n->right);
            deleteNodes(n->left);
        }
        void deleteInternal()
        {
            data->refcount--;
            if(!data->refcount)
            {
                deleteNodes(data->top);
                delete data;
            }
            data = nullptr;
        }

        Node* copyNodes(Node *from, Node *parent)
        {
            Node *to = nullptr;

            if(!from)
                return to;
            to = new Node;
            to->key = from->key;
            to->height = from->height;
            to->parent = parent;
            to->left = copyNodes(from->left,to);
            to->right = copyNodes(from->right,to);

            return to;
        }

        void cloneInternal()
        {
            if(data->refcount<2)return;
            Internal *tmp = new Internal;
            tmp->refcount++;

            data->top = copyNodes(data->top,nullptr);

            deleteInternal();
            data=tmp;
        }

        Node* find(Node *n, const K &key)
        {
            if(!n)
                return nullptr;
            if(n->key == key)
                return n;
            if(key < n->key)
                return find(n->left,key);
            return find(n->right,key);
        }

        Node* findByIndex(Node *n, uintz &index) const
        {
            if(!n)
                return nullptr;
            if(index>=n->count)
                return nullptr;
            if(index < (n->left?n->left->count:0))
                return findByIndex(n->left,index);
            if(index >= n->count-(n->right?n->right->count:0))
            {
                index -= n->count-(n->right?n->right->count:0);
                return findByIndex(n->right,index);
            }
            index -= (n->left?n->left->count:0);
            return n;
        }

        Node* rotateRight(Node * n)
        {
            Node *x = n->left;
            n->left = x->right;
            x->right = n;

            if(n->parent)
            {
                if(n->parent->left == n)
                    n->parent->left = x;
                else if(n->parent->right == n)
                    n->parent->right = x;
            }

            x->parent = n->parent;
            if(x->left)
                x->left->parent = x;
            if(x->right)
                x->right->parent = x;

            if(n->left)
                n->left->parent = n;
            if(n->right)
                n->right->parent = n;

            countNode(n);
            countNode(x);

            if(!x->parent)
                data->top = x;

            return x;
        }

        Node* rotateLeft(Node * n)
        {
            Node *x = n->right;
            n->right = x->left;
            x->left = n;

            if(n->parent)
            {
                if(n->parent->left == n)
                    n->parent->left = x;
                else if(n->parent->right == n)
                    n->parent->right = x;
            }

            x->parent = n->parent;
            if(x->left)
                x->left->parent = x;
            if(x->right)
                x->right->parent = x;

            if(n->left)
                n->left->parent = n;
            if(n->right)
                n->right->parent = n;

            countNode(n);
            countNode(x);

            if(!x->parent)
                data->top = x;

            return x;
        }

        void countNode(Node *n)
        {
            n->height = 1+alt::imath::max(n->left?n->left->height:0,n->right?n->right->height:0);
            n->count = 1+(n->left?n->left->count:0)+(n->right?n->right->count:0);
        }

        Node* createEntry(Node *n, const K &key)
        {
            Node *rv = nullptr;
            if(key < n->key)
            {
                if(n->left)
                {
                    rv = createEntry(n->left,key);
                    countNode(n);
                }
                else
                {
                    rv = new Node;
                    rv->key = key;
                    rv->parent = n;
                    n->left = rv;
                    countNode(n);
                    return rv;
                }
            }
            else
            {
                if(n->right)
                {
                    rv = createEntry(n->right,key);
                    countNode(n);
                }
                else
                {
                    rv = new Node;
                    rv->key = key;
                    rv->parent = n;
                    n->right = rv;
                    countNode(n);
                    return rv;
                }
            }

            int balance = checkBalance(n);

            if(balance>1)
            {
                if(checkBalance(n->left)<0)
                    rotateLeft(n->left);
                rotateRight(n);
            }
            else if(balance<-1)
            {
                if(checkBalance(n->right)>0)
                    rotateRight(n->right);
                rotateLeft(n);
            }

            return rv;
        }

        int checkBalance(Node *n)
        {
            return (n->left?n->left->height:0) - (n->right?n->right->height:0);
        }

        Node* makeEntry(const K &key)
        {
            Node* n;
            if(!data->top)
            {
                n = new Node;
                n->key = key;
                data->top = n;
            }
            else
            {
                n = createEntry(data->top,key);
                countNode(data->top);
            }
            return n;
        }

        void fixEntry(Node* n)
        {
            if(!n)
                return;
            countNode(n);

            int balance = checkBalance(n);

            if(balance>1)
            {
                if(checkBalance(n->left)<0)
                    rotateLeft(n->left);
                n = rotateRight(n);
            }
            else if(balance<-1)
            {
                if(checkBalance(n->right)>0)
                    rotateRight(n->right);
                n = rotateLeft(n);
            }

            if(n->parent)
                fixEntry(n->parent);
        }

        void removeEntry(Node* n)
        {
            if(n->left==nullptr && n->right==nullptr)
            {
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = nullptr;
                    else if(n->parent->right==n)
                        n->parent->right = nullptr;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = nullptr;
                }
            }
            else if(n->left==nullptr)
            {
                n->right->parent = n->parent;
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = n->right;
                    else if(n->parent->right==n)
                        n->parent->right = n->right;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = n->right;

                }
            }
            else if(n->right==nullptr)
            {
                n->left->parent = n->parent;
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = n->left;
                    else if(n->parent->right==n)
                        n->parent->right = n->left;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = n->left;
                }
            }
            else
            {
                int balance = checkBalance(n);
                if(balance > 0)
                {
                    Node *up;
                    Node *replacer = n->left->right;
                    if(replacer)
                    {
                        while(replacer->right) replacer = replacer->right;
                        up = replacer->parent;
                        up->right = replacer->left;
                        if(up->right)
                            up->right->parent = up;
                        replacer->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = replacer;
                            else if(n->parent->right==n)
                                n->parent->right = replacer;
                        }
                        else
                        {
                            data->top = replacer;
                        }
                        replacer->left = n->left;
                        replacer->right = n->right;
                        if(n->left)
                            n->left->parent = replacer;
                        if(n->right)
                            n->right->parent = replacer;
                    }
                    else
                    {
                        up = n->right;
                        n->left->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = n->left;
                            else if(n->parent->right==n)
                                n->parent->right = n->left;
                        }
                        else
                        {
                            data->top = n->left;
                        }
                        n->left->right = n->right;
                        n->right->parent = n->left;
                    }
                    fixEntry(up);
                }
                else
                {
                    Node *up;
                    Node *replacer = n->right->left;

                    if(replacer)
                    {
                        while(replacer->left) replacer = replacer->left;
                        up = replacer->parent;
                        up->left = replacer->right;
                        if(up->left)
                            up->left->parent = up;
                        replacer->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = replacer;
                            else if(n->parent->right==n)
                                n->parent->right = replacer;
                        }
                        else
                        {
                            data->top = replacer;
                        }
                        replacer->left = n->left;
                        replacer->right = n->right;

                        if(n->left)
                            n->left->parent = replacer;
                        if(n->right)
                            n->right->parent = replacer;
                    }
                    else
                    {
                        up = n->left;
                        n->right->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = n->right;
                            else if(n->parent->right==n)
                                n->parent->right = n->right;
                        }
                        else
                        {
                            data->top = n->right;
                        }
                        n->right->left = n->left;
                        n->left->parent = n->right;
                    }
                    fixEntry(up);
                }
            }

            delete n;
        }

#ifdef ALT_DEBUG_ENABLE
        void printNode(Node *n)
        {
            if(!n)
                return;
            std::cout << "{" << n->key() << ",";
            printNode(n->left);
            std::cout << ",";
            printNode(n->right);
            std::cout << "}";
        }
#endif

    public:

#ifdef ALT_DEBUG_ENABLE
        void print()
        {
            printNode(data->top);
            std::cout << std::endl;
        }
#endif

        order()
        {
            data = new Internal;
            data->refcount++;
        }
        order(const order<K> &val)
        {
            data=val.data;
            data->refcount++;
        }
        order& operator=(const order<K> &val)
        {
            if(val.data==data)
                return *this;
            deleteInternal();
            data=val.data;
            data->refcount++;
            return *this;
        }
        ~order()
        {
            deleteInternal();
        }

        order& clear()
        {
            deleteInternal();
            data = new Internal;
            data->refcount++;
            return *this;
        }

        uintz size() const
        {
            if(data->top)
                return data->top->count;
            return 0;
        }

        uintz height() const
        {
            if(data->top)
                return data->top->height;
            return 0;
        }

        bool operator==(const order &val) const
        {
            if(data==val.data)return true;
            if(size()!=val.size())return false;
            for(int i=0;i<size();i++)
            {
                if(!val.contains((*this)[i]))return false;
            }
            return true;
        }

        bool operator!=(const order &val) const
        {
            return !((*this)==val);
        }

        bool contains(const K &key) const
        {
            return find(data->top,key);
        }

        intz indexOf(const K &key) const
        {
            Node *n = find(data->top,key);
            if(!n)
                return -1;
            intz off = n->left?n->left->count:0;

            while(n->parent)
            {
                if(n == n->parent->right)
                    off+=(n->parent->count-n->count);
                n = n->parent;
            }

            return off;
        }

        order& insert(const K &key)
        {
            cloneInternal();
            Node *n=find(data->top,key);
            if(!n)
                n=makeEntry(key);

            return *this;
        }

        const K& operator[](intz ind) const
        {
            uintz off = ind;
            return findByIndex(data->top,off)->key;
        }

        void removeByIndex(intz ind)
        {
            uintz off = ind;
            Node *n = findByIndex(data->top,off);
            if(n)
                removeEntry(n);
        }

        void remove(const K &key)
        {
            Node *n=find(data->top,key);
            if(n)
                removeEntry(n);
        }

    };

    /////////////////////////////////////////////////////////////////////////////////////////

    template <class K, class V>
    class map //based on AVL tree
    {
    private:

        struct ListNode
        {
            V value;
            ListNode *next = nullptr;
        };

        struct Node
        {
            ListNode  values; //todo: on pointer for avoid of recreation
            K	  key;
            uintz height = 1;
            uintz count = 1;
            Node *parent = nullptr;
            Node *left = nullptr;
            Node *right = nullptr;
        };

        struct Internal
        {
            Node *top = nullptr;
            uintz refcount = 0;
        };

        Internal *data = nullptr;

        void deleteList(ListNode *n)
        {
            if(n->next)
                deleteList(n->next);
            delete n;
        }

        void deleteNodes(Node *n)
        {
            if(!n)
                return;
            if(n->values.next)
                deleteList(n->values.next);
            deleteNodes(n->right);
            deleteNodes(n->left);
        }
        void deleteInternal()
        {
            data->refcount--;
            if(!data->refcount)
            {
                deleteNodes(data->top);
                delete data;
            }
            data = nullptr;
        }

        int findList(ListNode *n, const V &val, int off = 0)
        {
            if(!n)
                return -1;
            if(n->value == val)
                return off;
            return findList(n->next,val,off+1);
        }

        int countList(ListNode *n)
        {
            if(!n)
                return 0;
            return 1+countList(n->next);
        }

        ListNode* copyList(ListNode *from)
        {
            ListNode *to = nullptr;
            if(!from)
                return to;
            to = new ListNode;
            to->value = from->value;
            to->next = copyList(from->next);
            return to;
        }

        Node* copyNodes(Node *from, Node *parent)
        {
            Node *to = nullptr;

            if(!from)
                return to;
            to = new Node;
            to->key = from->key;
            to->height = from->height;
            to->values.value = from->values.value;
            to->values.next = copyList(from->values.next);
            to->parent = parent;
            to->left = copyNodes(from->left,to);
            to->right = copyNodes(from->right,to);

            return to;
        }

        void cloneInternal()
        {
            if(data->refcount<2)return;
            Internal *tmp = new Internal;
            tmp->refcount++;

            tmp->top = copyNodes(data->top,nullptr);

            deleteInternal();
            data=tmp;
        }

        Node* find(Node *n, const K &key) const
        {
            if(!n)
                return nullptr;
            if(n->key == key)
                return n;
            if(key < n->key)
                return find(n->left,key);
            return find(n->right,key);
        }

        Node* findByIndex(Node *n, uintz &index) const
        {
            if(!n)
                return nullptr;
            if(index>=n->count)
                return nullptr;
            if(index < (n->left?n->left->count:0))
                return findByIndex(n->left,index);
            if(index >= n->count-(n->right?n->right->count:0))
            {
                index -= n->count-(n->right?n->right->count:0);
                return findByIndex(n->right,index);
            }
            index -= (n->left?n->left->count:0);
            return n;
        }

        Node* rotateRight(Node * n)
        {
            Node *x = n->left;
            n->left = x->right;
            x->right = n;

            if(n->parent)
            {
                if(n->parent->left == n)
                    n->parent->left = x;
                else if(n->parent->right == n)
                    n->parent->right = x;
            }

            x->parent = n->parent;
            if(x->left)
                x->left->parent = x;
            if(x->right)
                x->right->parent = x;

            if(n->left)
                n->left->parent = n;
            if(n->right)
                n->right->parent = n;

            countNode(n);
            countNode(x);

            if(!x->parent)
                data->top = x;

            return x;
        }

        Node* rotateLeft(Node * n)
        {
            Node *x = n->right;
            n->right = x->left;
            x->left = n;

            if(n->parent)
            {
                if(n->parent->left == n)
                    n->parent->left = x;
                else if(n->parent->right == n)
                    n->parent->right = x;
            }

            x->parent = n->parent;
            if(x->left)
                x->left->parent = x;
            if(x->right)
                x->right->parent = x;

            if(n->left)
                n->left->parent = n;
            if(n->right)
                n->right->parent = n;

            countNode(n);
            countNode(x);

            if(!x->parent)
                data->top = x;

            return x;
        }

        void countNode(Node *n)
        {
            n->height = 1+alt::imath::max(n->left?n->left->height:0,n->right?n->right->height:0);
            n->count = countList(&n->values)+(n->left?n->left->count:0)+(n->right?n->right->count:0);
        }

        Node* createEntry(Node *n, const K &key)
        {
            Node *rv = nullptr;
            if(key < n->key)
            {
                if(n->left)
                {
                    rv = createEntry(n->left,key);
                    countNode(n);
                }
                else
                {
                    rv = new Node;
                    rv->key = key;
                    rv->parent = n;
                    n->left = rv;
                    countNode(n);
                    return rv;
                }
            }
            else
            {
                if(n->right)
                {
                    rv = createEntry(n->right,key);
                    countNode(n);
                }
                else
                {
                    rv = new Node;
                    rv->key = key;
                    rv->parent = n;
                    n->right = rv;
                    countNode(n);
                    return rv;
                }
            }

            int balance = checkBalance(n);

            if(balance>1)
            {
                if(checkBalance(n->left)<0)
                    rotateLeft(n->left);
                rotateRight(n);
            }
            else if(balance<-1)
            {
                if(checkBalance(n->right)>0)
                    rotateRight(n->right);
                rotateLeft(n);
            }

            return rv;
        }

        int checkBalance(Node *n)
        {
            return (n->left?n->left->height:0) - (n->right?n->right->height:0);
        }

        Node* makeEntry(const K &key)
        {
            Node* n;
            if(!data->top)
            {
                n = new Node;
                n->key = key;
                data->top = n;
            }
            else
            {
                n = createEntry(data->top,key);
                countNode(data->top);
            }
            return n;
        }

        void fixEntry(Node* n)
        {
            if(!n)
                return;
            countNode(n);

            int balance = checkBalance(n);

            if(balance>1)
            {
                if(checkBalance(n->left)<0)
                    rotateLeft(n->left);
                n = rotateRight(n);
            }
            else if(balance<-1)
            {
                if(checkBalance(n->right)>0)
                    rotateRight(n->right);
                n = rotateLeft(n);
            }

            if(n->parent)
                fixEntry(n->parent);
        }

        void removeEntry(Node* n)
        {
            if(n->values.next)
                deleteList(n->values.next);
            if(n->left==nullptr && n->right==nullptr)
            {
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = nullptr;
                    else if(n->parent->right==n)
                        n->parent->right = nullptr;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = nullptr;
                }
            }
            else if(n->left==nullptr)
            {
                n->right->parent = n->parent;
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = n->right;
                    else if(n->parent->right==n)
                        n->parent->right = n->right;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = n->right;

                }
            }
            else if(n->right==nullptr)
            {
                n->left->parent = n->parent;
                if(n->parent)
                {
                    if(n->parent->left==n)
                        n->parent->left = n->left;
                    else if(n->parent->right==n)
                        n->parent->right = n->left;
                    fixEntry(n->parent);
                }
                else
                {
                    data->top = n->left;
                }
            }
            else
            {
                int balance = checkBalance(n);
                if(balance > 0)
                {
                    Node *up;
                    Node *replacer = n->left->right;
                    if(replacer)
                    {
                        while(replacer->right) replacer = replacer->right;
                        up = replacer->parent;
                        up->right = replacer->left;
                        if(up->right)
                            up->right->parent = up;
                        replacer->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = replacer;
                            else if(n->parent->right==n)
                                n->parent->right = replacer;
                        }
                        else
                        {
                            data->top = replacer;
                        }
                        replacer->left = n->left;
                        replacer->right = n->right;
                        if(n->left)
                            n->left->parent = replacer;
                        if(n->right)
                            n->right->parent = replacer;
                    }
                    else
                    {
                        up = n->right;
                        n->left->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = n->left;
                            else if(n->parent->right==n)
                                n->parent->right = n->left;
                        }
                        else
                        {
                            data->top = n->left;
                        }
                        n->left->right = n->right;
                        n->right->parent = n->left;
                    }
                    fixEntry(up);
                }
                else
                {
                    Node *up;
                    Node *replacer = n->right->left;

                    if(replacer)
                    {
                        while(replacer->left) replacer = replacer->left;
                        up = replacer->parent;
                        up->left = replacer->right;
                        if(up->left)
                            up->left->parent = up;
                        replacer->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = replacer;
                            else if(n->parent->right==n)
                                n->parent->right = replacer;
                        }
                        else
                        {
                            data->top = replacer;
                        }
                        replacer->left = n->left;
                        replacer->right = n->right;

                        if(n->left)
                            n->left->parent = replacer;
                        if(n->right)
                            n->right->parent = replacer;
                    }
                    else
                    {
                        up = n->left;
                        n->right->parent = n->parent;
                        if(n->parent)
                        {
                            if(n->parent->left==n)
                                n->parent->left = n->right;
                            else if(n->parent->right==n)
                                n->parent->right = n->right;
                        }
                        else
                        {
                            data->top = n->right;
                        }
                        n->right->left = n->left;
                        n->left->parent = n->right;
                    }
                    fixEntry(up);
                }
            }

            delete n;
        }

#ifdef ALT_DEBUG_ENABLE
        void printNode(Node *n)
        {
            if(!n)
                return;
            std::cout << "{" << n->key() << ",";
            printNode(n->left);
            std::cout << ",";
            printNode(n->right);
            std::cout << "}";
        }
#endif

    public:

#ifdef ALT_DEBUG_ENABLE
        void print()
        {
            printNode(data->top);
            std::cout << std::endl;
        }
#endif

        map()
        {
            data = new Internal;
            data->refcount++;
        }
        map(const map<K,V> &val)
        {
            data=val.data;
            data->refcount++;
        }
        map& operator=(const map<K,V> &val)
        {
            if(val.data==data)
                return *this;
            deleteInternal();
            data=val.data;
            data->refcount++;
            return *this;
        }
        ~map()
        {
            deleteInternal();
        }

        map& clear()
        {
            deleteInternal();
            data = new Internal;
            data->refcount++;
            return *this;
        }

        uintz size() const
        {
            if(data->top)
                return data->top->count;
            return 0;
        }

        uintz height() const
        {
            if(data->top)
                return data->top->height;
            return 0;
        }

        V& operator[](const K &key)
        {
            cloneInternal();
            Node *n=find(data->top,key);
            if(!n)
                n=makeEntry(key);
            return n->values.value;
        }

        const V& operator[](const K &key) const
        {
            Node *n=find(data->top,key);
            return n->values.value;
        }

        bool operator==(const map &val) const
        {
            if(data==val.data)return true;
            if(size()!=val.size())return false;
            for(int i=0;i<size();i++)
            {
                if(!val.contains(key(i),value(i)))return false;
            }
            return true;
        }

        bool operator!=(const map &val) const
        {
            return !((*this)==val);
        }

        bool contains(const K &key) const
        {
            return find(data->top,key);
        }

        bool contains(const K &key, const V &val) const
        {
            Node *n = find(data->top,key);
            if(!n)
                return false;
            return findList(&n->values,val) >= 0;
        }

        intz indexOf(const K &key) const
        {
            Node *n = find(data->top,key);
            if(!n)
                return -1;
            intz off = n->left?n->left->count:0;

            while(n->parent)
            {
                if(n == n->parent->right)
                    off+=(n->parent->count-n->count);
                n = n->parent;
            }

            return off;
        }

        map& insert(const K &key, const V &val)
        {
            cloneInternal();
            Node *n=find(data->top,key);
            if(!n)
                n=makeEntry(key);
            n->values.value = val;

            return *this;
        }

        map& insertMulti(const K &key, const V &val, bool evenFullClone=true)
        {
            cloneInternal();
            Node *n=find(data->top,key);
            if(!n)
            {
                n=makeEntry(key);
                n->values.value = val;
                return *this;
            }

            int off = -1;
            if(!evenFullClone)
                off = findList(n, val);
            if(off<0)
            {
                ListNode *next = n->values.next;
                n->values.next = new ListNode;
                n->values.next->next = next;
                n->values.value = val;
            }
            return *this;
        }

        const K& key(intz ind) const
        {
            uintz off = ind;
            return findByIndex(data->top,off)->key;
        }

        V& value(intz ind)
        {
            cloneInternal();
            uintz off = ind;
            Node *n = findByIndex(data->top,off);
            ListNode *l = &n->values;
            while(off)
            {
                l = l->next;
                off--;
            }
            return l->value;
        }

        const V& value(intz ind) const
        {
            uintz off = ind;
            Node *n = findByIndex(data->top,off);
            ListNode *l = &n->values;
            while(off)
            {
                l = l->next;
                off--;
            }
            return l->value;
        }

        void removeByIndex(intz ind)
        {
            cloneInternal();
            uintz off = ind;
            Node *n = findByIndex(data->top,off);
            if(!n->values.next)
            {
                removeEntry(n);
                return;
            }
            else if(off<2)
            {
                ListNode *l = n->values.next;
                if(!off)n->values.value = l->value;
                n->values.next = l->next;
                delete l;
            }
            else
            {
                ListNode *l = n->values.next;
                off--;
                while(off>1)
                {
                    l=l->next;
                    off--;
                }
                if(!l->next)
                {
                    delete l->next;
                    l->next = nullptr;
                }
                else
                {
                    ListNode *tmp = l->next->next;
                    delete  l->next;
                    l->next = tmp;
                }
            }
        }

        void remove(const K &key)
        {
            cloneInternal();
            Node *n=find(data->top,key);
            if(n)
                removeEntry(n);
        }

    };

    /////////////////////////////////////////////////////////////////////////////////////////


    template <class K, class V>
    class hash
    {
    private:

        struct Internal
        {
            array<V>     Values;
            array<K>	  Keys;
            array<int>	  *Hash;
            int tabp2p;
            int refcount;
        };

        Internal *data;

        Internal* newInternal(int p2p)
        {
            Internal *rv;
            rv=new Internal;
            rv->tabp2p=p2p;
            rv->Hash=new array<int>[1ull<<(rv->tabp2p)];
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
            Internal *tmp=newInternal(data->tabp2p);
            tmp->Values=data->Values;
            tmp->Keys=data->Keys;
            for(int i=0;i<(1<<data->tabp2p);i++)tmp->Hash[i]=data->Hash[i];
            deleteInternal();
            data=tmp;
        }

        void refactory()
        {
            int count=alt::imath::bsr32(data->Keys.size()/alt::utils::upsize(0));
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
                    data->Hash=new array<int>[(1ull<<data->tabp2p)];

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

        hash()
        {
            data=newInternal(ATHASH_MIN_TABCOUNT);
        }
        hash(const hash<K,V> &val)
        {
            data=val.data;
            data->refcount++;
        }

        hash(std::initializer_list<pair<K,V>> list)
        {
            data=newInternal(ATHASH_MIN_TABCOUNT);
            for(auto it: list)
                insert(it.left(),it.right());
        }

        hash& operator=(const hash<K,V> &val)
        {
            if(val.data==data)return *this;
            deleteInternal();
            data=val.data;
            data->refcount++;
            return *this;
        }
        ~hash()
        {
            deleteInternal();
        }

        hash& clear()
        {
            deleteInternal();
            data=newInternal(ATHASH_MIN_TABCOUNT);
            return *this;
        }

        bool operator==(const hash &val) const
        {
            if(data==val.data)return true;
            if(size()!=val.size())return false;
            for(int i=0;i<size();i++)
            {
                if(!val.contains(key(i),value(i)))return false;
            }
            return true;
        }

        bool operator!=(const hash &val) const
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

        hash& insert(const hash<K,V> &val)
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
                    if(data->Keys[ind]==key && data->Values[ind]==val)
                        return ind;
                }
            }
            ind=makeEntry(key);
            data->Values[ind]=val;
            return ind;
        }

        hash& remove(const K &key)
        {
            cloneInternal();
            int ind=indexOf(key);
            if(ind<0)return *this;
            removeEntry(ind);
            refactory();
            return *this;
        }

        hash& removeByIndex(int ind)
        {
            cloneInternal();
            if(ind<size() && ind>=0)
            {
                removeEntry(ind);
                refactory();
            }
            return *this;
        }

        hash& remove(const K &key, const V &val, bool all = true)
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
                    if(!all) break;
                }
            }
            refactory();
            return *this;
        }

        hash& removeMulty(const K &key)
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

        bool contains(const array<K> &keys) const
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
        const K& key(int ind) const
        {
            return data->Keys[ind];
        }
        const V& value(int ind) const
        {
            return data->Values[ind];
        }
        V& value_ref(int ind)
        {
            cloneInternal();
            return data->Values[ind];
        }
        const V& last() const
        {
            return data->Values.last();
        }

        const array<K>& keys() const
        {
            return data->Keys;
        }
        const array<V>& values() const
        {
            return data->Values;
        }

        array<V> values(const K &key) const
        {
            array<V> rv;
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind]==key)
                    rv.append(data->Values[ind]);
            }
            return rv;
        }

        const array<int>& index_list_mesh(const K &key) const
        {
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

        array<int> indexes(const K &key) const
        {
            array<int> rv;
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind] == key)
                    rv.append(ind);
            }
            return rv;
        }
    };


    //////////////////////////////////////////////////////////////////////////////////
    // Хэш-таблица с проддержкой множества ключей к значению
    //////////////////////////////////////////////////////////////////////////////////

    template <class K, class V>
    class multikey
    {
    private:

        struct Internal
        {
            array<V>     Values;
            array< array<K> >  Keys;

            array<int>  *Hash;
            int tabp2p;
            int refcount;
        };

        Internal *data;

        Internal* newInternal(int p2p)
        {
            Internal *rv;
            rv=new Internal;
            rv->tabp2p=p2p;
            rv->Hash=new array<int>[((int)1)<<(rv->tabp2p)];
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
            Internal *tmp=newInternal(data->tabp2p);
            tmp->Values=data->Values;
            tmp->Keys=data->Keys;
            for(int i=0;i<(1<<data->tabp2p);i++)
                tmp->Hash[i]=data->Hash[i];
            deleteInternal();
            data=tmp;
        }

        void refactory()
        {
            int count=alt::imath::bsr32(data->Keys.size()/alt::utils::upsize(0));
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
                    data->Hash=new array<int>[(1<<data->tabp2p)];

                    for(int i=0;i<data->Keys.size();i++)
                    {
                        for(int j=0;j<data->Keys[i].size() || !j;j++)
                        {
                            unsigned int code=data->Keys[i].size()?aHash(data->Keys[i][j]):0;
                            int tab=code&((1<<data->tabp2p)-1);
                            data->Hash[tab].append(i);
                        }
                    }
                }
            }
        }

        int makeEntry(const array<K> &keyTemp)
        {
            refactory();

            int ind=data->Keys.size();

            set<int> tabs;
            for(int i=0; i<keyTemp.size() || !i; i++)
            {
                uint32 code=keyTemp.size()?aHash(keyTemp[i]):0;
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
            for(int j=0; j<data->Keys[ind].size() || !j; j++)
            {
                uint32 code=data->Keys[ind].size()?aHash(data->Keys[ind][j]):0;
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
                for(int j=0; j<data->Keys.last().size() || !j; j++)
                {
                    uint32 code=data->Keys.last().size()?aHash(data->Keys.last()[j]):0;
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

        multikey()
        {
            data=newInternal(ATHASH_MIN_TABCOUNT);
        }
        multikey(const multikey<K,V> &val)
        {
            data=val.data;
            data->refcount++;
        }
        multikey& operator=(const multikey<K,V> &val)
        {
            if(val.data==data)
                return *this;
            deleteInternal();
            data=val.data;
            data->refcount++;
            return *this;
        }
        ~multikey()
        {
            deleteInternal();
        }

        multikey& clear()
        {
            deleteInternal();
            data=newInternal(ATHASH_MIN_TABCOUNT);
            return *this;
        }

        bool operator==(const multikey &val) const
        {
            if(data==val.data)return true;
            if(size()!=val.size())return false;
            for(int i=0;i<size();i++)
            {
                if(!val.contains(key(i),value(i)))return false;
            }
            return true;
        }

        bool operator!=(const multikey &val) const
        {
            return !((*this)==val);
        }

        int indexOf(const array<K>& keyTemp) const
        {
            uint32 code=keyTemp.size()?aHash(keyTemp[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind]==keyTemp)
                    return ind;
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
                if(data->Keys[ind].size()!=2)
                    continue;
                if(data->Keys[ind][0]==k1 && data->Keys[ind][1]==k2)
                    return ind;
            }
            return -1;
        }

        int indexOf(const K &key) const
        {
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size()!=1)
                    continue;
                if(data->Keys[ind][0]==key)
                    return ind;
            }
            return -1;
        }

        //работа с содержимым таблицы
        int insert(const K &key, const V &val)
        {
            array<K> keyTemp(1);
            keyTemp[0] = key;
            return insert(keyTemp,val);
        }
        int insert(const K &k1, const K &k2, const V &val)
        {
            array<K> keyTemp(2);
            keyTemp[0] = k1;
            keyTemp[1] = k2;
            return insert(keyTemp,val);
        }
        int insert(const array<K>& key, const V &val)
        {
            cloneInternal();
            int ind=indexOf(key);
            if(ind<0)ind=makeEntry(key);
            data->Values[ind]=val;
            return ind;
        }

        int insertMulty(const K& key, const V &val, bool evenFullClone=true)
        {
            array<K> keyTemp(1);
            keyTemp[0] = key;
            return insertMulty(keyTemp,val,evenFullClone);
        }

        int insertMulty(const K& k1, const K& k2, const V &val, bool evenFullClone=true)
        {
            array<K> keyTemp(2);
            keyTemp[0] = k1;
            keyTemp[1] = k2;
            return insertMulty(keyTemp,val,evenFullClone);
        }

        int insertMulty(const array<K>& key, const V &val, bool evenFullClone=true)
        {
            cloneInternal();
            if(!evenFullClone)
            {
                uint32 code=key.size()?aHash(key[0]):0;
                int tab=code&((1<<data->tabp2p)-1);
                for(int i=0;i<data->Hash[tab].size();i++)
                {
                    int ind=data->Hash[tab][i];
                    if(data->Keys[ind] == key && data->Values[ind] == val)
                        return ind;
                }
            }
            int ind=indexOf(key);
            if(ind<0)ind=makeEntry(key);
            data->Values[ind]=val;
            return ind;
        }

        multikey& removeByIndex(int ind)
        {
            cloneInternal();
            removeEntry(ind);
            refactory();
            return *this;
        }

        multikey& remove(const K& key)
        {
            cloneInternal();
            int ind=indexOf(key);
            if(ind<0) return *this;
            removeEntry(ind);
            refactory();
            return *this;
        }

        multikey& remove(const K& k1, const K& k2)
        {
            cloneInternal();
            int ind=indexOf(k1,k2);
            if(ind<0) return *this;
            removeEntry(ind);
            refactory();
            return *this;
        }

        multikey& remove(const array<K>& key)
        {
            cloneInternal();
            int ind=indexOf(key);
            if(ind<0) return *this;
            removeEntry(ind);
            refactory();
            return *this;
        }

        multikey& remove(const array<K>& key, const V &val, bool all = true)
        {
            cloneInternal();
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind] == key && data->Values[ind] == val)
                {
                    removeEntry(ind);
                    if(!all) break;
                    i--;
                    size=data->Hash[tab].size();
                }
            }
            refactory();
            return *this;
        }

        multikey& removeMulty(const K& key)
        {
            cloneInternal();
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size() == 1 && data->Keys[ind][0] == key)
                {
                    removeEntry(ind);
                    i--;
                    size=data->Hash[tab].size();
                }
            }
            refactory();
            return *this;
        }

        multikey& removeMulty(const K& k1, const K& k2)
        {
            cloneInternal();
            uint32 code=aHash(k1);
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size() == 2 && data->Keys[ind][0] == k1 && data->Keys[ind][1] == k2)
                {
                    removeEntry(ind);
                    i--;
                    size=data->Hash[tab].size();
                }
            }
            refactory();
            return *this;
        }

        multikey& removeMulty(const array<K>& key)
        {
            cloneInternal();
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind] == key)
                {
                    removeEntry(ind);
                    i--;
                    size=data->Hash[tab].size();
                }
            }
            refactory();
            return *this;
        }

        multikey& removeWith(const K &key)
        {
            cloneInternal();
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

        bool contains(const K &k1) const
        {
            if(indexOf(k1)<0)return false;
            return true;
        }

        bool contains(const K &k1, const K &k2) const
        {
            if(indexOf(k1,k2)<0)return false;
            return true;
        }

        bool contains(const array<K>& key) const
        {
            if(indexOf(key)<0)return false;
            return true;
        }

        bool contains(const array<K>& key, const V &val) const
        {
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind] == key && data->Values[ind] == val)
                {
                    return true;
                }
            }
            return false;
        }

        bool contains_unordered(const K &k1, const K &k2) const
        {
            uint32 code=aHash(k1);
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size()==2 && data->Keys[ind].contains(k1) && data->Keys[ind].contains(k2))
                {
                    if(k1==k2 && data->Keys[ind][0]!=data->Keys[ind][1])
                        return false;
                    return true;
                }
            }
            return false;
        }

        bool contains_unordered(const set<K>& key) const
        {
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            int size=data->Hash[tab].size();
            for(int i=0;i<size;i++)
            {
                int ind=data->Hash[tab][i];
                if(set<K>(data->Keys[ind]) == key)
                {
                    return true;
                }
            }
            return false;
        }

        int size() const
        {
            return data->Keys.size();
        }
        const array<K>& key(int ind) const
        {
            return data->Keys[ind];
        }
        const V& value(int ind) const
        {
            return data->Values[ind];
        }
        V& value_ref(int ind)
        {
            cloneInternal();
            return data->Values[ind];
        }
        const V& last() const
        {
            return data->Values.last();
        }

        const array<array<K>>& keys() const
        {
            return data->Keys;
        }
        const array<V>& values() const
        {
            return data->Values;
        }

        const array<int>& index_list_mesh(const K &key) const
        {
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            return data->Hash[tab];
        }

        V& operator[](const array<K> &key)
        {
            cloneInternal();
            int ind=indexOf(key);
            if(ind<0)ind=makeEntry(key);
            return data->Values[ind];
        }

        const V& operator[](const array<K> &key) const
        {
            int ind=indexOf(key);
            return data->Values[ind];
        }

        V& operator[](std::initializer_list<K> list)
        {
            cloneInternal();
            int i=0;
            array<K> key(list.size());
            for(auto it: list)
            {
                key[i] = it;
                i++;
            }
            int ind=indexOf(key);
            if(ind<0)ind=makeEntry(key);
            return data->Values[ind];
        }

        const V& operator[](std::initializer_list<K> list) const
        {
            int ind = -1;
            if(list.size() == 1 )
            {
                ind = indexOf(*list.begin());
            }
            else if(list.size() == 2)
            {
                ind = indexOf(*list.begin(),*(list.begin()+1));
            }
            else
            {
                int i=0;
                array<K> key(list.size());
                for(auto it: list)
                {
                    key[i] = it;
                    i++;
                }
                ind=indexOf(key);
            }
            return data->Values[ind];
        }

        array<int> indexes(const array<K> &key) const
        {
            array<int> rv;
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind] == key)
                    rv.append(ind);
            }
            return rv;
        }

        array<int> indexes(const K &k1, const K &k2) const
        {
            array<int> rv;
            uint32 code=aHash(k1);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size() == 2 && data->Keys[ind][0] == k1 && data->Keys[ind][1] == k2)
                    rv.append(ind);
            }
            return rv;
        }

        array<int> indexes(const K &key) const
        {
            array<int> rv;
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size() == 1 && data->Keys[ind][0] == key)
                    rv.append(ind);
            }
            return rv;
        }

        array<int> indexes_unordered(const K &k1, const K &k2) const
        {
            array<int> rv;
            uint32 code=aHash(k1);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(data->Keys[ind].size() == 2 && data->Keys[ind].contains(k1) && data->Keys[ind].contains(k2))
                {
                    if(!(k1 == k2 && data->Keys[ind][0] != data->Keys[ind][1]))
                        rv.append(ind);
                }
            }
            return rv;
        }

        array<int> indexes_unordered(const set<K> &key) const
        {
            array<int> rv;
            uint32 code=key.size()?aHash(key[0]):0;
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(set<K>(data->Keys[ind]) == key)
                    rv.append(ind);
            }
            return rv;
        }

        set<K> keysWith(const K &key) const
        {
            set<K> rv;
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

        array<V> valuesWith(const K &key, int at = -1) const
        {
            array<V> rv;
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(at>=0)
                {
                    if(data->Keys[ind].size()>at && data->Keys[ind][at] == key)
                        rv.append(data->Values[ind]);
                }
                else
                {
                    if(data->Keys[ind].contains(key))
                        rv.append(data->Values[ind]);
                }
            }
            return rv;
        }

        array<int> indexesWith(const K &key, int at = -1) const
        {
            array<int> rv;
            uint32 code=aHash(key);
            int tab=code&((1<<data->tabp2p)-1);
            for(int i=0;i<data->Hash[tab].size();i++)
            {
                int ind=data->Hash[tab][i];
                if(at>=0)
                {
                    if(data->Keys[ind].size()>at && data->Keys[ind][at] == key)
                        rv.append(ind);
                }
                else
                {
                    if(data->Keys[ind].contains(key))
                        rv.append(ind);
                }
            }
            return rv;
        }

    };

} // namespace alt

#endif // AT_HASH_H
