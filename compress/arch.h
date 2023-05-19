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

#ifndef ARCH_H
#define ARCH_H

#include "../atypes.h"
#include "../abyte_array.h"

alt::byteArray _arch_data_block_compress_diff(const void *data_p, int size,
                                          const void *old_data_p);
bool _arch_data_block_decompress_diff(uint8 *patch, int patch_size,
                                            uint8 *data, int size);

alt::byteArray _arch_data_block_compress_best(const void *data_p, int size);
alt::byteArray _arch_data_block_compress_fast(const void *data_p, int size);
alt::byteArray _arch_data_block_compress_rle(const void *data_p, int size);

alt::byteArray _arch_data_block_decompress(const void *data_p, int size, int32 predict_size=-1);


#endif // ARCH_H
