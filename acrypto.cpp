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

#include "acrypto.h"
#include "crypto/crc32.h"
#include "external/crypto/md5.h"
#include "external/crypto/sha1.h"
#include "amath_int.h"

using namespace alt;

cryptoHash::cryptoHash(HashFunction method)
    : hashfun(method)
{
    switch(hashfun)
    {
    case MD5:
        context = new MD5Context;
        break;
    case SHA1:
        context = new sha1;
        break;
    default: //CRC32
        context = new uint32;
        break;
    }
    reset();
}

cryptoHash::~cryptoHash()
{
    switch(hashfun)
    {
    case MD5:
        delete (MD5Context*)context;
        break;
    case SHA1:
        delete (sha1*)context;
        break;
    default: //CRC32
        delete (uint32*)context;
        break;
    }
}

void cryptoHash::reset()
{
    hashdata.clear();
    switch(hashfun)
    {
    case MD5:
        MD5Init((MD5Context*)context);
        break;
    case SHA1:
        ((sha1*)context)->reset();
        break;
    default: //CRC32
        *(uint32*)context=0xffffffff;
        break;
    }
}

void cryptoHash::append(const alt::byteArray &data)
{
    switch(hashfun)
    {
    case MD5:
        MD5Update((MD5Context*)context,data(),data.size());
        break;
    case SHA1:
        ((sha1*)context)->add(data(),data.size());
        break;
    default: //CRC32
        *(uint32*)context=__crc32(data(),data.size(),*(uint32*)context);
        break;
    }
}

void cryptoHash::append(void *buff, int size)
{
    switch(hashfun)
    {
    case MD5:
        MD5Update((MD5Context*)context,(uint8*)buff,size);
        break;
    case SHA1:
        ((sha1*)context)->add((uint8*)buff,size);
        break;
    default: //CRC32
        *(uint32*)context=__crc32(buff,size,*(uint32*)context);
        break;
    }
}

alt::byteArray cryptoHash::result()
{
    if(hashdata.isEmpty())
    {
        uint8 buff[32];
        switch(hashfun)
        {
        case MD5:
            MD5Final(buff,(MD5Context*)context);
            hashdata.append(buff,16);
            break;
        case SHA1:
            ((sha1*)context)->finalize();
            alt::unaligned_write(buff,imath::bswap32(((sha1*)context)->state[0]));
            alt::unaligned_write(buff+4,imath::bswap32(((sha1*)context)->state[1]));
            alt::unaligned_write(buff+8,imath::bswap32(((sha1*)context)->state[2]));
            alt::unaligned_write(buff+12,imath::bswap32(((sha1*)context)->state[3]));
            alt::unaligned_write(buff+16,imath::bswap32(((sha1*)context)->state[4]));
            hashdata.append(buff,20);
            break;
        default: //CRC32
            hashdata.appendT(*(uint32*)context);
            break;
        }
    }
    return hashdata;
}

uint32 cryptoHash::makeCRC32(void *buff, int size)
{
    return __crc32(buff,size);
}
