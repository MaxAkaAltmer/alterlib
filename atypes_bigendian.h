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

#ifndef ATYPES_BIGENDIAN_H
#define ATYPES_BIGENDIAN_H

#include "atypes.h"
#include "amath_int.h"

namespace alt {
namespace imath {

    // ==========================================================================
    // Базовые свапы (используются внутри swap_bytes)
    // ==========================================================================

    __inline uint16 bswap16(uint16 x) { return uint16((x>>8) | (x<<8)); }
    __inline int16  bswap16(int16  x) { return int16(bswap16(uint16(x))); }
    __inline uint32 bswap32(uint32 x)
    { return (x>>24) | ((x>>8)&0x0000FF00L) | ((x&0x0000FF00L)<<8) | (x<<24); }
    __inline int32  bswap32(int32  x) { return int32(bswap32(uint32(x))); }
    __inline uint64 bswap64(uint64 x)
    {
        return (x>>56) | ((x>>40)&0x0000FF00) | ((x>>24)&0x00FF0000) | ((x>>8)&0xFF000000) |
                ((x&0xFF000000)<<8) | ((x&0x00FF0000)<<24) | ((x&0x0000FF00)<<40) | (x<<56);
    }
    __inline int64  bswap64(int64  x) { return int64(bswap64(uint64(x))); }

    // ==========================================================================
    // Универсальный байтовый swap с addr_xor
    //
    // addr_xor = 3  (схемотехническая аксиома!)
    // addr_xor & (sizeof(T)-1) — маскировка под размер типа
    //
    // Каждый байт с позицией i перемещается на позицию (i ^ addr_xor):
    //   shift = 8 * ((i ^ ax) - i), где ax = addr_xor & (sizeof(T)-1)
    //   результат[i] = (x >> shift) & 0xFF
    // ==========================================================================

    template <typename T, uint8 addr_xor>
    __inline T swap_bytes(T x)
    {
        if constexpr (sizeof(T) == 2 && addr_xor == 1)
            return bswap16(x);
        else if constexpr (sizeof(T) == 4 && addr_xor == 3)
            return bswap32(x);
        else if constexpr (sizeof(T) == 8 && addr_xor == 7)
            return bswap64(x);
        else
        {
            T result = T(0);
            uint8 ax = addr_xor & (sizeof(T) - 1);
            for (int i = 0; i < int(sizeof(T)); ++i)
            {
                int shift = 8 * ((i ^ ax) - i);
                if (shift >= 0)
                    result |= (T(x) >> shift) & T(0xFF);
                else
                    result |= (T(x) << (-shift)) & T(0xFF);
            }
            return result;
        }
    }

    // ==========================================================================
    // endian<base_type, addr_xor> — generic big-endian wrapper
    //
    // addr_xor = 3  (схемотехническая аксиома!)
    // ==========================================================================

    template <typename base_type, uint8 addr_xor = sizeof(base_type) - 1>
    class endian
    {
    private:
        base_type data_;

    public:
        // constructors
        endian() : data_() {}

        endian(const endian &o) : data_(o.data_) {}

        endian(base_type val) : data_(swap_bytes<base_type, addr_xor>(val)) {}

        // conversion
        explicit operator base_type() const
        {
            return swap_bytes<base_type, addr_xor>(data_);
        }

        explicit operator bool() const
        {
            return data_ != base_type(0);
        }

        // assignment
        endian& operator=(const endian &o)
        {
            data_ = o.data_;
            return *this;
        }

        endian& operator=(base_type val)
        {
            data_ = swap_bytes<base_type, addr_xor>(val);
            return *this;
        }

        // compound assignment (with base_type)
        endian& operator+=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) + val); return *this; }
        endian& operator-=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) - val); return *this; }
        endian& operator*=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) * val); return *this; }
        endian& operator/=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) / val); return *this; }
        endian& operator%=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) % val); return *this; }
        endian& operator&=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) & val); return *this; }
        endian& operator|=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) | val); return *this; }
        endian& operator^=(base_type val)  { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) ^ val); return *this; }
        endian& operator<<=(base_type val) { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) << val); return *this; }
        endian& operator>>=(base_type val) { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) >> val); return *this; }

        // prefix increment / decrement
        endian& operator++() { return *this += base_type(1); }
        endian& operator--() { return *this -= base_type(1); }

        // postfix increment / decrement
        endian operator++(int)
        { endian tmp(*this); ++*this; return tmp; }
        endian operator--(int)
        { endian tmp(*this); --*this; return tmp; }

        // unary minus (signed types)
        friend endian operator-(const endian &x)
        { return endian(-static_cast<base_type>(x)); }

        // bitwise NOT
        friend endian operator~(const endian &x)
        { return endian(~static_cast<base_type>(x)); }
    };

    // ==========================================================================
    // Helpers
    // ==========================================================================

    template <typename T, uint8 X>
    __inline T get_value(const endian<T, X> &v) { return static_cast<T>(v); }

    // ==========================================================================
    // Non-member binary operators (uendian + endian share same implementation)
    // ==========================================================================

    // --- arithmetic (base_type operand) ---
    template <typename T, uint8 X>
    __inline endian<T, X> operator+(const endian<T, X> &a, T b)
    { return endian<T, X>(a) += b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator-(const endian<T, X> &a, T b)
    { return endian<T, X>(a) -= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator*(const endian<T, X> &a, T b)
    { return endian<T, X>(a) *= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator/(const endian<T, X> &a, T b)
    { return endian<T, X>(a) /= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator%(const endian<T, X> &a, T b)
    { return endian<T, X>(a) %= b; }

    // --- bitwise (base_type operand) ---
    template <typename T, uint8 X>
    __inline endian<T, X> operator&(const endian<T, X> &a, T b)
    { return endian<T, X>(a) &= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator|(const endian<T, X> &a, T b)
    { return endian<T, X>(a) |= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator^(const endian<T, X> &a, T b)
    { return endian<T, X>(a) ^= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator<<(const endian<T, X> &a, T b)
    { return endian<T, X>(a) <<= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator>>(const endian<T, X> &a, T b)
    { return endian<T, X>(a) >>= b; }

    // --- comparison (base_type operand) ---
    template <typename T, uint8 X>
    __inline bool operator==(const endian<T, X> &a, T b) { return get_value(a) == b; }
    template <typename T, uint8 X>
    __inline bool operator!=(const endian<T, X> &a, T b) { return get_value(a) != b; }
    template <typename T, uint8 X>
    __inline bool operator<(const endian<T, X> &a, T b)   { return get_value(a) < b; }
    template <typename T, uint8 X>
    __inline bool operator>(const endian<T, X> &a, T b)   { return get_value(a) > b; }
    template <typename T, uint8 X>
    __inline bool operator<=(const endian<T, X> &a, T b)  { return get_value(a) <= b; }
    template <typename T, uint8 X>
    __inline bool operator>=(const endian<T, X> &a, T b)  { return get_value(a) >= b; }

    // --- arithmetic (endian vs endian) ---
    template <typename T, uint8 X>
    __inline endian<T, X> operator+(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) += get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator-(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) -= get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator*(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) *= get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator/(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) /= get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator%(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) %= get_value(b); }

    // --- bitwise (endian vs endian) ---
    template <typename T, uint8 X>
    __inline endian<T, X> operator&(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) &= get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator|(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) |= get_value(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator^(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) ^= get_value(b); }

    // --- shift (endian vs endian) ---
    template <typename T, uint8 X>
    __inline endian<T, X> operator<<(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) <<= static_cast<T>(b); }
    template <typename T, uint8 X>
    __inline endian<T, X> operator>>(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) >>= static_cast<T>(b); }

    // --- comparison (endian vs endian) ---
    template <typename T, uint8 X>
    __inline bool operator==(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) == get_value(b); }
    template <typename T, uint8 X>
    __inline bool operator!=(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) != get_value(b); }
    template <typename T, uint8 X>
    __inline bool operator<(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) < get_value(b); }
    template <typename T, uint8 X>
    __inline bool operator>(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) > get_value(b); }
    template <typename T, uint8 X>
    __inline bool operator<=(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) <= get_value(b); }
    template <typename T, uint8 X>
    __inline bool operator>=(const endian<T, X> &a, const endian<T, X> &b)
    { return get_value(a) >= get_value(b); }

} // namespace imath
} // namespace alt

using alt::imath::endian;

// ==========================================================================
// Concrete type aliases
// ==========================================================================

// unsigned big-endian types
typedef endian<uint16, 3> uint16b;
typedef endian<uint32, 3> uint32b;
typedef endian<uint64, 3> uint64b;

// signed big-endian types
typedef endian<int16, 3>  int16b;
typedef endian<int32, 3>  int32b;
typedef endian<int64, 3>  int64b;

#endif // ATYPES_BIGENDIAN_H
