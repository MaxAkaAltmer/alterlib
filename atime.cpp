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

#include "atime.h"

#include <time.h>
#if defined(_MSC_VER)
#include <windows.h>
static const unsigned __int64 epoch = ((unsigned __int64) 116444736000000000ULL);
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER ularge;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    tp->tv_sec = (long) ((ularge.QuadPart - epoch) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);

    return 0;
}
#else
#include <sys/time.h>
#endif

ATime ATime::current()
{
    return ATime(uStamp());
}

uint64 ATime::uStamp()
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    if(sizeof(currentTime.tv_sec)==8)
        return (uint64)currentTime.tv_sec*(uint64)1000000+currentTime.tv_usec;

    return ((uint32)currentTime.tv_sec)*(uint64)1000000+currentTime.tv_usec;
}

void ATime::fragmentation(
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

AString ATime::toString()
{
    time_t t=stamp/1000000;
    struct tm *val=localtime(&t);

    return AString::fromIntFormat(val->tm_mday,2)+"."+
            AString::fromIntFormat(val->tm_mon+1,2)+"."+
            AString::fromIntFormat(val->tm_year+1900,4)+" "+
            AString::fromIntFormat(val->tm_hour,2)+":"+
            AString::fromIntFormat(val->tm_min,2)+":"+
            AString::fromIntFormat(val->tm_sec,2)+"."+
            AString::fromIntFormat((stamp/1000)%1000,3);
}
