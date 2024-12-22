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

#ifndef ACRYPTO_H
#define ACRYPTO_H

#include "../abyte_array.h"

namespace alt {

//todo: решить проблему с Algorithm::Algorithm
//Currently required:
//../alterlib/external/cryptopp/allocate.cpp \
//../alterlib/external/cryptopp/misc.cpp \
//../alterlib/external/cryptopp/iterhash.cpp \
//../alterlib/external/cryptopp/integer.cpp \
//../alterlib/external/cryptopp/queue.cpp \
//../alterlib/external/cryptopp/mqueue.cpp \
//../alterlib/external/cryptopp/filters.cpp \
//../alterlib/external/cryptopp/algparam.cpp \
//../alterlib/external/cryptopp/asn.cpp \
//../alterlib/external/cryptopp/nbtheory.cpp \
//../alterlib/external/cryptopp/sha.cpp \
//../alterlib/external/cryptopp/pubkey.cpp \
//../alterlib/external/cryptopp/primetab.cpp \

    class cryptoHash
    {
    public:
        enum HashFunction
        {
            CRC32 = 0,
            MD5,
            SHA1
        };

        cryptoHash(HashFunction method);
        ~cryptoHash();

        void reset();
        void append(const alt::byteArray &data);
        void append(void *buff, int size);
        alt::byteArray result();

        static uint32 makeCRC32(void *buff, int size);

    private:
        HashFunction hashfun;
        void *context;
        alt::byteArray hashdata;
    };

} // namespace alt

#endif // ACRYPTO_H
