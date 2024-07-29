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

#include "atime.h"

#include <time.h>
#include <chrono>

using namespace alt;

alt::time time::current()
{
    return time(uStamp());
}

#if !defined(linux) && !defined(__APPLE__)
#include <intrin.h>

uint64 time::systemTick()
{
    return __rdtsc();
}
#else
uint64 time::systemTick()
{
    uint64 d;
    __asm__ __volatile__ ("rdtsc" : "=A" (d) );
    return d;
}
#endif

uint64 time::uStamp()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void time::fragmentation(
        int *year,
        int *month,
        int *day,
        int *wday,
        int *hour,
        int *min,
        int *sec,
        int *usec)
{
    time_t t=stamp/1000000;
    struct tm *val=localtime(&t);

    if(year)*year=val->tm_year+1900;
    if(month)*month=val->tm_mon+1;
    if(day)*day=val->tm_mday;
    if(wday)*wday=val->tm_wday;
    if(hour)*hour=val->tm_hour;
    if(min)*min=val->tm_min;
    if(sec)*sec=val->tm_sec;
    if(usec)*usec=stamp%1000000;
}

time::time( int year,
      int month,
      int day,
      int hour,
      int min,
      int sec,
      int usec )
{
    struct tm val = {0};
    val.tm_year = year-1900;
    val.tm_mon = month-1;
    val.tm_mday = day;
    val.tm_sec = sec;
    val.tm_min = min;
    val.tm_hour = hour;
    time_t t = mktime(&val);
    stamp = t*1000000+usec;
}

string time::toString()
{
    time_t t=stamp/1000000;
    struct tm *val=localtime(&t);

    return string::fromIntFormat(val->tm_mday,2)+"."+
            string::fromIntFormat(val->tm_mon+1,2)+"."+
            string::fromIntFormat(val->tm_year+1900,4)+" "+
            string::fromIntFormat(val->tm_hour,2)+":"+
            string::fromIntFormat(val->tm_min,2)+":"+
            string::fromIntFormat(val->tm_sec,2)+"."+
            string::fromIntFormat((stamp/1000)%1000,3);
}
