/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2020 Maxim L. Grishin  (altmer@arts-union.ru)

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

#ifndef BIGEND_TYPES_HEADER
#define BIGEND_TYPES_HEADER

#include "types.h"
#include "math_int.h"

class uint32b
{
private:
        uint32 data;
public:
        uint32b()
        {
          data=0;
        }

        uint32b(const uint32b &num)
        {
           data=num.data;
        }

        uint32b(const uint32 &num)
        {
           data=__bswap32(num);
        }

        uint32b& operator=(const uint32 &x)
        {
               data=__bswap32(x);
               return *this;
        }

        uint32b& operator+=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)+x);
               return *this;
        }

        uint32b& operator-=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)-x);
               return *this;
        }

        uint32b& operator*=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)*x);
               return *this;
        }

        uint32b& operator/=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)/x);
               return *this;
        }

        uint32b& operator<<=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)<<x);
               return *this;
        }

        uint32b& operator>>=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)>>x);
               return *this;
        }

        uint32b& operator%=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)%x);
               return *this;
        }

        uint32b& operator&=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)&x);
               return *this;
        }

        uint32b& operator|=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)|x);
               return *this;
        }

        uint32b& operator^=(const uint32 &x)
        {
               data=__bswap32(__bswap32(data)^x);
               return *this;
        }

        operator uint32()
        {
               return __bswap32(data);
        }


};

class int32b
{
private:
        int32 data;
public:
        int32b()
        {
          data=0;
        }

        int32b(const int32b &num)
        {
           data=num.data;
        }

        int32b(const int32 &num)
        {
           data=__bswap32(num);
        }

        int32b& operator=(const int32 &x)
        {
               data=__bswap32(x);
               return *this;
        }

        int32b& operator+=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)+x);
               return *this;
        }

        int32b& operator-=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)-x);
               return *this;
        }

        int32b& operator*=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)*x);
               return *this;
        }

        int32b& operator/=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)/x);
               return *this;
        }

        int32b& operator<<=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)<<x);
               return *this;
        }

        int32b& operator>>=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)>>x);
               return *this;
        }

        int32b& operator%=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)%x);
               return *this;
        }

        int32b& operator&=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)&x);
               return *this;
        }

        int32b& operator|=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)|x);
               return *this;
        }

        int32b& operator^=(const int32 &x)
        {
               data=__bswap32(__bswap32(data)^x);
               return *this;
        }

        operator int32()
        {
               return __bswap32(data);
        }
};

class uint16b
{
private:
        uint16 data;
public:
        uint16b()
        {
          data=0;
        }

        uint16b(const uint16b &num)
        {
           data=num.data;
        }

        uint16b(const uint16 &num)
        {
           data=__bswap16(num);
        }

        uint16b& operator=(const uint16 &x)
        {
               data=__bswap16(x);
               return *this;
        }

        uint16b& operator+=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)+x);
               return *this;
        }

        uint16b& operator-=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)-x);
               return *this;
        }

        uint16b& operator*=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)*x);
               return *this;
        }

        uint16b& operator/=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)/x);
               return *this;
        }

        uint16b& operator<<=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)<<x);
               return *this;
        }

        uint16b& operator>>=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)>>x);
               return *this;
        }

        uint16b& operator%=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)%x);
               return *this;
        }

        uint16b& operator&=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)&x);
               return *this;
        }

        uint16b& operator|=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)|x);
               return *this;
        }

        uint16b& operator^=(const uint16 &x)
        {
               data=__bswap16(__bswap16(data)^x);
               return *this;
        }

        operator uint16()
        {
               return __bswap16(data);
        }


};

#endif
