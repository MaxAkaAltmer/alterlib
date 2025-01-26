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

#include "adelegate.h"

namespace alt {

    void sleep(int us);

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

        void *internal;

    };

    class shadower
    {
    public:
        shadower(){counter=compier=0;}

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
            return compier-compier;
        }

    private:
        volatile int counter,compier;
    };

} //namespace alt

#endif // ATHREAD_H
