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

#ifndef ATIME_H
#define ATIME_H

#include "astring.h"

namespace alt {

    class time
    {
    public:
        time(uint64 us=0)
        {
            stamp=us;
        }

        time( int year,
              int month,
              int day,
              int hour,
              int min,
              int sec,
              int usec = 0 );

        void addSeconds(uint64 seconds)
        {
            stamp += seconds*1000000;
        }

        time(const time &val){*this=val;}

        time& operator=(const time &val)
        {
            stamp=val.stamp;
            return *this;
        }

        uint64 uSeconds(){return stamp;}

        void fragmentation(int *year=NULL,
                int *month=NULL,
                int *day=NULL,
                int *wday=NULL,
                int *hour=NULL,
                int *min=NULL,
                int *sec=NULL,
                int *usec=NULL);

        static time current();
        static uint64 uStamp();
        static uint64 systemTick();

        string toString(bool full = true);

    private:

        uint64 stamp;
    };

}

#endif // ATIME_H
