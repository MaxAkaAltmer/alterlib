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

#include <type_traits>

namespace alt {

    // ==========================================================================
    // endian<base_type, addr_xor> — generic big-endian wrapper
    //
    // Хранит значение в "big-endian" виде (swap_bytes — инволюция, поэтому
    // конструктор, деструкция и operator= используют один и тот же обмен).
    //
    // addr_xor по умолчанию = sizeof(base_type)-1 -> полный разворот байтов,
    // то есть хранение big-endian в точности как bswap16/bswap32/bswap64.
    // Меньший addr_xor (например, 3 для 64-битного типа) моделирует чтение
    // значения с шины меньшей разрядности.
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
        // Битовые операции не зависят от порядка байтов: swap_bytes — это
        // перестановка байтов, поэтому
        //     swap(a OP b) == swap(a) OP swap(b),   swap(~a) == ~swap(a)
        // То есть менять местами байты нужно только внешнему операнду, а если
        // оба уже в swap-представлении — обмен не нужен вовсе. Арифметика и
        // сдвиги так не сокращаются: перенос идёт через границы байтов.
        endian& operator&=(base_type val)  { data_ &= swap_bytes<base_type, addr_xor>(val); return *this; }
        endian& operator|=(base_type val)  { data_ |= swap_bytes<base_type, addr_xor>(val); return *this; }
        endian& operator^=(base_type val)  { data_ ^= swap_bytes<base_type, addr_xor>(val); return *this; }
        endian& operator<<=(base_type val) { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) << val); return *this; }
        endian& operator>>=(base_type val) { data_ = swap_bytes<base_type, addr_xor>(swap_bytes<base_type, addr_xor>(data_) >> val); return *this; }

        // то же над swap-представлением обоих операндов — без единого обмена
        endian& operator&=(const endian &o) { data_ &= o.data_; return *this; }
        endian& operator|=(const endian &o) { data_ |= o.data_; return *this; }
        endian& operator^=(const endian &o) { data_ ^= o.data_; return *this; }

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

        // bitwise NOT — перестановка байтов и ~ коммутируют, поэтому меняем
        // биты прямо в swap-представлении: ни одного обмена
        friend endian operator~(const endian &x)
        { endian r; r.data_ = ~x.data_; return r; }
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
    // оба операнда уже лежат в swap-представлении -> обменов нет вообще
    template <typename T, uint8 X>
    __inline endian<T, X> operator&(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) &= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator|(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) |= b; }
    template <typename T, uint8 X>
    __inline endian<T, X> operator^(const endian<T, X> &a, const endian<T, X> &b)
    { return endian<T, X>(a) ^= b; }

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

} // namespace alt

using alt::imath::endian;

// ==========================================================================
// Concrete type aliases
//
// Пока только 16- и 32-битные: addr_xor = sizeof-1, то есть полный разворот
// байтов — значение в памяти лежит ровно в big-endian порядке (== bswap16/
// bswap32 на little-endian хосте). Для 64 бит такой же alias пишется явно как
// endian<uint64, 7>; endian<uint64, 3> — это значение, приехавшее с 32-битной
// шины (разворот внутри dword + обмен dword), а не big-endian.
// ==========================================================================

// unsigned big-endian types
typedef endian<uint16, 1> uint16b;
typedef endian<uint32, 3> uint32b;

// signed big-endian types
typedef endian<int16, 1>  int16b;
typedef endian<int32, 3>  int32b;

#endif // ATYPES_BIGENDIAN_H
