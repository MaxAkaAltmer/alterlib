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

#ifndef ATHREAD_H
#define ATHREAD_H

#include "atypes.h"
#include "at_array.h"
#include "adelegate.h"
#include <atomic>

namespace alt {

    void sleep(int us);

    long long threadId();

    class thread
    {
    public:
        thread(delegate<int,void *> proc);
        thread(int (*fun)(void *));

        ~thread();

        bool run(void *data, bool loop=false);
        bool wait(int count=-1, int us=0);
        bool isOff();
        void terminate(); //вызывать только в крайнем случае!

    private:

        void *internal;

    };

    class semaphore
    {
    public:
        semaphore();
        ~semaphore();

        void lock();
        void unlock();

        bool trylock();

    private:

        void *internal = nullptr;

    };

    class shadower
    {
    public:
        shadower(){counter=0; compier=0;}

        void increment(int val=1){counter=counter+val;}
        int check()
        {
            volatile int tmp=counter;
            volatile int rv=tmp-compier;
            compier=tmp;
            return rv;
        }

        int distance()
        {
            return counter-compier;
        }

    private:
        volatile int counter,compier;
    };

    //////////////////////////////////////////////////////////////////////////////////////

    struct sharedArrayInternal
    {
        std::atomic<int> refcount = 0;
        alt::array<uint8> buffer;
    };

    class sharedArrayRef
    {
    public:
        sharedArrayRef()
        {
        }
        sharedArrayRef(sharedArrayInternal *hand)
            : handler(hand)
        {
            if(!hand)
                return;
            handler->refcount++;
        }

        sharedArrayRef(const sharedArrayRef& val)
        {
            *this = val;
        }

        ~sharedArrayRef()
        {
            if(handler)
                handler->refcount--;
        }

        sharedArrayRef& operator = (const sharedArrayRef& val)
        {
            if(handler)
                handler->refcount--;
            if(!val.handler)
            {
                handler = nullptr;
                return *this;
            }
            handler = val.handler;
            handler->refcount++;
            return *this;
        }

        void clear()
        {
            if(!handler)
                return;
            handler->refcount--;
            handler = nullptr;
        }

        const uint8& operator[](uintz ind) const
        {
            return handler->buffer[ind];
        }

        const uint8* operator()() const
        {
            if(!handler)
                return nullptr;
            return handler->buffer();
        }

        uintz size() const
        {
            if(!handler)
                return 0;
            return handler->buffer.size();
        }

        bool is_valid() const
        {
            return handler;
        }

    private:

        sharedArrayInternal *handler = nullptr;

    };

    class sharedArrays
    {
    public:
        sharedArrays();
        virtual ~sharedArrays();

        void attach();
        void dettach();
        void cleanup();

        void reserve(uint32 size, int count)
        {
            attach();
            for(int i=0;i<count;i++)
            {
                sharedArrayInternal *tmp = new sharedArrayInternal;
                tmp->buffer.resize(size,false);
                ptr->append(tmp);
            }
        }

        void allocate(uintz size)
        {
            attach();
            sharedArrayInternal *tmp = new sharedArrayInternal;
            tmp->buffer.resize(size,false);
            ptr->append(tmp);
        }

        int findIndex(uintz size)
        {
            attach();
            for(int i=0;i<ptr->size();i++)
            {
                if(!(*ptr)[i]->refcount)
                {
                    (*ptr)[i]->buffer.resize(size,false);
                    return i;
                }
            }
            allocate(size);
            return ptr->size()-1;
        }

        sharedArrayRef getReference(int index)
        {
            return sharedArrayRef((*ptr)[index]);
        }

        uint8* getPointer(int index)
        {
            return (*ptr)[index]->buffer();
        }

    protected:

        static std::atomic<int> initialized;

        static thread_local alt::array<sharedArrayInternal*> *ptr;

        semaphore mutex;
        alt::array<alt::array<sharedArrayInternal*>*> arrays;
        alt::array<alt::array<sharedArrayInternal*>*> to_remove;
        long long root_thread_id;

    };

} //namespace alt

#endif // ATHREAD_H
