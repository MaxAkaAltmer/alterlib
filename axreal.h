// SPDX-License-Identifier: MIT
//
// Idea and design: Maxim L. Grishin  <altmer@arts-union.ru>
// Implementation:  Qwen3.8-Flash-Next (AI assistant working inside AiderDesk)
//
// Copyright (c) 2026 the authors named above.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// axreal.h - header-only arbitrary precision binary floating point library
// (C++20).
//
// Namespace alt:
//   alt::xreal<Bits> - floating point number with a mantissa of Bits bits and
//                      an exponent of type std::ptrdiff_t.
//
// The companion header axint.h provides the integer backend (alt::xuint and
// alt::xint) this class is built on, and it is included here, so a program that
// uses both only has to include axreal.h.
//
// Representation
// --------------
// A value is  (-1)^sign * mantissa * 2^exponent  where the mantissa is an
// unsigned integer of at most Bits bits whose lowest bit is set, and the
// exponent is a std::ptrdiff_t.  Normalising by dropping the trailing zero bits
// leaves every number exactly one representation, which makes comparisons,
// hashing and equality cheap.  Zero and the two infinities are the special
// cases, and a zero carries a sign.
// There is no NaN: a domain error such as sqrt(-1) or 0/0 throws
// std::domain_error instead, because a type with an unlimited exponent range
// has no use for a second kind of "not a number".
//
// Values whose binade lies outside +/-2^exponent_limit are rounded away: the
// overflow direction produces an infinity, the underflow direction a signed
// zero.  The limit is a quarter of the exponent type, so intermediate results
// can never overflow the exponent itself.
//
// Precision and rounding
// ----------------------
// The mantissa width is a template parameter, so the precision is chosen by the
// declaration: xreal<53> behaves like a double, xreal<2048> like a scientific
// calculator with more than 600 decimal digits.  The default is 512 bits.
//
// Addition, subtraction, multiplication, division, sqrt, the conversions to and
// from the built-in floating point types and the decimal output are all
// correctly rounded in the sense of IEEE 754 / ISO 18661: the internally exact
// result is rounded once, ties to even being the default.  The five rounding
// directions of the standard are available as enum alt::rnd and can be selected
// by the explicit operations (add, sub, mul, div, sqrt, to_double, ...); the
// ordinary operators use round-to-nearest, ties to even.
//
// The transcendental functions (exp, log, trigonometry, gamma, ...) aim for a
// maximum error of one ulp: they compute with about a hundred guard bits and
// round once at the end.
//
// Short example:
//
//     #include "axreal.h"
//     #include <iostream>
//
//     using real = alt::xreal<300>;            // about 90 decimal digits
//     const real a = real("1.5") * 4 + 1;      // 7
//     std::cout << real::sqrt(a) << '\n';                    // 2.64575...
//     std::cout << real::sqrt(a, alt::rnd::toward_zero) << '\n';
//     std::cout << alt::pi<real>().to_string(40, 'f') << '\n';
//     std::cout << alt::pow(real(2), 100) - 1 << '\n';
//
// The type mixes freely with built-in integers, with the built-in floats (the
// conversion is exact) and with alt::xint / alt::xuint, because all of those
// conversions are implicit.  Widening to a wider xreal is implicit as well,
// narrowing is explicit (explicit constructor or rescale<P>()).
//
// Accompanying files, none of which the library itself needs:
//
//     axreal_test.cpp - unit tests, no framework, prints "all tests passed".
//     axreal_fuzz.cpp - an evaluator that reads "op operand..." lines on stdin
//                       and answers them with the library.
//     axreal_fuzz.py  - feeds that evaluator random operands, recomputes the
//                       exact answers with fractions.Fraction and the
//                       transcendental ones with mpmath, and reports the
//                       differences, which keeps the check independent of this
//                       implementation.

#ifndef AXREAL_H_INCLUDED
#define AXREAL_H_INCLUDED

#include "axint.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace alt {

inline namespace axreal_v1 {

// ---------------------------------------------------------------------------
// Rounding directions, named after what they do rather than after the +/-inf
// spelling of the standard.  nearest_even is what the operators use.
// ---------------------------------------------------------------------------

enum class rnd {
    nearest_even,    // closest representable value, ties to the even mantissa
    nearest_away,    // closest representable value, ties away from zero
    toward_positive, // the first representable value towards +inf
    toward_negative, // the first representable value towards -inf
    toward_zero      // the representable value of the smallest magnitude
};

template <unsigned Bits = 512>
class xreal;

// ---------------------------------------------------------------------------
// Internals.
//
// The name is xdetail and not detail, because axint.h already publishes
// alt::detail through its own inline namespace and two same named namespaces
// reached through two inline namespaces would make that name ambiguous.
// ---------------------------------------------------------------------------

namespace xdetail {

// True when at least one of the bits below position `pos` of m is set; pos 0
// therefore always reports false.
inline bool any_bits_below(const xuint& m, std::size_t pos) noexcept {
    return pos != 0 && alt::detail::any_low_bits(m.limb_data(), m.limb_size(), pos);
}

// Should the truncated mantissa be incremented by one ulp?  `top` is the
// highest discarded bit, `lower` reports whether any of the remaining discarded
// bits is set (top && !lower is an exact tie), `keep_odd` is the lowest bit that
// survives and `neg` the sign of the value.
inline bool need_increment(rnd mode, bool top, bool lower, bool keep_odd, bool neg) noexcept {
    switch (mode) {
        case rnd::nearest_even:    return top && (lower || keep_odd);
        case rnd::nearest_away:    return top; // above and exactly at half
        case rnd::toward_positive: return !neg && (top || lower);
        case rnd::toward_negative: return neg && (top || lower);
        default:                   return false; // toward_zero truncates
    }
}

// Where a value too large for its destination ends up.  As IEEE 754 asks: the
// directions running away from zero reach an infinity, while the ones heading
// back towards zero stop at the largest finite value - truncation always, and
// the two directed modes when they point at zero for this very sign.
inline bool overflow_stays_finite(rnd mode, bool neg) noexcept {
    return mode == rnd::toward_zero || (mode == rnd::toward_positive && neg) ||
           (mode == rnd::toward_negative && !neg);
}

// True for any instantiation of xreal, used to keep the operators from eating
// every type on earth.
template <class T>
concept big_float = requires { T::bits; T::exponent_limit; };

// Number of significant bits of the significand of a built-in float.
template <class F>
inline constexpr int binary_digits = std::numeric_limits<F>::digits;

// Exponent arithmetic that cannot overflow.  The outcome is only ever compared
// against the exponent limits afterwards, so parking it far outside of them is
// all it takes to get the right infinity or the right zero.
inline std::ptrdiff_t add_exp(std::ptrdiff_t a, std::ptrdiff_t b) noexcept {
    constexpr std::ptrdiff_t big = std::numeric_limits<std::ptrdiff_t>::max() / 2;
    if (b > 0 && a > big - b)
        return big;
    if (b < 0 && a < -big - b)
        return -big;
    return a + b;
}

// Drops everything below `width` bits of m * 2^e, rounds as mode says and brings
// the mantissa back into the normal form.  m must not be zero; the value stays
// as close to the exact one as the mode allows, so no range check happens here.
inline void round_mantissa(xuint& m, std::ptrdiff_t& e, bool neg, rnd mode,
                           unsigned width) {
    const std::size_t n = m.bit_size();
    if (n <= width)
        return; // nothing to drop, only the normalisation below matters
    // Dropping the low `shift` bits leaves the highest one of them (half an
    // ulp) and the ones below it (sticky) to decide the rounding.
    const std::size_t shift = n - width;
    const bool top = m.test_bit(shift - 1);
    const bool lower = any_bits_below(m, shift - 1);
    m >>= shift;
    if (need_increment(mode, top, lower, m.is_odd(), neg)) {
        m += xuint(1u);
        if (m.bit_size() > width) { // the increment carried out of the mantissa
            m >>= 1;
            ++e;
        }
    }
    e += std::ptrdiff_t(shift);
    // The normal form again: the lowest bit of the mantissa has to be set,
    // which is what gives every value a single representation.
    const std::size_t tail = alt::trailing_zeros(m);
    if (tail != 0) {
        m >>= tail;
        e += std::ptrdiff_t(tail);
    }
}

// The sign of an exact zero produced by an addition: the two zeros agree, and
// otherwise only rounding towards -inf picks the negative one.
inline bool zero_sign(bool aneg, bool bneg, rnd mode) noexcept {
    if (aneg == bneg)
        return aneg;
    return mode == rnd::toward_negative;
}

// --- text conversions ------------------------------------------------------

// ASCII only, so no locale and no character type surprises.
inline bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

// The value of a digit character, or -1.
inline int digit_of(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return -1;
}

inline char lower_of(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
}

inline std::string upper(std::string text) {
    for (char& c : text)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    return text;
}

// A magnitude that clamped or estimated exponents always fit into, used for the
// exponent of the small power functions.
inline unsigned long long exponent_count(std::ptrdiff_t e) noexcept {
    return e < 0 ? unsigned long long(-(e + 1)) + 1ull : unsigned long long(e);
}

// Case insensitive comparison against a lower case literal.
inline bool word_is(std::string_view text, std::string_view word) noexcept {
    if (text.size() != word.size())
        return false;
    for (std::size_t i = 0; i < word.size(); ++i)
        if (lower_of(text[i]) != word[i])
            return false;
    return true;
}

// Reads a decimal exponent, saturating far above any exponent limit so that the
// range check afterwards cannot be fooled by the overflow.
inline std::ptrdiff_t read_exponent(std::string_view text) {
    constexpr std::ptrdiff_t cap = std::numeric_limits<std::ptrdiff_t>::max() / 8;
    bool neg = false;
    std::size_t i = 0;
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
        neg = text[0] == '-';
        i = 1;
    }
    std::ptrdiff_t value = 0;
    bool big = false;
    std::size_t seen = 0;
    for (; i < text.size(); ++i) {
        const int d = digit_of(text[i]);
        if (d < 0 || d > 9)
            throw std::invalid_argument("alt::xreal::from_string: broken exponent");
        ++seen;
        if (!big && (value > cap / 10 || (value == cap / 10 && d > cap % 10)))
            big = true;
        if (!big)
            value = value * 10 + d;
    }
    if (seen == 0)
        throw std::invalid_argument("alt::xreal::from_string: broken exponent");
    if (big)
        value = cap; // far enough away from zero either way
    return neg ? -value : value;
}

// The decimal form of a finite, non zero value: the significant digits without
// leading or trailing zeros together with the position of the highest one, so
// that the value is digits * 10^(exp - len(digits) + 1).
struct decimal_form {
    std::string digits;
    std::ptrdiff_t exp = 0;
    // How many digits the expansion had before the trailing zeros were dropped,
    // which is the precision the general format compares its exponent against.
    std::size_t significant = 0;
};

// The decimal conversions work on the exact value, which means that they build
// an integer as long as the value has decimal digits.  Anything past this limit
// is reported instead of eating up the memory of the machine.
inline constexpr std::size_t max_decimal_digits = std::size_t(1) << 17;

// Marker for "as many digits as there are".
inline constexpr std::size_t all_places = std::numeric_limits<std::size_t>::max();

// Rounds a decimal expansion to `want` significant digits (at least one), ties
// to the even last digit, and drops the trailing zeros again.
inline void round_decimal(decimal_form& f, std::size_t want) {
    if (want == 0 || f.digits.size() <= want)
        return;
    const char top = f.digits[want];
    bool up = top > '5';
    if (top == '5') {
        bool rest = false;
        for (std::size_t i = want + 1; i < f.digits.size(); ++i)
            if (f.digits[i] != '0') {
                rest = true;
                break;
            }
        up = rest || (f.digits[want - 1] - '0') % 2 != 0;
    }
    f.digits.resize(want);
    if (up) {
        bool carry = true;
        for (std::size_t i = want; carry && i-- > 0;) {
            if (f.digits[i] == '9')
                f.digits[i] = '0';
            else {
                ++f.digits[i];
                carry = false;
            }
        }
        if (carry) { // 9.99 turned into 1 * 10^(exp + 1)
            f.digits.assign(1, '1');
            ++f.exp;
            return;
        }
    }
    while (f.digits.size() > 1 && f.digits.back() == '0')
        f.digits.pop_back();
}

// Rounds an expansion to the digit at the position `place`, which is what a
// fixed notation with a fixed number of digits behind the point needs.  Returns
// false when everything rounded away into a zero.
inline bool round_at(decimal_form& f, std::ptrdiff_t place) {
    const std::ptrdiff_t keep = f.exp - place + 1;
    if (keep >= 1) {
        round_decimal(f, std::size_t(keep));
        return true;
    }
    // The whole value sits below half of the rounding position, except when its
    // highest digit is exactly the five of a tie - and a tie goes to the even
    // neighbour, which here is the zero.
    bool up = false;
    if (keep == 0) {
        up = f.digits[0] > '5';
        if (f.digits[0] == '5')
            for (std::size_t i = 1; i < f.digits.size(); ++i)
                if (f.digits[i] != '0')
                    up = true;
    }
    if (!up)
        return false;
    f.digits.assign(1, '1');
    f.exp = place;
    f.significant = 1;
    return true;
}

// The exponent of a value that was parsed or scaled by hand: a magnitude far
// above the exponent limit is enough for the range check to see an overflow or
// an underflow, and it cannot overflow the exponent type on the way there.
inline std::ptrdiff_t clamp_exp(std::ptrdiff_t a) noexcept {
    constexpr std::ptrdiff_t far = std::numeric_limits<std::ptrdiff_t>::max() / 8;
    if (a > far)
        return far;
    if (a < -far)
        return -far;
    return a;
}

// The exponent of a printed value: "e+05" for the scientific notation, which
// the standard asks for with at least two digits, and "p+5" for the exact
// binary forms, which carry no padding.
inline std::string exponent_text(std::ptrdiff_t exp, char marker, bool padded = true) {
    std::string out(1, marker);
    out.push_back(exp < 0 ? '-' : '+');
    const std::uint64_t mag = exp < 0 ? std::uint64_t(-(exp + 1)) + 1ull : std::uint64_t(exp);
    const std::string digits = std::to_string(mag);
    if (padded && digits.size() < 2)
        out.push_back('0');
    out += digits;
    return out;
}

} // namespace xdetail

// The counters a probe reads when nanoseconds alone do not say where the time
// of a call went: how many steps the series summed, how many roots the
// logarithm took, and how often the four operations were asked for.  Nothing
// is reserved or touched unless the build defines AXREAL_MATH_STATS, and the
// counters are plain rather than atomic - they serve a single threaded probe.
#ifdef AXREAL_MATH_STATS
namespace xdetail {

struct math_stats {
    unsigned long long terms = 0ULL;  // next() calls of every series summed
    unsigned long long series = 0ULL; // series summed by sum_series
    unsigned long long roots = 0ULL;  // square roots taken inside log_core
    unsigned long long add = 0ULL;    // the class operations, every caller
    unsigned long long mul = 0ULL;
    unsigned long long div = 0ULL;
    unsigned long long sqrt = 0ULL;
};

inline math_stats& stats() {
    static math_stats value;
    return value;
}

} // namespace xdetail

#define AXREAL_STAT(field) (++alt::xdetail::stats().field)
#else
#define AXREAL_STAT(field)
#endif

// ---------------------------------------------------------------------------
// xreal - floating point with an arbitrarily wide binary mantissa.
// ---------------------------------------------------------------------------

template <unsigned Bits>
class xreal {
  public:
    static constexpr unsigned bits = Bits;         // mantissa width
    static constexpr unsigned guard = 96u;         // scratch bits of the math
    using exponent_type = std::ptrdiff_t;          // type of the exponent

    // Small widths are as valid as wide ones - xreal<24> behaves like a float -
    // but a mantissa of a handful of bits has no practical use.
    static_assert(Bits >= 8u, "alt::xreal needs a mantissa of at least 8 bits");

    // Binades closer to zero than 2^-exponent_limit or further away than
    // 2^exponent_limit are not representable; a quarter of the exponent type
    // leaves room for the intermediate steps of every operation.
    static constexpr exponent_type exponent_limit =
        std::numeric_limits<exponent_type>::max() / 4;

    static constexpr rnd default_rounding = rnd::nearest_even;

    xreal() noexcept = default;
    xreal(const xreal&) = default;
    xreal(xreal&&) noexcept = default;
    xreal& operator=(const xreal&) = default;
    xreal& operator=(xreal&&) noexcept = default;
    ~xreal() = default;

    // Built-in integers and the two big integer classes convert implicitly, the
    // value is always exact.
    template <std::integral T>
    xreal(T v) {
        assign_integer(v);
    }
    xreal(const xuint& v) { assign(false, v, 0); }
    xreal(const xint& v) { assign(v.is_negative(), v.magnitude(), 0); }
    template <std::integral T>
    xreal& operator=(T v) {
        assign_integer(v);
        return *this;
    }
    xreal& operator=(const xuint& v) {
        assign(false, v, 0);
        return *this;
    }
    xreal& operator=(const xint& v) {
        assign(v.is_negative(), v.magnitude(), 0);
        return *this;
    }

    // IEC 60559 floats are decomposed exactly; a value wider than Bits is
    // rounded once with the default mode.
    template <alt::detail::binary_float F>
    xreal(F v) {
        assign_binary(v);
    }
    template <alt::detail::binary_float F>
    xreal& operator=(F v) {
        assign_binary(v);
        return *this;
    }
    // An exotic format (IBM hexadecimal floating point, ...) is taken through
    // its decimal representation instead, which is exact for every width.
    template <std::floating_point F>
        requires(!alt::detail::binary_float<F>)
    explicit xreal(F v) {
        *this = from_string(decompose_exotic(v));
    }

    // Decimal, scientific and hexadecimal C99 float literals, see from_string.
    explicit xreal(std::string_view text) { *this = from_string(text); }
    explicit xreal(const char* text) : xreal(std::string_view(text)) {}

    // Widening (the source has the narrower mantissa) keeps every bit and is
    // therefore implicit, narrowing has to be asked for explicitly.
    template <unsigned P>
        requires(P < Bits)
    xreal(const xreal<P>& v) noexcept {
        assign_raw(v.is_infinity(), v.signbit(), v.mantissa(), v.exponent());
    }
    template <unsigned P>
        requires(P < Bits)
    xreal& operator=(const xreal<P>& v) noexcept {
        assign_raw(v.is_infinity(), v.signbit(), v.mantissa(), v.exponent());
        return *this;
    }
    template <unsigned P>
        requires(P > Bits)
    explicit xreal(const xreal<P>& v) {
        *this = build(v.is_infinity(), v.signbit(), v.mantissa(), v.exponent(),
                      default_rounding);
    }
    template <unsigned P>
        requires(P > Bits)
    explicit xreal(const xreal<P>& v, rnd mode) {
        *this = build(v.is_infinity(), v.signbit(), v.mantissa(), v.exponent(), mode);
    }

    // --- factories
    // The raw form takes a mantissa and an exponent as they are, rounding the
    // way the operators do.  It is the precise counterpart of from_string.
    static xreal from_mantissa(const xuint& mantissa, exponent_type exponent,
                               bool negative = false, rnd mode = default_rounding) {
        return build(false, negative, mantissa, exponent, mode);
    }
    static xreal infinity(bool negative = false) noexcept {
        xreal r;
        r.inf_ = true;
        r.neg_ = negative;
        return r;
    }
    static xreal zero(bool negative = false) noexcept {
        xreal r;
        r.neg_ = negative;
        return r;
    }
    // 2^(1-Bits), the distance from 1 to the next larger value.
    static xreal epsilon() noexcept {
        return from_mantissa(xuint(1u), exponent_type(1) - exponent_type(Bits));
    }
    static xreal max_value() noexcept {
        return from_mantissa((xuint(1u) << Bits) - xuint(1u),
                             exponent_limit - exponent_type(Bits) + 1);
    }
    static xreal lowest() noexcept { return -max_value(); }
    static xreal min_value() noexcept {
        return from_mantissa(xuint(1u), -exponent_limit);
    }

    // --- observers
    bool is_zero() const noexcept { return !inf_ && m_.is_zero(); }
    bool is_infinity() const noexcept { return inf_; }
    bool is_finite() const noexcept { return !inf_; }
    bool is_normal() const noexcept { return !inf_ && !m_.is_zero(); }
    bool signbit() const noexcept { return neg_; }
    explicit operator bool() const noexcept { return !is_zero(); }
    // -1, 0 or 1, the sign of the value (signed zero reports 0).
    int sign() const noexcept { return is_zero() ? 0 : (neg_ ? -1 : 1); }
    // The two halves of the representation; for a finite value the mantissa is
    // odd unless it is zero, and the exponent is the weight of its lowest bit.
    const xuint& mantissa() const noexcept { return m_; }
    exponent_type exponent() const noexcept { return e_; }
    // Number of significant bits of the mantissa, at most Bits.
    std::size_t precision() const noexcept { return m_.bit_size(); }
    // Position of the highest set bit, so 2^ilogb() <= |value| < 2^(ilogb()+1);
    // zero and infinity throw, like ilogb() does for the built-in types.
    exponent_type ilogb() const {
        if (inf_)
            throw std::domain_error("alt::xreal::ilogb of an infinity");
        if (m_.is_zero())
            throw std::domain_error("alt::xreal::ilogb of zero");
        return e_ + exponent_type(m_.bit_size()) - 1;
    }

    // --- conversions
    template <alt::detail::binary_float F>
    [[nodiscard]] F to_floating(rnd mode = default_rounding) const noexcept;
    [[nodiscard]] float to_float(rnd mode = default_rounding) const noexcept {
        return to_floating<float>(mode);
    }
    [[nodiscard]] double to_double(rnd mode = default_rounding) const noexcept {
        return to_floating<double>(mode);
    }
    [[nodiscard]] long double to_long_double(rnd mode = default_rounding) const noexcept {
        return to_floating<long double>(mode);
    }
    template <alt::detail::binary_float F>
    explicit operator F() const noexcept {
        return to_floating<F>();
    }

    // The integral part, in the four directions of the standard library; all of
    // them throw for an infinity, trunc() and friends for a value that does not
    // fit into the target type as well.
    [[nodiscard]] xint trunc() const;
    [[nodiscard]] xint floor() const;
    [[nodiscard]] xint ceil() const;
    // The two nearest integers differ in what they do with a halfway value:
    // round() leaves it the way std::round does, rint() takes the even one the
    // way the rounding modes of this class do.
    [[nodiscard]] xint round() const;
    [[nodiscard]] xint rint() const;
    // The value minus trunc(), keeping the sign.
    [[nodiscard]] xreal frac() const;

    template <std::integral T>
    [[nodiscard]] std::optional<T> to(rnd mode = default_rounding) const;

    // Every value of this type as one of the width P; the mode is only asked for
    // when P is the narrower type, a wider one holds the value exactly.
    template <unsigned P>
    [[nodiscard]] xreal<P> rescale(rnd mode = default_rounding) const {
        using W = xreal<P>;
        if constexpr (P > Bits) {
            if (inf_)
                return W::infinity(neg_);
            return W::from_mantissa(m_, e_, neg_, mode); // exact, whatever the mode
        } else if constexpr (P == Bits) {
            static_cast<void>(mode);
            return *this;
        } else {
            return W(*this, mode);
        }
    }

    // --- strings
    // A decimal ("1.75e-4"), a C99 hexadecimal ("0x1.8p+3") or a binary
    // ("0b1.11p-2") literal, with an optional sign and the words "inf" and
    // "infinity".  "nan" is rejected, this type has no NaN.  The result is the
    // correctly rounded one, exactly like the arithmetic.
    static xreal from_string(std::string_view text, rnd mode = default_rounding);
    // digits == 0 means "as many as the mantissa needs to be exact"; the fmt
    // follows the printf letters: 'f', 'e', 'g' and their upper case twins, plus
    // 'x' for the exact hexadecimal form.  Rounding ties go to an even digit.
    [[nodiscard]] std::string to_string(unsigned digits = 0, char fmt = 'g') const;
    // Exact, bit for bit, forms: 0x1.8p+3 style and 0b1.1p+3 style.
    [[nodiscard]] std::string to_hex_string(bool upper = false) const;
    [[nodiscard]] std::string to_binary_string() const;

    // --- arithmetic, one explicit operation per operator so that the rounding
    // mode can be named; they are also what the operators call.
    static xreal add(const xreal& a, const xreal& b, rnd mode = default_rounding);
    static xreal sub(const xreal& a, const xreal& b, rnd mode = default_rounding);
    static xreal mul(const xreal& a, const xreal& b, rnd mode = default_rounding);
    static xreal div(const xreal& a, const xreal& b, rnd mode = default_rounding);
    static xreal sqrt(const xreal& a, rnd mode = default_rounding);

    // --- the xreal operator set (hidden friends)
    friend xreal operator+(const xreal& a, const xreal& b) { return add(a, b); }
    friend xreal operator-(const xreal& a, const xreal& b) { return sub(a, b); }
    friend xreal operator*(const xreal& a, const xreal& b) { return mul(a, b); }
    friend xreal operator/(const xreal& a, const xreal& b) { return div(a, b); }

    friend xreal operator-(const xreal& a) noexcept {
        xreal r(a);
        r.neg_ = !r.neg_; // an infinity and a signed zero flip as well
        return r;
    }
    friend xreal operator+(const xreal& a) noexcept { return a; }

    friend xreal& operator+=(xreal& a, const xreal& b) { return a = add(a, b); }
    friend xreal& operator-=(xreal& a, const xreal& b) { return a = sub(a, b); }
    friend xreal& operator*=(xreal& a, const xreal& b) { return a = mul(a, b); }
    friend xreal& operator/=(xreal& a, const xreal& b) { return a = div(a, b); }

    friend xreal& operator++(xreal& a) { return a = add(a, xreal(1)); }
    friend xreal& operator--(xreal& a) { return a = sub(a, xreal(1)); }
    friend xreal operator++(xreal& a, int) {
        const xreal t(a);
        ++a;
        return t;
    }
    friend xreal operator--(xreal& a, int) {
        const xreal t(a);
        --a;
        return t;
    }

    // Signed zeros compare equal, so the ordering is a strong one and no value
    // is unordered.
    friend std::strong_ordering operator<=>(const xreal& a, const xreal& b) noexcept {
        const int c = compare(a, b);
        if (c == 0)
            return std::strong_ordering::equal;
        return c < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    friend bool operator==(const xreal& a, const xreal& b) noexcept {
        return compare(a, b) == 0;
    }

    friend void swap(xreal& a, xreal& b) noexcept {
        std::swap(a.inf_, b.inf_);
        std::swap(a.neg_, b.neg_);
        std::swap(a.e_, b.e_);
        a.m_.swap(b.m_);
    }

    // Multiplying by an exact power of two never rounds, so the shift happens
    // without the rounding step; an overflow still yields an infinity.
    [[nodiscard]] xreal scaled(exponent_type shift) const {
        if (inf_ || is_zero() || shift == 0)
            return *this;
        return build(inf_, neg_, m_, xdetail::add_exp(e_, shift),
                     default_rounding);
    }

    // Splits the value into a mantissa in [0.5, 1) and a power of two, like
    // frexp().  The exponent may be null when only the mantissa is of interest.
    [[nodiscard]] xreal frexp(exponent_type* exp = nullptr) const {
        if (inf_ || is_zero()) {
            if (exp != nullptr)
                *exp = 0;
            return *this;
        }
        const exponent_type k = ilogb() + 1;
        if (exp != nullptr)
            *exp = k;
        // Dividing by 2^k moves the top bit of the mantissa to the position
        // right below the point, which is exactly the [0.5, 1) half.
        return build(false, neg_, m_, e_ - k, default_rounding);
    }

  private:
    static xreal build(bool inf, bool neg, const xuint& mantissa, exponent_type exponent,
                       rnd mode);

    // A rounded value that left the range; like in IEEE 754 the mode decides
    // whether the result goes to infinity (or to zero) or is held back at the
    // closest finite border value.
    static xreal overflowed(bool neg, rnd mode) noexcept;
    // The same at the lower end, where the midpoint between zero and the
    // smallest magnitude has to be respected by the two nearest modes.
    static xreal underflowed(xuint m, exponent_type e, bool neg, rnd mode);

    // Rounds m * 2^e to W bits and normalises the result.  W never exceeds Bits,
    // so the outcome always satisfies the class invariants.
    template <unsigned W>
    static xreal rounded(xuint m, exponent_type e, bool neg, rnd mode);

    void assign(bool neg, const xuint& m, exponent_type e) {
        *this = build(false, neg, m, e, default_rounding);
    }
    void assign_raw(bool inf, bool neg, const xuint& m, exponent_type e) noexcept {
        inf_ = inf;
        neg_ = neg;
        // An infinity carries no mantissa, its exponent is meaningless.
        m_ = inf ? xuint() : m;
        e_ = inf ? exponent_type(0) : e;
    }
    template <std::integral T>
    void assign_integer(T v);
    template <alt::detail::binary_float F>
    void assign_binary(F v);
    template <std::floating_point F>
        requires(!alt::detail::binary_float<F>)
    static std::string decompose_exotic(F v);

    static int compare(const xreal& a, const xreal& b) noexcept;
    static int compare_raw(const xuint& ma, exponent_type ea, const xuint& mb,
                           exponent_type eb) noexcept;

    // The signed sum of two magnitudes, the single place addition and
    // subtraction are implemented in.  aneg and bneg are the signs the operands
    // are to be taken with, which lets sub() flip the second one.
    static xreal summed(const xreal& a, bool aneg, const xreal& b, bool bneg, rnd mode);
    // The representable value one ulp further from (away) or closer to zero than
    // the magnitude neg * m * 2^e.
    static xreal advanced(const xuint& m, exponent_type e, bool neg, bool away) noexcept;
    // The integer nearest to v in the given direction; throws for an infinity.
    static xint rounded_integer(const xreal& v, rnd mode);

    // The exact decimal expansion of the magnitude, which is always finite.
    // Throws std::overflow_error when the expansion would become unreasonably
    // long, because the conversion works on the exact value.
    xdetail::decimal_form decimal_digits() const;
    // The fewest decimal digits that read back as this very value.
    unsigned shortest_digits() const;
    // The three printf styles, working on the digits of decimal_digits().
    static std::string render_fixed(const xdetail::decimal_form& f, std::size_t places,
                                   bool keep_point);
    static std::string render_scientific(const xdetail::decimal_form& f,
                                         std::size_t places, bool keep_point);

    xuint m_;
    exponent_type e_ = 0;
    bool inf_ = false;
    bool neg_ = false;
};

// Concepts that name the two families the class interoperates with.
template <class T>
concept any_xreal = requires { T::bits; T::exponent_limit; T::guard; };

// ---------------------------------------------------------------------------
// Construction and normalisation.
// ---------------------------------------------------------------------------

template <unsigned Bits>
template <unsigned W>
xreal<Bits> xreal<Bits>::rounded(xuint m, exponent_type e, bool neg, rnd mode) {
    static_assert(W >= 8u && W <= Bits, "rounding width outside the mantissa");
    if (m.is_zero()) {
        xreal r;
        r.neg_ = neg; // zero keeps its sign, x - x of equal values included
        return r;
    }
    xdetail::round_mantissa(m, e, neg, mode, W);
    // Normal form: the lowest bit of the mantissa is set, which is what makes
    // the representation unique.
    const std::size_t tail = alt::trailing_zeros(m);
    if (tail != 0) {
        m >>= tail;
        e += exponent_type(tail);
    }
    // The binade decides about the range, not the exponent of the lowest bit:
    // a wide mantissa reaches further up than its own exponent suggests.
    const exponent_type top = e + exponent_type(m.bit_size()) - 1;
    if (top > exponent_limit)
        return overflowed(neg, mode);
    if (top < -exponent_limit)
        return underflowed(std::move(m), e, neg, mode);
    xreal r;
    r.m_ = std::move(m);
    r.e_ = e;
    r.neg_ = neg;
    return r;
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::build(bool inf, bool neg, const xuint& mantissa,
                               exponent_type exponent, rnd mode) {
    if (inf)
        return infinity(neg);
    if (mantissa.is_zero())
        return zero(neg);
    return rounded<Bits>(mantissa, exponent, neg, mode);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::overflowed(bool neg, rnd mode) noexcept {
    // The closest value to anything that is too large is an infinity, and so is
    // the outcome of the two directed modes that step away from zero.
    const bool outward = mode == rnd::nearest_even || mode == rnd::nearest_away ||
                         (neg ? mode == rnd::toward_negative : mode == rnd::toward_positive);
    if (outward)
        return infinity(neg);
    xreal r = max_value();
    r.neg_ = neg;
    return r;
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::underflowed(xuint m, exponent_type e, bool neg, rnd mode) {
    // The smallest magnitude the format has, with the requested sign.
    const auto smallest = [neg]() {
        xreal r;
        r.m_ = xuint(1u);
        r.e_ = -exponent_limit;
        r.neg_ = neg;
        return r;
    };
    switch (mode) {
        case rnd::toward_zero:
            return zero(neg); // truncation always reaches zero first
        case rnd::nearest_even:
        case rnd::nearest_away: {
            // Only the midpoint between zero and the smallest magnitude is
            // closer to zero than to it; an exact tie belongs to the even
            // mantissa, and zero counts as even.
            const int c = compare_raw(m, e, xuint(1u), -exponent_limit - 1);
            if (c < 0)
                return zero(neg);
            if (c > 0)
                return smallest();
            return mode == rnd::nearest_away ? smallest() : zero(neg);
        }
        default:
            break;
    }
    const bool outward = neg ? mode == rnd::toward_negative : mode == rnd::toward_positive;
    return outward ? smallest() : zero(neg);
}

template <unsigned Bits>
template <std::integral T>
void xreal<Bits>::assign_integer(T v) {
    if (v == T(0)) {
        m_.clear();
        e_ = 0;
        inf_ = false;
        neg_ = false;
        return;
    }
    if constexpr (std::is_signed_v<T>) {
        using U = std::make_unsigned_t<T>;
        // Negating in the unsigned domain keeps the lowest value of the type
        // representable, exactly like in xint.
        if (v < T(0)) {
            assign(true, xuint(static_cast<U>(U(0) - static_cast<U>(v))), 0);
            return;
        }
        assign(false, xuint(static_cast<U>(v)), 0);
    } else {
        assign(false, xuint(v), 0);
    }
}

template <unsigned Bits>
template <alt::detail::binary_float F>
void xreal<Bits>::assign_binary(F v) {
    if (std::isnan(v))
        throw std::domain_error("alt::xreal has no NaN, a NaN cannot be converted");
    if (std::isinf(v)) {
        *this = infinity(v < F(0));
        return;
    }
    if (v == F(0)) {
        *this = zero(std::signbit(v));
        return;
    }
    // frexp() reports the true exponent of a subnormal too, so scaling its
    // mantissa into [2^(d-1), 2^d) for the precision d gives the integral
    // significand exactly, without ever leaving the range of the type.
    int be = 0;
    const F part = std::frexp(v < F(0) ? -v : v, &be);
    const int shift = xdetail::binary_digits<F>;
    const std::uint64_t sig = std::uint64_t(std::ldexp(part, shift));
    *this = rounded<Bits>(xuint(sig), exponent_type(be) - exponent_type(shift),
                          v < F(0), default_rounding);
}

template <unsigned Bits>
template <std::floating_point F>
    requires(!alt::detail::binary_float<F>)
std::string xreal<Bits>::decompose_exotic(F v) {
    if (std::isnan(v))
        throw std::domain_error("alt::xreal has no NaN, a NaN cannot be converted");
    if (std::isinf(v))
        return v < F(0) ? "-inf" : "inf";
    std::ostringstream os;
    os << std::setprecision(std::numeric_limits<F>::max_digits10) << v;
    return os.str();
}

// ---------------------------------------------------------------------------
// Comparison.
// ---------------------------------------------------------------------------

template <unsigned Bits>
int xreal<Bits>::compare_raw(const xuint& ma, exponent_type ea, const xuint& mb,
                             exponent_type eb) noexcept {
    // The position of the highest bit decides first; equal positions mean the
    // two mantissas have to be lined up, which costs at most two shifts.
    const exponent_type pa = ea + exponent_type(ma.bit_size());
    const exponent_type pb = eb + exponent_type(mb.bit_size());
    if (pa != pb)
        return pa < pb ? -1 : 1;
    if (ea == eb)
        return alt::detail::cmp_wide(ma.limb_data(), ma.limb_size(), mb.limb_data(),
                                   mb.limb_size());
    const exponent_type base = ea < eb ? ea : eb;
    const xuint ua = ma << std::size_t(ea - base);
    const xuint ub = mb << std::size_t(eb - base);
    return alt::detail::cmp_wide(ua.limb_data(), ua.limb_size(), ub.limb_data(), ub.limb_size());
}

template <unsigned Bits>
int xreal<Bits>::compare(const xreal& a, const xreal& b) noexcept {
    if (a.inf_ || b.inf_) {
        if (a.inf_ && b.inf_)
            return a.neg_ == b.neg_ ? 0 : (a.neg_ ? -1 : 1);
        return a.inf_ ? (a.neg_ ? -1 : 1) : (b.neg_ ? 1 : -1);
    }
    const bool az = a.m_.is_zero(), bz = b.m_.is_zero();
    if (az || bz)
        return az ? (bz ? 0 : (b.neg_ ? 1 : -1)) : (a.neg_ ? -1 : 1);
    if (a.neg_ != b.neg_)
        return a.neg_ ? -1 : 1;
    const int c = compare_raw(a.m_, a.e_, b.m_, b.e_);
    return a.neg_ ? -c : c;
}

// ---------------------------------------------------------------------------
// Addition and subtraction.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xreal<Bits> xreal<Bits>::advanced(const xuint& m, exponent_type e, bool neg,
                                  bool away) noexcept {
    // The representable numbers in the binade of m * 2^e are spaced by
    // 2^(top + 1 - Bits), so the neighbour is one step of that grid away from
    // the mantissa widened to the full width.
    const std::size_t n = m.bit_size();
    if (!away && n == 1) {
        // A power of two sits at the bottom of its binade where the grid is only
        // half as wide as the one above it, so its predecessor is the widest
        // mantissa of all one exponent lower, not the mantissa decreased by one.
        xuint wide = (xuint(1u) << Bits) - xuint(1u);
        return rounded<Bits>(std::move(wide),
                             xdetail::add_exp(e, -exponent_type(Bits)), neg,
                             default_rounding);
    }
    const exponent_type step =
        xdetail::add_exp(e, exponent_type(n) - exponent_type(Bits));
    const xuint wide = m << (Bits - n);
    const xuint moved = away ? wide + xuint(1u) : wide - xuint(1u);
    return rounded<Bits>(moved, step, neg, default_rounding);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::summed(const xreal& a, bool aneg, const xreal& b, bool bneg,
                                rnd mode) {
    // Infinities first: adding anything finite keeps the infinity, and the two
    // opposite infinities have no sum at all.
    if (a.inf_ || b.inf_) {
        if (a.inf_ && b.inf_) {
            if (aneg != bneg)
                throw std::domain_error("alt::xreal: an infinity minus itself has no value");
            return infinity(aneg);
        }
        return infinity(a.inf_ ? aneg : bneg);
    }
    const bool az = a.m_.is_zero(), bz = b.m_.is_zero();
    if (az || bz) {
        if (az && bz)
            return zero(xdetail::zero_sign(aneg, bneg, mode));
        const xreal& other = az ? b : a;
        return build(false, az ? bneg : aneg, other.m_, other.e_, mode); // exact
    }

    // Positions one past the highest set bit, which is where the magnitude of
    // each operand ends.
    const exponent_type pa = xdetail::add_exp(a.e_, exponent_type(a.m_.bit_size()));
    const exponent_type pb = xdetail::add_exp(b.e_, exponent_type(b.m_.bit_size()));
    const exponent_type d = xdetail::add_exp(pa, -pb);
    const bool grow = aneg == bneg; // the magnitudes add up or work against each other

    if (d >= exponent_type(Bits) + 2 || d <= -(exponent_type(Bits) + 2)) {
        // The smaller magnitude is below a quarter of an ulp of the bigger one,
        // so the exact result lies strictly between the bigger operand and its
        // neighbour.  Round to nearest cannot leave the operand, the directed
        // modes step one grid point away from or towards zero.
        const bool a_first = d > 0;
        const xreal& big = a_first ? a : b;
        const bool big_neg = a_first ? aneg : bneg;
        if (mode == rnd::nearest_even || mode == rnd::nearest_away)
            return build(false, big_neg, big.m_, big.e_, mode);
        // grow: |result| is a little above |big|, cancel: a little below it.
        // A value just above a representable one rounds to the next number in
        // the direction of the rounding, a value just below to the previous.
        const bool up = (mode == rnd::toward_positive);
        const bool down = (mode == rnd::toward_negative);
        // "away from zero" is upwards for positive and downwards for negative
        // values, the opposite for "towards zero".
        const bool exact_above = grow; // the exact result is further from zero
        bool away = false, toward = false;
        if (exact_above) {
            away = (up && !big_neg) || (down && big_neg);
        } else {
            toward = (up && big_neg) || (down && !big_neg) || mode == rnd::toward_zero;
            away = false;
        }
        if (mode == rnd::toward_zero && exact_above)
            return build(false, big_neg, big.m_, big.e_, mode);
        if (away)
            return advanced(big.m_, big.e_, big_neg, true);
        if (toward)
            return advanced(big.m_, big.e_, big_neg, false);
        return build(false, big_neg, big.m_, big.e_, mode);
    }

    // Line the two mantissas up on the smaller exponent - at most Bits + 2 bits
    // of stretch, so the temporary stays small - and combine them exactly.  One
    // rounding afterwards is what makes the result correctly rounded.
    const exponent_type base = a.e_ < b.e_ ? a.e_ : b.e_;
    const xuint ua = a.m_ << std::size_t(xdetail::add_exp(a.e_, -base));
    const xuint ub = b.m_ << std::size_t(xdetail::add_exp(b.e_, -base));
    if (grow)
        return rounded<Bits>(ua + ub, base, aneg, mode);
    const int c = alt::detail::cmp_wide(ua.limb_data(), ua.limb_size(), ub.limb_data(),
                                     ub.limb_size());
    if (c == 0)
        return zero(xdetail::zero_sign(aneg, bneg, mode)); // they cancel exactly
    if (c > 0)
        return rounded<Bits>(ua - ub, base, aneg, mode);
    return rounded<Bits>(ub - ua, base, bneg, mode);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::add(const xreal& a, const xreal& b, rnd mode) {
    AXREAL_STAT(add);
    return summed(a, a.neg_, b, b.neg_, mode);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::sub(const xreal& a, const xreal& b, rnd mode) {
    return summed(a, a.neg_, b, !b.neg_, mode);
}

// ---------------------------------------------------------------------------
// Multiplication, division and square root.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xreal<Bits> xreal<Bits>::mul(const xreal& a, const xreal& b, rnd mode) {
    AXREAL_STAT(mul);
    const bool neg = a.neg_ != b.neg_;
    if (a.inf_ || b.inf_) {
        if (a.is_zero() || b.is_zero())
            throw std::domain_error("alt::xreal: zero times an infinity has no value");
        return infinity(neg);
    }
    if (a.is_zero() || b.is_zero())
        return zero(neg);
    // The product of two odd mantissas needs at most 2 * Bits bits, which the
    // single rounding inside rounded() turns into the correct result.
    return rounded<Bits>(a.m_ * b.m_, xdetail::add_exp(a.e_, b.e_), neg, mode);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::div(const xreal& a, const xreal& b, rnd mode) {
    AXREAL_STAT(div);
    const bool neg = a.neg_ != b.neg_;
    if (a.inf_) {
        if (b.inf_)
            throw std::domain_error("alt::xreal: infinity divided by infinity has no value");
        return infinity(neg);
    }
    if (b.inf_)
        return zero(neg);
    if (b.is_zero()) {
        if (a.is_zero())
            throw std::domain_error("alt::xreal: zero divided by zero has no value");
        return infinity(neg); // IEEE: a non zero dividend gives an infinity
    }
    if (a.is_zero())
        return zero(neg);
    // Three guard bits: the quotient ends up with Bits + 2 of them, so the
    // sticky bit of the remainder cannot end up in the rounding decision, and
    // OR-ing it into the lowest bit of the quotient is enough to round correctly
    // in every direction.
    std::size_t k = std::size_t(Bits) + 3 + b.m_.bit_size() - a.m_.bit_size();
    for (;;) {
        xuint q, r;
        xuint::divrem(a.m_ << k, b.m_, q, r);
        if (q.bit_size() >= Bits + 2) {
            if (!r.is_zero())
                q |= xuint(1u);
            exponent_type e = xdetail::add_exp(a.e_, -b.e_);
            e = xdetail::add_exp(e, -exponent_type(k));
            return rounded<Bits>(std::move(q), e, neg, mode);
        }
        k += 2; // the leading digits cancelled more than expected
    }
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::sqrt(const xreal& a, rnd mode) {
    AXREAL_STAT(sqrt);
    if (a.inf_) {
        if (a.neg_)
            throw std::domain_error("alt::xreal: square root of a negative infinity");
        return infinity();
    }
    if (a.m_.is_zero())
        return zero(a.neg_); // the square root of a signed zero is itself
    if (a.neg_)
        throw std::domain_error("alt::xreal: square root of a negative value");
    // sqrt(m * 2^e) == sqrt(m * 2^j) * 2^((e - j) / 2) for every j with the same
    // parity as e.  j lifts the radicand high enough that its root carries two
    // guard bits, and the remainder of the integer root tells an exact square
    // from a rounded one, which is what the ties have to know.
    exponent_type j = exponent_type(2u * (Bits + 4u)) - exponent_type(a.m_.bit_size());
    if (j < 0)
        j = 0;
    if (((a.e_ - j) & 1) != 0)
        ++j; // (e - j) has to be even, or the exponent of the root is off by a half
    for (;;) {
        const std::pair<xuint, xuint> sr = alt::sqrtrem(a.m_ << std::size_t(j));
        if (sr.first.bit_size() >= Bits + 2) {
            xuint s = sr.first;
            if (!sr.second.is_zero())
                s |= xuint(1u); // sticky bit of the remainder
            return rounded<Bits>(std::move(s), xdetail::add_exp(a.e_, -j) / 2, false,
                                 mode);
        }
        j += 2; // parity preserved, root too narrow
    }
}

// ---------------------------------------------------------------------------
// Conversions to the built-in floating point types.
// ---------------------------------------------------------------------------

template <unsigned Bits>
template <alt::detail::binary_float F>
F xreal<Bits>::to_floating(rnd mode) const noexcept {
    constexpr int digs = std::numeric_limits<F>::digits;
    constexpr exponent_type top_max = std::numeric_limits<F>::max_exponent - 1;
    // The fixed grid of the subnormals, whose quantum 2^(min_exponent - digs)
    // is the smallest positive value of the type (2^-1074 for a double).
    constexpr exponent_type step_min =
        exponent_type(std::numeric_limits<F>::min_exponent) - digs;
    if (inf_)
        return neg_ ? -std::numeric_limits<F>::infinity()
                    : std::numeric_limits<F>::infinity();
    if (m_.is_zero())
        return neg_ ? -F(0) : F(0);

    const exponent_type top = ilogb();
    if (top > top_max + 1) {
        // Far above the largest finite value; only the directions running back
        // towards zero have a finite answer left.
        if (xdetail::overflow_stays_finite(mode, neg_))
            return neg_ ? -std::numeric_limits<F>::max()
                        : std::numeric_limits<F>::max();
        return neg_ ? -std::numeric_limits<F>::infinity()
                    : std::numeric_limits<F>::infinity();
    }
    // The grid the value has to land on: the normal one, or the fixed subnormal
    // one when the binade is below the smallest normal.
    exponent_type step = xdetail::add_exp(top, 1 - digs);
    if (step < step_min)
        step = step_min;
    const std::ptrdiff_t shift = xdetail::add_exp(step, -e_); // bits to drop
    xuint m = m_;
    exponent_type e = e_;
    if (shift > 0) {
        const std::size_t sh = std::size_t(shift);
        const std::size_t n = m.bit_size();
        bool top_bit = false, lower = false;
        if (sh <= n) {
            top_bit = m.test_bit(sh - 1);
            lower = xdetail::any_bits_below(m, sh - 1);
            m >>= sh;
        } else {
            lower = true; // everything falls away, the value is below half a step
            m.clear();
        }
        e = xdetail::add_exp(e, std::ptrdiff_t(sh));
        if (xdetail::need_increment(mode, top_bit, lower, m.is_odd(), neg_))
            m += xuint(1u);
    } else if (shift < 0) {
        m <<= std::size_t(-shift); // exact, the target has more bits than we do
        e = xdetail::add_exp(e, std::ptrdiff_t(shift));
    }
    if (m.is_zero())
        return neg_ ? -F(0) : F(0);
    if (xdetail::add_exp(e, exponent_type(m.bit_size()) - 1) > top_max) {
        // The rounding itself overflowed, and the mode decides between the
        // largest finite value and an infinity, as it did a little ways above.
        if (xdetail::overflow_stays_finite(mode, neg_))
            return neg_ ? -std::numeric_limits<F>::max()
                        : std::numeric_limits<F>::max();
        return neg_ ? -std::numeric_limits<F>::infinity()
                    : std::numeric_limits<F>::infinity();
    }
    // m now has at most digs bits - the binary_float concept keeps that within a
    // machine word - and m * 2^e is exactly representable, so scaling it cannot
    // round a second time.
    const F scaled = std::ldexp(static_cast<F>(static_cast<std::uint64_t>(m)),
                                static_cast<int>(e));
    return neg_ ? -scaled : scaled;
}

// ---------------------------------------------------------------------------
// The integral part.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xint xreal<Bits>::rounded_integer(const xreal& v, rnd mode) {
    if (v.inf_)
        throw std::domain_error("alt::xreal: an infinity has no integral part");
    if (v.m_.is_zero())
        return xint(0);
    xuint m = v.m_;
    if (v.e_ >= 0) {
        m <<= std::size_t(v.e_); // exact, nothing to round
        return v.neg_ ? -xint(m) : xint(m);
    }
    const std::size_t drop = std::size_t(-v.e_); // bits behind the binary point
    const std::size_t n = m.bit_size();
    bool top_bit = false, lower = false;
    if (drop <= n) {
        top_bit = m.test_bit(drop - 1);
        lower = xdetail::any_bits_below(m, drop - 1);
        m >>= drop;
    } else {
        lower = true; // |value| is below a half, only the magnitude stays
        m.clear();
    }
    if (xdetail::need_increment(mode, top_bit, lower, m.is_odd(), v.neg_))
        m += xuint(1u);
    if (m.is_zero())
        return xint(0);
    return v.neg_ ? -xint(m) : xint(m);
}

template <unsigned Bits>
xint xreal<Bits>::trunc() const {
    return rounded_integer(*this, rnd::toward_zero);
}

template <unsigned Bits>
xint xreal<Bits>::floor() const {
    return rounded_integer(*this, rnd::toward_negative);
}

template <unsigned Bits>
xint xreal<Bits>::ceil() const {
    return rounded_integer(*this, rnd::toward_positive);
}

template <unsigned Bits>
xint xreal<Bits>::round() const {
    return rounded_integer(*this, rnd::nearest_away);
}

template <unsigned Bits>
xint xreal<Bits>::rint() const {
    return rounded_integer(*this, rnd::nearest_even);
}

template <unsigned Bits>
xreal<Bits> xreal<Bits>::frac() const {
    if (inf_)
        throw std::domain_error("alt::xreal: an infinity has no fractional part");
    if (m_.is_zero() || e_ >= 0)
        return zero(neg_); // the value is a whole number, the sign is kept
    const std::size_t drop = std::size_t(-e_);
    xuint rest = m_;
    if (drop < rest.bit_size())
        rest &= (xuint(1u) << drop) - xuint(1u); // the bits behind the point
    return build(false, neg_, rest, e_, default_rounding);
}

template <unsigned Bits>
template <std::integral T>
std::optional<T> xreal<Bits>::to(rnd mode) const {
    if (inf_)
        return std::nullopt;
    return rounded_integer(*this, mode).template to<T>();
}

// ---------------------------------------------------------------------------
// Text: parsing.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xreal<Bits> xreal<Bits>::from_string(std::string_view text, rnd mode) {
    while (!text.empty() && xdetail::is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && xdetail::is_space(text.back()))
        text.remove_suffix(1);

    bool neg = false;
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
        neg = text.front() == '-';
        text.remove_prefix(1);
    }
    if (xdetail::word_is(text, "inf") || xdetail::word_is(text, "infinity"))
        return infinity(neg);
    if (xdetail::word_is(text, "nan"))
        throw std::invalid_argument("alt::xreal::from_string: this type has no NaN");

    unsigned base = 10u;
    unsigned per_digit = 1u; // binary bits worth one mantissa digit
    if (text.size() > 2 && text[0] == '0' && xdetail::lower_of(text[1]) == 'x') {
        base = 16u;
        per_digit = 4u;
        text.remove_prefix(2);
    } else if (text.size() > 2 && text[0] == '0' && xdetail::lower_of(text[1]) == 'b') {
        base = 2u;
        per_digit = 1u;
        text.remove_prefix(2);
    }
    // 'e' is a hexadecimal digit, so the two binary bases take the C99 'p' and
    // the exponent itself is always written in decimal digits.
    const char marker = base == 10u ? 'e' : 'p';
    std::size_t at = 0;
    for (; at < text.size(); ++at)
        if (xdetail::lower_of(text[at]) == marker)
            break;
    const std::string_view body = text.substr(0, at);
    const std::ptrdiff_t power =
        at < text.size() ? xdetail::read_exponent(text.substr(at + 1)) : 0;

    std::string digits;
    std::size_t frac = 0;
    bool point = false, any = false;
    for (const char c : body) {
        if (c == '.') {
            if (point)
                throw std::invalid_argument("alt::xreal::from_string: two radix points");
            point = true;
            continue;
        }
        const int d = xdetail::digit_of(c);
        if (d < 0 || unsigned(d) >= base)
            throw std::invalid_argument("alt::xreal::from_string: digit outside the radix");
        any = true;
        digits.push_back(xdetail::lower_of(c));
        if (point)
            ++frac;
    }
    if (!any)
        throw std::invalid_argument("alt::xreal::from_string: no digits");

    // Leading zeros say nothing, trailing ones only move the exponent.
    const std::size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos)
        return zero(neg); // zero times any power of the radix is still zero
    digits.erase(0, first);
    std::size_t dropped = 0;
    while (digits.size() > 1 && digits.back() == '0') {
        digits.pop_back();
        ++dropped;
    }
    const xuint mantissa = xuint::from_string(digits, base);
    const std::ptrdiff_t places =
        std::ptrdiff_t(dropped - frac) * std::ptrdiff_t(per_digit);

    if (base != 10u) {
        // Here the marker exponent already counts powers of two, while every
        // digit of the mantissa is worth a whole group of bits.
        return build(false, neg, mantissa, xdetail::clamp_exp(power + places), mode);
    }
    const std::ptrdiff_t scale =
        xdetail::clamp_exp(power + std::ptrdiff_t(dropped - frac));

    // A decimal exponent that far out cannot be balanced by any mantissa the
    // text could possibly carry, so the range decides without any big work.
    const long double estimate = static_cast<long double>(mantissa.bit_size() - 1) +
                                 static_cast<long double>(scale) * 3.32192809488736234787l;
    if (estimate > static_cast<long double>(exponent_limit) + 64.0l)
        return overflowed(neg, mode);
    if (estimate < -(static_cast<long double>(exponent_limit) + 64.0l))
        return underflowed(xuint(1u), -exponent_limit - 2, neg, mode);

    if (scale == 0)
        return build(false, neg, mantissa, 0, mode);
    if (scale > 0) {
        // 10^k is 5^k shifted by k bits, and the shift is free.
        xuint m = mantissa * alt::pow(xuint(5u), xdetail::exponent_count(scale));
        return build(false, neg, m, scale, mode);
    }
    // Divide exactly into a window that is wide enough for the decision: the
    // quotient has to keep two bits below the mantissa width, so that the
    // remainder can be folded into its lowest bit as a sticky without touching
    // the highest discarded one.
    const xuint five = alt::pow(xuint(5u), xdetail::exponent_count(-scale));
    const std::size_t want = std::size_t(Bits) + 3u;
    std::size_t shift = 0;
    for (;;) {
        xuint scaled = mantissa;
        scaled <<= shift;
        xuint q, r;
        xuint::divrem(scaled, five, q, r);
        const std::size_t n = q.bit_size();
        if (n >= want) {
            if (!r.is_zero())
                q |= xuint(1u);
            return build(false, neg, q, xdetail::clamp_exp(scale - std::ptrdiff_t(shift)),
                         mode);
        }
        shift += want - n;
    }
}

// ---------------------------------------------------------------------------
// Text: the exact binary forms.
// ---------------------------------------------------------------------------

namespace xdetail {

// The digits behind the leading one of m, padded to whole `group` bit groups.
inline std::string tail_bits(const xuint& m, std::size_t group) {
    const std::size_t rest = m.bit_size() - 1;
    const std::size_t units = (rest + group - 1) / group;
    if (units == 0)
        return std::string();
    xuint rest_value = m - (xuint(1u) << rest); // the leading one is not part of it
    rest_value <<= units * group - rest;
    std::string text = rest_value.to_string(group == 4u ? 16u : 2u);
    // The conversion drops the leading zeros, which are part of the form.
    if (text.size() < units * (group == 4u ? 1u : group))
        text.insert(text.begin(), units * (group == 4u ? 1u : group) - text.size(), '0');
    return text;
}

} // namespace xdetail

template <unsigned Bits>
std::string xreal<Bits>::to_hex_string(bool upper) const {
    const std::string sign = neg_ ? "-" : "";
    if (inf_)
        return sign + (upper ? "INF" : "inf");
    if (m_.is_zero())
        return sign + (upper ? "0X0P+0" : "0x0p+0");
    const std::string tail = xdetail::tail_bits(m_, 4u);
    const std::string point = tail.empty() ? "" : ".";
    return sign + (upper ? "0X1" : "0x1") + point + (upper ? xdetail::upper(tail) : tail) +
           xdetail::exponent_text(ilogb(), upper ? 'P' : 'p', false);
}

template <unsigned Bits>
std::string xreal<Bits>::to_binary_string() const {
    const std::string sign = neg_ ? "-" : "";
    if (inf_)
        return sign + "inf";
    if (m_.is_zero())
        return sign + "0b0p+0";
    const std::string tail = xdetail::tail_bits(m_, 1u);
    const std::string point = tail.empty() ? "" : ".";
    return sign + "0b1" + point + tail + xdetail::exponent_text(ilogb(), 'p', false);
}

// ---------------------------------------------------------------------------
// Text: decimal output.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xdetail::decimal_form xreal<Bits>::decimal_digits() const {
    // The expansion is always finite: the value is an integer for a non
    // negative exponent, and M * 5^n / 10^n otherwise, exact because 2^n * 5^n
    // is 10^n.  Both forms need an integer as long as the value has digits.
    const long double log10_of_2 = 0.3010299956639811952137389l;
    const long double log10_of_5 = 0.6989700043360188047862611l;
    const std::size_t n = e_ >= 0 ? 0 : std::size_t(-e_);
    const long double moved = static_cast<long double>(e_ >= 0 ? std::size_t(e_) : n);
    const std::size_t estimate =
        std::size_t(static_cast<long double>(m_.bit_size()) * log10_of_2 +
                    moved * (e_ >= 0 ? log10_of_2 : log10_of_5)) +
        2u;
    if (estimate > xdetail::max_decimal_digits)
        throw std::overflow_error(
            "alt::xreal::to_string: value too far from zero for a decimal conversion");

    xuint numerator = m_;
    if (n != 0)
        numerator *= alt::pow(xuint(5u), xdetail::exponent_count(-e_));
    else if (e_ > 0)
        numerator <<= std::size_t(e_);

    xdetail::decimal_form f;
    f.digits = numerator.to_string(10u);
    // The position of the highest digit follows the length before the trailing
    // zeros went away, the value itself did not change.
    f.significant = f.digits.size();
    f.exp = std::ptrdiff_t(f.significant) - 1 - std::ptrdiff_t(n);
    while (f.digits.size() > 1 && f.digits.back() == '0')
        f.digits.pop_back();
    return f;
}

template <unsigned Bits>
std::string xreal<Bits>::render_fixed(const xdetail::decimal_form& f, std::size_t places,
                                     bool keep_point) {
    std::string head, tail;
    if (f.exp >= 0) {
        const std::size_t whole = std::size_t(f.exp) + 1u;
        head = f.digits.size() >= whole
                   ? f.digits.substr(0, whole)
                   : f.digits + std::string(whole - f.digits.size(), '0');
        tail = f.digits.size() > whole ? f.digits.substr(whole) : std::string();
    } else {
        head = "0";
        tail = std::string(std::size_t(-f.exp) - 1u, '0') + f.digits;
    }
    if (places == xdetail::all_places)
        places = tail.size();
    else if (tail.size() > places)
        tail.resize(places); // the rounding came first, so this is only a safety net
    else
        tail.append(places - tail.size(), '0');
    if (places == 0 && !keep_point)
        return head;
    return head + "." + tail;
}

template <unsigned Bits>
std::string xreal<Bits>::render_scientific(const xdetail::decimal_form& f,
                                          std::size_t places, bool keep_point) {
    std::string tail = f.digits.size() > 1 ? f.digits.substr(1) : std::string();
    if (places == xdetail::all_places)
        places = tail.size();
    else if (tail.size() > places)
        tail.resize(places);
    else
        tail.append(places - tail.size(), '0');
    std::string out = f.digits.substr(0, 1);
    if (places != 0 || keep_point)
        out += "." + tail;
    return out + xdetail::exponent_text(f.exp, 'e');
}

template <unsigned Bits>
unsigned xreal<Bits>::shortest_digits() const {
    if (inf_ || m_.is_zero())
        return 1u;
    // Bits bits of mantissa never need more than ceil(Bits * log10(2)) + 1
    // decimal digits, so looking no further than that is enough to find the
    // shortest representation that comes back on its own.
    const unsigned limit = unsigned((std::uint64_t(Bits) * 1233u) / 4096u + 3u);
    const xdetail::decimal_form exact = decimal_digits();
    const std::string sign = neg_ ? "-" : "";
    for (unsigned k = 1u; k <= limit; ++k) {
        xdetail::decimal_form t = exact;
        xdetail::round_decimal(t, k);
        std::string text = sign + t.digits.substr(0, 1);
        if (t.digits.size() > 1u)
            text += "." + t.digits.substr(1);
        text += xdetail::exponent_text(t.exp, 'e', false);
        if (from_string(text) == *this)
            return k;
    }
    return limit;
}

template <unsigned Bits>
std::string xreal<Bits>::to_string(unsigned digits, char fmt) const {
    const char kind = xdetail::lower_of(fmt);
    if (kind != 'f' && kind != 'e' && kind != 'g' && kind != 'p' && kind != 'x')
        throw std::invalid_argument("alt::xreal::to_string: unsupported format");
    if (digits > unsigned(xdetail::max_decimal_digits))
        throw std::invalid_argument("alt::xreal::to_string: too many digits requested");
    const bool upper = kind != fmt;
    if (kind == 'x')
        return to_hex_string(upper);

    const std::string sign = neg_ ? "-" : "";
    if (inf_)
        return sign + (upper ? "INF" : "inf");
    if (m_.is_zero()) {
        // The sign of a zero survives, like printf keeps it.
        if (kind == 'g' || kind == 'p')
            return sign + "0";
        const std::string body =
            digits != 0u ? std::string("0.") + std::string(digits, '0') : std::string("0");
        return sign + body + (kind == 'e' ? xdetail::exponent_text(0, 'e') : "");
    }

    xdetail::decimal_form f = decimal_digits();
    if (kind == 'f') {
        // The digits are rounded at the position of the last one that is
        // printed, so a value below half of it turns into a plain zero.
        if (digits != 0u && !xdetail::round_at(f, -std::ptrdiff_t(digits)))
            return sign + "0." + std::string(digits, '0');
        return sign + render_fixed(f, digits != 0u ? std::size_t(digits)
                                                   : xdetail::all_places,
                                  false);
    }
    if (kind == 'e') {
        if (digits != 0u)
            xdetail::round_decimal(f, std::size_t(digits) + 1u);
        return sign + render_scientific(f, digits != 0u ? std::size_t(digits)
                                                        : xdetail::all_places,
                                       false);
    }
    if (kind == 'p') {
        // The fewest digits that read back as this very value.  A number of
        // digits of its own is not wanted here, the point of the exercise being
        // that the value decides how many it needs.
        xdetail::round_decimal(f, shortest_digits());
        const bool scientific = f.exp < -4 || f.exp >= 6; // the window of %g
        return sign + (scientific ? render_scientific(f, xdetail::all_places, false)
                                  : render_fixed(f, xdetail::all_places, false));
    }
    // %g: the shorter of the two, without the zeros that mean nothing.
    const std::size_t significant = digits != 0u ? std::size_t(digits) : f.significant;
    if (digits != 0u)
        xdetail::round_decimal(f, std::size_t(digits));
    const bool scientific = f.exp < -4 || f.exp >= std::ptrdiff_t(significant);
    return sign + (scientific ? render_scientific(f, xdetail::all_places, false)
                              : render_fixed(f, xdetail::all_places, false));
}

// ---------------------------------------------------------------------------
// Streams.  The field of the stream picks the style: fixed for the plain
// notation, scientific for the exponential one, hexfloat or hex for the exact
// binary form, and nothing set for the general one.  The precision is the digit
// count of to_string(), so a precision of zero prints every single digit the
// mantissa holds; the hexadecimal form is exact and does not ask for one.
// ---------------------------------------------------------------------------

template <unsigned Bits>
std::ostream& operator<<(std::ostream& os, const xreal<Bits>& v) {
    char kind = 'g';
    const std::ios_base::fmtflags field = os.flags() & std::ios_base::floatfield;
    if (field == std::ios_base::fixed)
        kind = 'f';
    else if (field == std::ios_base::scientific)
        kind = 'e';
    else if (field == (std::ios_base::fixed | std::ios_base::scientific))
        kind = 'x'; // hexfloat
    else if ((os.flags() & std::ios_base::basefield) == std::ios_base::hex)
        kind = 'x';
    const std::streamsize precision = os.precision();
    std::string body = v.to_string(precision > 0 ? unsigned(precision) : 0u, kind);

    std::string prefix;
    if (!body.empty() && (body.front() == '-' || body.front() == '+')) {
        prefix = body.substr(0, 1);
        body.erase(body.begin());
    } else if (os.flags() & std::ios_base::showpos) {
        prefix = "+";
    }
    alt::detail::write_padded(os, prefix, body);
    return os;
}

template <unsigned Bits>
std::istream& operator>>(std::istream& is, xreal<Bits>& v) {
    std::string token;
    if (!(is >> token))
        return is;
    if ((is.flags() & std::ios_base::basefield) == std::ios_base::hex) {
        // A stream in hexadecimal mode reads the mantissa digits without the
        // mark, just like a built in hexadecimal float does.
        std::size_t at = (!token.empty() && (token.front() == '+' || token.front() == '-'))
                             ? 1u
                             : 0u;
        const bool marked = token.size() > at + 1 && token[at] == '0' &&
                            (xdetail::lower_of(token[at + 1]) == 'x' ||
                             xdetail::lower_of(token[at + 1]) == 'b');
        if (!marked)
            token.insert(at, "0x");
    }
    try {
        v = xreal<Bits>::from_string(token);
    } catch (const std::invalid_argument&) {
        is.setstate(std::ios_base::failbit);
    }
    return is;
}

// ---------------------------------------------------------------------------
// Mathematics
//
// Everything of this section computes on a scratch type that is `guard` bits
// wider than the answer and narrows the outcome in one step, so the published
// value stays within one ulp of the true one.  The rounding mode names the
// direction of that last step.  Where no real number exists - the logarithm of
// a negative value, a pole of the gamma function, an infinity minus itself -
// a std::domain_error is thrown, because the type has no NaN to hand back.
//
// The reductions of the arguments use those guard bits, so an operand of
// extraordinary size (a magnitude beyond 2^64 for the exponential functions, or
// for the trigonometry) costs accuracy in proportion.
// ---------------------------------------------------------------------------

// The magnitude of a value, the sign of a zero falling away as fabs() does it.
template <unsigned Bits>
xreal<Bits> fabs(const xreal<Bits>& v) noexcept {
    return v.signbit() ? -v : v;
}
template <unsigned Bits>
xreal<Bits> abs(const xreal<Bits>& v) noexcept {
    return fabs(v);
}

// The amount of `magnitude` with the sign of `sign`, zeros and infinities included.
template <unsigned Bits>
xreal<Bits> copysign(const xreal<Bits>& magnitude, const xreal<Bits>& sign) noexcept {
    return sign.signbit() ? -fabs(magnitude) : fabs(magnitude);
}

// The integer part of a value.  trunc() cuts the fraction off, floor() and
// ceil() step towards the two infinities, round() breaks a tie away from zero
// and rint() towards the even neighbour - the four directions of the standard
// library.  An infinity comes back as it is, every one of them is exact.
template <unsigned Bits>
xreal<Bits> trunc(const xreal<Bits>& v) {
    if (v.is_infinity() || v.is_zero())
        return v;
    // The integral part of a value that fits into Bits bits fits into them too,
    // so this conversion is exact; only the zero needs its sign back.
    const xreal<Bits> t(v.trunc());
    return t.is_zero() ? xreal<Bits>::zero(v.signbit()) : t;
}
template <unsigned Bits>
xreal<Bits> floor(const xreal<Bits>& v) {
    if (v.is_infinity())
        return v;
    const xreal<Bits> t = trunc(v);
    return (v.signbit() && !v.frac().is_zero()) ? t - xreal<Bits>(1) : t;
}
template <unsigned Bits>
xreal<Bits> ceil(const xreal<Bits>& v) {
    if (v.is_infinity())
        return v;
    const xreal<Bits> t = trunc(v);
    return (!v.signbit() && !v.frac().is_zero()) ? t + xreal<Bits>(1) : t;
}
template <unsigned Bits>
xreal<Bits> round(const xreal<Bits>& v) {
    const xreal<Bits> t = trunc(v);
    const xreal<Bits> part = fabs(v - t);
    if (part.is_zero() || part < xreal<Bits>(0.5))
        return t;
    return t + (v.signbit() ? xreal<Bits>(-1) : xreal<Bits>(1));
}
template <unsigned Bits>
xreal<Bits> rint(const xreal<Bits>& v) {
    if (v.is_infinity() || v.is_zero())
        return v;
    const xreal<Bits> t(v.rint()); // the member nearest integer, ties to even
    return t.is_zero() ? xreal<Bits>::zero(v.signbit()) : t;
}

namespace xdetail {

using exponent_type = std::ptrdiff_t; // what the class itself uses

// The type a computation for T runs in.
template <class T>
using scratch_of = xreal<T::bits + T::guard>;

// A magnitude no term of a series may fall below; two bits under the last one
// that can still change the scratch value.
template <class T>
T series_floor() {
    return T(1).scaled(-exponent_type(T::bits) - 2);
}

// The three trades the elementary functions make between the length of a
// series and the multiplications that shorten its argument: how many times the
// reduced argument of the exponential function is halved before the series and
// squared back afterwards, how many times the leftover of a quadrant is halved
// before the two trigonometric series and doubled back, and how many square
// roots the logarithm may take on its way towards one.  A build may pin any
// of the three - /DAXREAL_EXP_SUBSCALE=n, /DAXREAL_SIN_SUBSCALE=n and
// /DAXREAL_LOG_ROOTS=n - and the ladders of build\math_probe.cpp walked every
// width through exactly those knobs at run time to choose the defaults below.
// The functions are constexpr, so a call passes the number as a constant; the
// argument is the width the series run in, the scratch width bits + guard.
//
// The walks measured: the minimum of the exponential trade lies anywhere
// between 30 and 56 halvings from 224 bits upwards, and between 16 and 36 at
// 160 bits - the curve is so flat across the whole span that one number serves
// every width, and 40 is never more than a few percent off the winner of any
// ladder.  The trigonometric pair, whose doubling costs three multiplies
// rather than one, wants a number of its own per width, 0.75 times the square
// root: 6 at 160 bits, 36 at 2144.  The logarithm balances a root (a square
// root) against the terms of the atanh series it spares (a multiply and a
// division each), and its balance sits at 0.45 times the square root: 6 at 160
// bits, 16 to 24 at 2144, where the old budget spent its whole 64 roots on
// every single call.  The error the doubling back accumulates grows with 2^m;
// at m = 40 it keeps some fifty of the 96 guard bits for the one-ulp promise.
// The whole-number square root by Newton, because the compilers of this
// project's world keep the floating sqrt out of constexpr; two lines and the
// widths here never grow past what it settles in eight steps.
inline constexpr unsigned integer_sqrt(unsigned v) {
    unsigned x = v;
    unsigned y = (x + 1u) / 2u;
    while (y < x) {
        x = y;
        y = (x + v / x) / 2u;
    }
    return x;
}

inline constexpr unsigned exp_subscale(unsigned bits) {
#ifdef AXREAL_EXP_SUBSCALE
    (void)bits;
    return AXREAL_EXP_SUBSCALE;
#else
    (void)bits; // the flat minimum of the walks: one number serves every width
    return 40u;
#endif
}
inline constexpr unsigned sin_subscale(unsigned bits) {
#ifdef AXREAL_SIN_SUBSCALE
    return AXREAL_SIN_SUBSCALE;
#else
    const unsigned m = (3u * integer_sqrt(bits) + 2u) / 4u; // 0.75 sqrt(bits)
    return m < 4u ? 4u : (m > 40u ? 40u : m);
#endif
}
inline constexpr unsigned log_roots(unsigned bits) {
#ifdef AXREAL_LOG_ROOTS
    return AXREAL_LOG_ROOTS;
#else
    const unsigned m = (9u * integer_sqrt(bits) + 10u) / 20u; // 0.45 sqrt(bits)
    return m < 4u ? 4u : (m > 32u ? 32u : m);
#endif
}

// Sums the terms of a series.  `first` is the term of n = 0, `next` turns the
// term of n - 1 into the one of n, and the loop ends at the first term below the
// floor.  Since the first term is always counted, a series whose sum is tiny is
// no problem, and the terms of every series used here shrink from the start.
template <class T, class F>
T sum_series(const T& first, F&& next) {
    AXREAL_STAT(series);
    const T floor = series_floor<T>();
    T total = first;
    T term = first;
    for (unsigned long long n = 1;; ++n) {
        term = next(term, n);
        AXREAL_STAT(terms);
        if (term.is_zero() || fabs(term) <= floor)
            break;
        total += term;
    }
    return total;
}

// 2 atanh(z) = ln((1 + z) / (1 - z)), the series that serves both the logarithm
// and the constants, because z is always made small first.
template <class T>
T ln_from_z(const T& z) {
    const T zz = z * z;
    return sum_series(z, [&zz](const T& term, unsigned long long n) {
               return term * zz * T(2 * n - 1) / T(2 * n + 1);
           }) *
           T(2);
}

// ln 2 = 2 atanh(1/3) and ln 10 = 3 ln 2 + 2 atanh(1/9), the two series that
// reduce the arguments of the exponential functions.  Both are evaluated on the
// scratch width of their type and narrowed once, and they are kept per
// instantiation, so a whole program pays for them once.
template <class T>
const T& ln_two() {
    static const T value = []() {
        using w = scratch_of<T>;
        return T(ln_from_z<w>(w(1) / w(3)));
    }();
    return value;
}
template <class T>
const T& ln_ten() {
    static const T value = []() {
        using w = scratch_of<T>;
        return T(ln_two<w>() * 3 + ln_from_z<w>(w(1) / w(9)));
    }();
    return value;
}

// e, the sum of the reciprocals of the factorials.
template <class T>
const T& Napier() {
    static const T value = []() {
        using w = scratch_of<T>;
        return T(sum_series(w(1), [](const w& term, unsigned long long n) {
            return term / w(n);
        }));
    }();
    return value;
}

// e^x for a small x: the series itself, one multiply and one division by the
// step number per term.
template <class T>
T e_series(const T& r) {
    return sum_series(T(1), [&r](const T& term, unsigned long long n) {
               return term * r / T(n);
           });
}

// e^r for a reduced argument, |x| at most ln 2 / 2 above.  Every term the
// series spares is worth more than the squaring that spared it - a term is a
// multiply and a division of the working width, a squaring is one multiply -
// so the argument goes to r / 2^m, the shorter series is summed, and the
// answer is squared back m times.  The rounding of those squarings grows with
// 2^m and the truncation of the short series with it, which is what the guard
// bits of the scratch width are spent on here.
template <class T>
T e_in_range_doubled(const T& r, unsigned m) {
    T e = e_series(r.scaled(-exponent_type(m)));
    for (unsigned i = 0; i < m; ++i)
        e = e * e;
    return e;
}

template <class T>
T e_in_range(const T& r) {
    return e_in_range_doubled<T>(r, exp_subscale(T::bits));
}

// 2^k for an exact integer k, without ever building the integer itself.
template <class T>
T power_of_two(const T& k) {
    const std::optional<exponent_type> e = k.trunc().to<exponent_type>();
    if (!e.has_value())
        return k.signbit() ? T(0) : T::infinity();
    return T(1).scaled(*e);
}

// e^v for a finite v on the type it is asked for: the nearest whole number of
// doublings plus the series for what the exponent leaves over.  The published
// exp() is this one with the answer narrowed, the hyperbolic functions call it
// directly.
template <class T>
T exp_core(const T& v) {
    const T k = rint(v / ln_two<T>());
    const T r = v - k * ln_two<T>(); // |r| at most ln 2 / 2
    return e_in_range(r) * power_of_two(k);
}

// ln of a positive, finite value: the exponent times ln 2 plus the logarithm of
// a mantissa brought near to one by repeated square roots.  Every root halves
// the distance to one - and with it the number of terms the atanh series needs
// afterwards - while costing a square root, so the loop stops at whichever
// comes first: the mantissa close enough to one that the series needs a term
// or two, or the budget at which one more root costs more than the terms it
// spares.  The probe of build\math_probe.cpp measured that balance as
// log_roots(); the condition 2^-(bits/2) of an older revision was never met at
// any width, and the loop spent its whole budget of roots every single call.
template <class T>
T log_core_roots(const T& v, unsigned budget) {
    const exponent_type e = v.ilogb();
    T s = v.scaled(-e); // 1 <= s < 2
    unsigned roots = 0;
    const T near_one = T(1).scaled(-exponent_type(T::bits) / 2);
    while (roots < budget && fabs(s - T(1)) > near_one) {
        s = T::sqrt(s);
        ++roots;
        AXREAL_STAT(roots);
    }
    T result = ln_from_z<T>((s - T(1)) / (s + T(1)));
    if (roots != 0u)
        result = result.scaled(exponent_type(roots));
    if (e != 0)
        result = result + T(e) * ln_two<T>();
    return result;
}

template <class T>
T log_core(const T& v) {
    return log_core_roots<T>(v, log_roots(T::bits));
}

// ln(1 + x) for a x above -1: the atanh series while the value is near one, the
// full logarithm beyond that.  The inverse hyperbolic functions need it on the
// scratch type, so it lives here next to log_core().
template <class T>
T log1p_core(const T& x) {
    if (x <= T(-1))
        throw std::domain_error("alt::xreal: the logarithm of a value at or below -1");
    // The series of atanh(z) with z = x / (2 + x) needs z to stay well below one:
    // at half they are a third apart, which costs a term per two bits.  Nearer to
    // -1 than that the quotient creeps towards one and the sum stalls, so the
    // value is taken by the full logarithm instead - which loses nothing, since
    // the sum 1 + x is formed exactly and rounded once, however small it is.
    if (fabs(x) <= T(1) / T(2))
        return ln_from_z<T>(x / (T(2) + x)); // exact, free of any cancellation
    return log_core<T>(T(1) + x);
}

// pi by the Chudnovsky series, which adds fourteen digits per term.  The ratio
// of two terms is a quotient of two exact integers, so nothing but the series
// itself decides about the accuracy, and the loop ends when a term falls below
// the last two bits of the scratch value.
template <class T>
const T& circular_pi() {
    static const T value = []() {
        using w = scratch_of<T>;
        constexpr unsigned long long radix = 262537412640768000ull;
        // The summand of the k = 0 term is 13591409 on its own, and the ratio of
        // two summands carries the quotient of the two linear factors as well.
        w term(13591409);
        w sum(13591409);
        for (unsigned long long k = 0;; ++k) {
            const unsigned long long next = 13591409ull + 545140134ull * (k + 1ull);
            const unsigned long long here = 13591409ull + 545140134ull * k;
            const xuint num = xuint(6 * k + 1) * xuint(6 * k + 2) * xuint(6 * k + 3) *
                              xuint(6 * k + 4) * xuint(6 * k + 5) * xuint(6 * k + 6) *
                              xuint(next);
            const xuint den = xuint(3 * k + 1) * xuint(3 * k + 2) * xuint(3 * k + 3) *
                              xuint(k + 1) * xuint(k + 1) * xuint(k + 1) *
                              xuint(here) * xuint(radix);
            term = -term * (w(num) / w(den));
            sum += term;
            if (fabs(term) <= fabs(sum).scaled(-exponent_type(w::bits) - 2))
                break;
        }
        return T(w(426880) * w::sqrt(w(10005)) / sum);
    }();
    return value;
}

// v^e for a positive v: the logarithm times the exponent, taken back through
// the exponential function, which is what reduces the result to a power of two.
template <class T>
T pow_core(const T& v, const T& e) {
    const T t = log_core<T>(v) * e;
    const T k = rint(t / ln_two<T>());
    const T r = t - k * ln_two<T>();
    return e_in_range(r) * power_of_two(k);
}

} // namespace xdetail

// The constants.  Every one of them is computed with the guard bits of its type
// and is therefore good for the full width of that type.
template <any_xreal T>
const T& pi() {
    return xdetail::circular_pi<T>();
}
template <any_xreal T>
const T& Napier() {
    return xdetail::Napier<T>();
}
template <any_xreal T>
const T& ln2() {
    return xdetail::ln_two<T>();
}
template <any_xreal T>
const T& ln10() {
    return xdetail::ln_ten<T>();
}

// e raised to a value; exp2() and exp10() are the same function with a scaled
// argument, expm1() keeps the digits of a value close to one.
template <unsigned Bits>
xreal<Bits> exp(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>(1);
    if (v.is_infinity())
        return v.signbit() ? xreal<Bits>::zero() : xreal<Bits>::infinity();
    const w x(v);
    return xreal<Bits>(xdetail::exp_core(x), mode);
}

template <unsigned Bits>
xreal<Bits> exp2(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>(1);
    if (v.is_infinity())
        return v.signbit() ? xreal<Bits>::zero() : xreal<Bits>::infinity();
    const w x(v);
    const w k = rint(x);
    const w r = x - k;
    return xreal<Bits>(xdetail::e_in_range(r * xdetail::ln_two<w>()) *
                           xdetail::power_of_two(k),
                       mode);
}

template <unsigned Bits>
xreal<Bits> exp10(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>(1);
    if (v.is_infinity())
        return v.signbit() ? xreal<Bits>::zero() : xreal<Bits>::infinity();
    const w x(v);
    // The power of two has to come out of the natural exponent, so it is the
    // quotient ln 2 / ln 10 that turns the argument into the number of doublings.
    const w halves = xdetail::ln_two<w>() / xdetail::ln_ten<w>(); // log_10 of 2
    const w k = rint(x / halves);
    const w r = (x - k * halves) * xdetail::ln_ten<w>(); // |r| at most ln 2 / 2
    return xreal<Bits>(xdetail::e_in_range(r) * xdetail::power_of_two(k), mode);
}

template <unsigned Bits>
xreal<Bits> expm1(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        return v.signbit() ? xreal<Bits>(-1) : xreal<Bits>::infinity();
    const w x(v);
    if (fabs(x) <= w(1)) {
        // x + x^2/2! + x^3/3! ... : every term is smaller than the one before,
        // and no subtraction happens, so the leading digits of a small x stay.
        return xreal<Bits>(xdetail::sum_series(
                               x, [&x](const w& term, unsigned long long n) {
                                   return term * x / w(n + 1);
                               }),
                           mode);
    }
    const w k = rint(x / xdetail::ln_two<w>());
    const w r = x - k * xdetail::ln_two<w>();
    const w grown = xdetail::e_in_range(r);
    // e^x - 1 = (e^r - 1) * 2^k + (2^k - 1) keeps every leading digit.
    const w less = grown - w(1);
    const w whole = xdetail::power_of_two(k);
    return xreal<Bits>(less * whole + (whole - w(1)), mode);
}

// The logarithms.  log() is the natural one, log2() and log10() scale it by the
// reciprocal of ln 2 and ln 10, and log1p() takes a value close to zero.
template <unsigned Bits>
xreal<Bits> log(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>::infinity(true); // the limit of log(x) as x goes to 0
    if (v.signbit())
        throw std::domain_error("alt::log of a negative value");
    if (v.is_infinity())
        return v;
    return xreal<Bits>(xdetail::log_core(w(v)), mode);
}

template <unsigned Bits>
xreal<Bits> log2(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>::infinity(true);
    if (v.signbit())
        throw std::domain_error("alt::log2 of a negative value");
    if (v.is_infinity())
        return v;
    // The binary exponent is exact, so only the mantissa needs a series.
    const auto e = v.ilogb();
    const w head = xdetail::log_core(w(v).scaled(-e));
    return xreal<Bits>(head / xdetail::ln_two<w>() + w(e), mode);
}

template <unsigned Bits>
xreal<Bits> log10(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>::infinity(true);
    if (v.signbit())
        throw std::domain_error("alt::log10 of a negative value");
    if (v.is_infinity())
        return v;
    return xreal<Bits>(xdetail::log_core(w(v)) / xdetail::ln_ten<w>(), mode);
}

template <unsigned Bits>
xreal<Bits> log1p(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        return v.signbit() ? xreal<Bits>::infinity(true) : v;
    const w x(v);
    if (x <= w(-1))
        throw std::domain_error("alt::log1p of a value at or below -1");
    if (fabs(x) < w(1)) {
        // ln(1 + x) = 2 atanh(x / (2 + x)), exact and free of any cancellation.
        return xreal<Bits>(xdetail::ln_from_z<w>(x / (w(2) + x)), mode);
    }
    return xreal<Bits>(xdetail::log_core(w(1) + x), mode);
}

// ---------------------------------------------------------------------------
// Remainders, roots and powers.
// ---------------------------------------------------------------------------

namespace xdetail {

// What is left of |x| when the whole multiples of |y| are taken away, as an
// exact integer.  Both values are whole multiples of 2^weight, so the remainder
// of the two mantissas is the remainder of the values themselves, and the
// modular power keeps every temporary about as wide as the mantissa of y -
// whatever the distance of the two exponents is.
struct remain_of {
    xuint rest;                    // the magnitude, weighted by `weight`
    exponent_type weight;          // rest * 2^weight is what is left
    xuint whole;                   // the modulus: rest < whole
    bool half = false;             // rest is exactly half of the modulus
    bool odd = false;              // an odd number of |y| fits into |x|
};

template <unsigned Bits>
remain_of magnitude_remainder(const xreal<Bits>& x, const xreal<Bits>& y) {
    remain_of r;
    const xuint mx = x.mantissa();
    const xuint my = y.mantissa();
    const exponent_type ex = x.exponent();
    const exponent_type ey = y.exponent();
    if (ex >= ey) {
        // |x| is mx * 2^(ex - ey) times the quantum of |y|, and the factor
        // 2^(ex - ey) enters the remainder only through its residue class.
        const xuint power(std::uint64_t(ex - ey));
        r.weight = ey;
        r.whole = my;
        r.rest = (mx * alt::pow_mod(xuint(2u), power, my)) % my;
        r.half = (r.rest << 1) == my;
        if (r.half) {
            // The parity of the truncated quotient, which is the position of the
            // modulus inside twice the modulus.
            const xuint twice = my << 1;
            const xuint wide = (mx * alt::pow_mod(xuint(2u), power, twice)) % twice;
            r.odd = wide >= my;
        }
        return r;
    }
    // Here every bit of y is worth more than one of x, so the shift of its
    // mantissa stays small as soon as |x| is not below |y| - and every caller
    // has made sure of that before asking.
    const xuint wide = my << std::size_t(ey - ex);
    r.weight = ex;
    r.whole = wide;
    r.rest = mx % wide;
    r.half = (r.rest << 1) == wide;
    if (r.half)
        r.odd = ((mx / wide) & xuint(1u)) != xuint();
    return r;
}

} // namespace xdetail

// The part of x that the whole multiples of y leave over.  The result is a whole
// number of the quantum of y, therefore exactly representable; only a value
// below the smallest one of the type needs the rounding mode at all.
template <unsigned Bits>
xreal<Bits> fmod(const xreal<Bits>& x, const xreal<Bits>& y,
                 rnd mode = rnd::nearest_even) {
    if (x.is_infinity())
        throw std::domain_error("alt::fmod of an infinity has no value");
    if (y.is_zero())
        throw std::domain_error("alt::fmod by zero has no value");
    if (x.is_zero() || y.is_infinity())
        return x; // the sign of a zero is kept, and so is anything below an infinity
    const xreal<Bits> mx = fabs(x), my = fabs(y);
    if (mx < my)
        return x;
    const xdetail::remain_of r = xdetail::magnitude_remainder(mx, my);
    if (r.rest.is_zero())
        return xreal<Bits>::zero(x.signbit()); // C gives the sign of x here
    return xreal<Bits>::from_mantissa(r.rest, r.weight, x.signbit(), mode);
}

// The same part, but measured from the nearest whole multiple instead of the
// truncated one, with a tie to the even multiple - the IEEE remainder.  Exact as
// well; its sign is that of x - n * y, so it differs from fmod() whenever x lies
// more than half of y past a multiple.
template <unsigned Bits>
xreal<Bits> remainder(const xreal<Bits>& x, const xreal<Bits>& y,
                      rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (x.is_infinity() || y.is_zero())
        throw std::domain_error("alt::remainder of these operands has no value");
    if (x.is_zero() || y.is_infinity())
        return x;
    const xreal<Bits> mx = fabs(x), my = fabs(y);
    if (mx < my) {
        // The truncated quotient is zero here, so only the multiple one above
        // can win the race - and it does as soon as x passes half of y.
        const xreal<Bits> twice = mx + mx; // exact, a shift
        if (twice < my)
            return x;
        if (twice > my) {
            const w d = w(my) - w(mx); // exact on the scratch width
            return xreal<Bits>::from_mantissa(d.mantissa(), d.exponent(),
                                              !x.signbit(), mode);
        }
        // Exactly half of y: the two candidates are the multiples zero and one,
        // and the even of them - zero - leaves x itself as the remainder.
        return x;
    }
    const xdetail::remain_of r = xdetail::magnitude_remainder(mx, my);
    const bool beyond_half = r.half ? r.odd : (r.rest << 1) > r.whole;
    if (!beyond_half) {
        if (r.rest.is_zero())
            return xreal<Bits>::zero(x.signbit());
        return xreal<Bits>::from_mantissa(r.rest, r.weight, x.signbit(), mode);
    }
    const xuint back = r.whole - r.rest; // the distance to the multiple above
    return xreal<Bits>::from_mantissa(back, r.weight, !x.signbit(), mode);
}

// The Euclidean norm, scaled first so that no intermediate value can leave the
// range of the type on its own.
template <unsigned Bits>
xreal<Bits> hypot(const xreal<Bits>& a, const xreal<Bits>& b,
                  rnd mode = rnd::nearest_even) {
    if (a.is_infinity() || b.is_infinity())
        return xreal<Bits>::infinity(); // however small the other one is
    using w = xdetail::scratch_of<xreal<Bits>>;
    w x = fabs(w(a)), y = fabs(w(b));
    if (x < y)
        std::swap(x, y);
    if (x.is_zero())
        return xreal<Bits>(0);
    const w t = y / x; // at most one, so its square cannot leave the range
    return xreal<Bits>(x * w::sqrt(w(1) + t * t), mode);
}

// The square root, as the free function of the same name the rest of this
// header and axint.h both offer; the static member it forwards to is where the
// work is done.
template <unsigned Bits>
xreal<Bits> sqrt(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    return xreal<Bits>::sqrt(v, mode);
}

// The real cube root, of a negative value too.
template <unsigned Bits>
xreal<Bits> cbrt(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    if (v.is_zero() || v.is_infinity())
        return v;
    const xuint m = v.mantissa();
    const auto e = v.exponent();
    // m * 2^e == (m * 2^j)^(1/3) * 2^((e - j) / 3) for every j that leaves
    // (e - j) divisible by three; j lifts the radicand until its root holds the
    // two bits below the mantissa that the rounding of a tie has to know about.
    std::ptrdiff_t j = std::ptrdiff_t(3u * (Bits + 2u)) - std::ptrdiff_t(m.bit_size());
    if (j < 0)
        j = 0;
    while (((e - j) % 3) != 0)
        ++j;
    const xuint radicand = m << std::size_t(j);
    xuint r = alt::iroot(radicand, 3u);
    if (alt::pow(r, 3u) != radicand)
        r |= xuint(1u); // sticky: the root is not a whole number
    return xreal<Bits>::from_mantissa(r, (e - j) / 3, v.signbit(), mode);
}

// The real n-th root.  An even degree of a negative value has no real value, the
// odd ones keep the sign, exactly like cbrt().
template <unsigned Bits>
xreal<Bits> root(const xreal<Bits>& v, unsigned n, rnd mode = rnd::nearest_even) {
    if (n == 0u)
        throw std::domain_error("alt::root of degree zero has no value");
    if (n == 1u)
        return v;
    if (n == 2u)
        return xreal<Bits>::sqrt(v, mode);
    if (v.is_zero())
        return v; // a signed zero keeps its sign, as the square root does
    if (v.signbit() && (n & 1u) == 0u)
        throw std::domain_error("alt::root: an even degree of a negative value");
    if (v.is_infinity())
        return v; // an odd root of an infinity is that very infinity
    if (n > 64u) {
        // The integer route below lifts the radicand to n * Bits bits, which no
        // longer makes sense for a degree that large, so it goes the
        // transcendental way: within an ulp of the root, not necessarily the
        // representable value closest to it.
        using w = xdetail::scratch_of<xreal<Bits>>;
        const w part = xdetail::pow_core(fabs(w(v)), w(1) / w(n));
        const xreal<Bits> back = part.rescale<Bits>(mode);
        return v.signbit() ? -back : back;
    }
    const xuint m = v.mantissa();
    const auto e = v.exponent();
    std::ptrdiff_t j = std::ptrdiff_t(n) * std::ptrdiff_t(Bits + 2u) -
                       std::ptrdiff_t(m.bit_size());
    if (j < 0)
        j = 0;
    while (((e - j) % std::ptrdiff_t(n)) != 0)
        ++j; // the exponent of the root has to stay a whole number
    const xuint radicand = m << std::size_t(j);
    xuint r = alt::iroot(radicand, n);
    if (alt::pow(r, n) != radicand)
        r |= xuint(1u);
    return xreal<Bits>::from_mantissa(r, (e - j) / std::ptrdiff_t(n), v.signbit(),
                                      mode);
}

// x raised to a whole number, by repeated squaring on the scratch type.  Every
// squaring costs at most half an ulp of that width, so the guard bits cover an
// exponent far beyond anything a program will actually use.
template <unsigned Bits, std::integral T>
xreal<Bits> pow(const xreal<Bits>& x, T n, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (n == T(0))
        return xreal<Bits>(1); // 0 to the power 0 is one here, as in C
    const std::uint64_t bare = static_cast<std::uint64_t>(
        static_cast<std::make_unsigned_t<T>>(n)); // the low bits of |n|
    const bool flip = x.signbit() && (bare & 1ull) != 0ull;
    bool upwards = true;
    std::uint64_t left;
    if constexpr (std::is_signed_v<T>) {
        upwards = n > T(0);
        using u = std::make_unsigned_t<T>;
        left = static_cast<std::uint64_t>(upwards ? static_cast<u>(n)
                                                  : u(u(0) - static_cast<u>(n)));
    } else {
        left = static_cast<std::uint64_t>(n);
    }
    if (x.is_infinity())
        return upwards ? xreal<Bits>::infinity(flip) : xreal<Bits>::zero(flip);
    if (x.is_zero())
        return upwards ? xreal<Bits>::zero(flip) : xreal<Bits>::infinity(flip);

    w part(x);
    w whole(1);
    while (left != 0ull) {
        if ((left & 1ull) != 0ull)
            whole *= part;
        left >>= 1;
        if (left != 0ull)
            part *= part;
    }
    if (!upwards)
        whole = w(1) / whole; // a zero base was answered above
    if (whole.is_infinity())
        return xreal<Bits>::infinity(whole.signbit());
    if (whole.is_zero())
        return xreal<Bits>::zero(whole.signbit());
    return xreal<Bits>::from_mantissa(whole.mantissa(), whole.exponent(),
                                      whole.signbit(), mode);
}

// x raised to y.  A whole exponent - whatever type it is written in - takes the
// route above, everything else exp(y * log(x)).  A negative base with an
// exponent that is not a whole number has no real value, so it throws instead of
// handing back a NaN.
template <unsigned Bits>
xreal<Bits> pow(const xreal<Bits>& x, const xreal<Bits>& y,
                rnd mode = rnd::nearest_even) {
    if (y.is_zero())
        return xreal<Bits>(1); // anything to the power of zero
    if (x == xreal<Bits>(1))
        return xreal<Bits>(1); // whatever the exponent is
    bool whole = false, odd = false;
    std::optional<long long> small;
    if (y.is_finite()) {
        const xint part = y.trunc();
        if (xreal<Bits>(part) == y) { // the exponent really is a whole number
            whole = true;
            odd = part.is_odd();
            small = part.to<long long>();
        }
    }
    // Only a finite exponent can fail to be a whole number; an infinite one is
    // the limit of the whole exponents around it, so a negative base with such an
    // exponent has a value - and no sign of its own, like C's pow().
    if (x.signbit() && !whole && y.is_finite())
        throw std::domain_error("alt::pow: a negative base with an exponent that "
                                "is not a whole number has no real value");
    const bool flip = x.signbit() && odd;
    const xreal<Bits> ax = fabs(x);
    if (y.is_infinity()) {
        // Away from zero above one, towards it below; an exponent of the other
        // sign turns the outcome around.
        const bool outwards = (ax > xreal<Bits>(1)) == !y.signbit();
        return outwards ? xreal<Bits>::infinity(flip) : xreal<Bits>::zero(flip);
    }
    if (ax.is_zero())
        return y.signbit() ? xreal<Bits>::infinity(flip) : xreal<Bits>::zero(flip);
    if (ax.is_infinity())
        return y.signbit() ? xreal<Bits>::zero(flip) : xreal<Bits>::infinity(flip);
    if (whole && small.has_value())
        return pow(x, *small, mode);
    using w = xdetail::scratch_of<xreal<Bits>>;
    const w part = xdetail::pow_core(w(ax), w(y));
    if (part.is_infinity())
        return xreal<Bits>::infinity(flip);
    if (part.is_zero())
        return xreal<Bits>::zero(flip);
    return xreal<Bits>::from_mantissa(part.mantissa(), part.exponent(), flip, mode);
}

// The same power of a whole number that does not fit into a machine word.
template <unsigned Bits>
xreal<Bits> pow(const xreal<Bits>& x, const xint& n, rnd mode = rnd::nearest_even) {
    if (const auto part = n.to<long long>())
        return pow(x, *part, mode);
    return pow(x, xreal<Bits>(n), mode);
}

// ---------------------------------------------------------------------------
// The trigonometric functions.
//
// A value is first reduced to a multiple of a quarter circle plus a leftover of
// at most pi/4, and only that leftover enters the series, whose every term is
// therefore a tenth or less of the one before it.  The reduction happens on the
// scratch type, so an operand of an extraordinary magnitude costs accuracy in
// proportion to its size - the same price the exponential functions pay.
// ---------------------------------------------------------------------------

namespace xdetail {

// The quarter circle, pi / 2, which is the modulus of the reduction.
template <class T>
const T& quarter_circle() {
    static const T value = circular_pi<T>() / T(2);
    return value;
}

// sin and cos of an argument of at most a quarter circle.  The two factors of
// a denominator fit into one word, so the term divides by their product: one
// division instead of two, and one rounding fewer.
template <class T>
T sin_series(const T& x) {
    const T xx = x * x;
    return sum_series(x, [&xx](const T& term, unsigned long long n) {
               return -term * xx / T((2 * n) * (2 * n + 1));
           });
}
template <class T>
T cos_series(const T& x) {
    const T xx = x * x;
    return sum_series(T(1), [&xx](const T& term, unsigned long long n) {
               return -term * xx / T((2 * n - 1) * (2 * n));
           });
}

// How many quarter circles a value is away from zero - kept modulo four, which
// is all the quadrant needs - and what is left of it once they are subtracted.
// The leftover lies between -pi/4 and pi/4, because the count is rounded.
template <class T>
struct quadrant_of {
    int quadrant;
    T rest;
};

template <class T>
quadrant_of<T> quarter_count(const T& v) {
    const T turns = rint(v / quarter_circle<T>());
    const xint counted = turns.trunc();
    // The remainder of a negative count is negative in C++, and the four
    // quadrants are numbered from zero on.
    int q = (counted % xint(4)).to<int>().value_or(0);
    if (q < 0)
        q += 4;
    return {q, v - turns * quarter_circle<T>()};
}

// The sine and the cosine of the same value, which need one reduction and the
// pair of series anyway.  The leftover of the reduction is halved m times
// before the two series and the pair is doubled back m times afterwards by the
// double angle formulas - the same trade the exponential function makes, and
// paid for in the same guard bits.  The cosine doubles as 1 - 2 s^2 rather
// than c^2 - s^2: it is one multiply cheaper, and near a cosine of one the
// subtraction has no digits to lose.
template <class T>
std::pair<T, T> sine_cosine_doubled(const T& v, unsigned m) {
    const quadrant_of<T> q = quarter_count(v);
    const T rest = q.rest.scaled(-exponent_type(m));
    T s = sin_series(rest);
    T c = cos_series(rest);
    for (unsigned i = 0; i < m; ++i) {
        const T doubled_s = (s * c).scaled(1);
        c = T(1) - (s * s).scaled(1);
        s = doubled_s;
    }
    switch (q.quadrant) {
        case 1: return {c, -s};  // a quarter circle past that is a swapped pair
        case 2: return {-s, -c};
        case 3: return {-c, s};
        default: return {s, c};
    }
}

template <class T>
std::pair<T, T> sine_cosine_of(const T& v) {
    return sine_cosine_doubled<T>(v, sin_subscale(T::bits));
}

// atan of an argument of at most one half, by the series whose terms shrink by
// at least a factor of four.
template <class T>
T atan_series(const T& t) {
    const T tt = t * t;
    return sum_series(t, [&tt](const T& term, unsigned long long n) {
               return -term * tt * T(2 * n - 1) / T(2 * n + 1);
           });
}

// atan of an argument between -1 and 1.  The halving formula
//     atan(t) = 2 atan( t / (1 + sqrt(1 + t^2)) )
// pulls the argument down to a half, where the series is quick, and doubling the
// result again is an exact scaling.
template <class T>
T atan_small(const T& v) {
    T x = v;
    unsigned steps = 0;
    const T limit = T(1).scaled(-1); // one half
    while (!x.is_zero() && fabs(x) > limit) {
        x = x / (T(1) + T::sqrt(T(1) + x * x));
        ++steps;
    }
    return atan_series(x).scaled(exponent_type(steps));
}

// atan of a value between -1 and 1, and of its reciprocal at the same time:
// above one the complement pi/2 - atan(1/x) is taken, which is where the series
// has anything to say about it.
template <class T>
T atan_core(const T& v) {
    const T a = fabs(v);
    if (a > T(1))
        return quarter_circle<T>() - atan_small(T(1) / a);
    return atan_small(a);
}

// acos of a value between zero and one, in the form that stays accurate right
// where the function becomes small: 2 atan(sqrt((1 - x) / (1 + x))).
template <class T>
T acos_positive(const T& x) {
    if (x == T(1))
        return T(0); // the point where the function has an exact zero
    return atan_small(T::sqrt((T(1) - x) / (T(1) + x))).scaled(1);
}

// asin of a magnitude at most one, near zero through its tangent, further out as
// the complement of the cosine.
template <class T>
T asin_magnitude(const T& a) {
    if (a <= T(1).scaled(-1))
        return atan_small(a / T::sqrt(T(1) - a * a));
    return quarter_circle<T>() - acos_positive(a);
}

// The two series of the hyperbolic sine and cosine, the sine and cosine series
// without the alternating signs.
template <class T>
T sinh_series(const T& x) {
    const T xx = x * x;
    return sum_series(x, [&xx](const T& term, unsigned long long n) {
               return term * xx / T(2 * n) / T(2 * n + 1);
           });
}
template <class T>
T cosh_series(const T& x) {
    const T xx = x * x;
    return sum_series(T(1), [&xx](const T& term, unsigned long long n) {
               return term * xx / T(2 * n - 1) / T(2 * n);
           });
}

} // namespace xdetail

// The sine, the cosine and the tangent.  An infinity has no value here, since no
// limit exists for it; a zero comes back with its sign for the sine and the
// tangent, and as one for the cosine.
template <unsigned Bits>
xreal<Bits> sin(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        throw std::domain_error("alt::sin of an infinity has no value");
    return xreal<Bits>(xdetail::sine_cosine_of(w(v)).first, mode);
}

template <unsigned Bits>
xreal<Bits> cos(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>(1);
    if (v.is_infinity())
        throw std::domain_error("alt::cos of an infinity has no value");
    return xreal<Bits>(xdetail::sine_cosine_of(w(v)).second, mode);
}

template <unsigned Bits>
xreal<Bits> tan(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        throw std::domain_error("alt::tan of an infinity has no value");
    const xdetail::quadrant_of<w> q = xdetail::quarter_count(w(v));
    const w s = xdetail::sin_series(q.rest);
    const w c = xdetail::cos_series(q.rest);
    // In the second and the fourth quadrant the tangent is the cotangent of the
    // leftover, and with the opposite sign.
    return xreal<Bits>((q.quadrant & 1) ? -(c / s) : (s / c), mode);
}

// The inverse tangent of a single value; atan2() takes the two halves of a point
// and reports the angle of the point, which is what a program usually wants.
template <unsigned Bits>
xreal<Bits> atan(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity()) {
        const w limit = xdetail::quarter_circle<w>();
        return xreal<Bits>(v.signbit() ? -limit : limit, mode);
    }
    const w a = xdetail::atan_core(w(v));
    return xreal<Bits>(v.signbit() ? -a : a, mode);
}

// The arcsine and the arccosine, of a value between -1 and 1; anything beyond
// that has no real value and throws.
template <unsigned Bits>
xreal<Bits> asin(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        throw std::domain_error("alt::asin of an infinity has no value");
    const w a(fabs(w(v)));
    if (a > w(1))
        throw std::domain_error("alt::asin of a value beyond one has no real value");
    const w r = xdetail::asin_magnitude(a);
    return xreal<Bits>(v.signbit() ? -r : r, mode);
}

template <unsigned Bits>
xreal<Bits> acos(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_infinity())
        throw std::domain_error("alt::acos of an infinity has no value");
    const w a(fabs(w(v)));
    if (a > w(1))
        throw std::domain_error("alt::acos of a value beyond one has no real value");
    const w near = xdetail::acos_positive(a); // between zero and pi/2
    if (!v.signbit())
        return xreal<Bits>(near, mode);
    const w r = xdetail::circular_pi<w>() - near; // the left half of the circle
    return xreal<Bits>(r, mode);
}

// The angle of the point (x, y), between -pi and pi.  The two zeros together
// have no angle and throw; the border cases of C99 are otherwise respected, the
// sign of a zero included.
template <unsigned Bits>
xreal<Bits> atan2(const xreal<Bits>& y, const xreal<Bits>& x,
                  rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (y.is_zero() && x.is_zero())
        throw std::domain_error("alt::atan2 of two zeros has no angle");
    const w pi = xdetail::circular_pi<w>();
    if (y.is_zero()) {
        if (x > xreal<Bits>(0))
            return xreal<Bits>::zero(y.signbit()); // along the positive x axis
        return xreal<Bits>(y.signbit() ? -pi : pi, mode); // and the negative one
    }
    if (x.is_zero())
        return xreal<Bits>(y.signbit() ? -xdetail::quarter_circle<w>()
                                       : xdetail::quarter_circle<w>(),
                           mode);
    if (y.is_infinity() || x.is_infinity()) {
        if (y.is_infinity() && x.is_infinity()) {
            // The two infinities give the four diagonals: a quarter of the circle
            // to the right, three quarters of it to the left, the sign of the
            // ordinate on top of that.
            const w r = x.signbit() ? pi * w(3) / w(4) : pi / w(4);
            return xreal<Bits>(y.signbit() ? -r : r, mode);
        }
        if (y.is_infinity()) {
            const w r = xdetail::quarter_circle<w>();
            return xreal<Bits>(y.signbit() ? -r : r, mode);
        }
        if (x.signbit())
            return xreal<Bits>(y.signbit() ? -pi : pi, mode);
        return xreal<Bits>::zero(y.signbit()); // an infinite x to the right
    }
    const w angle = xdetail::atan_core(fabs(w(y)) / fabs(w(x)));
    const w r = x.signbit() ? pi - angle : angle;
    return xreal<Bits>(y.signbit() ? -r : r, mode);
}

// ---------------------------------------------------------------------------
// The hyperbolic functions.
//
// Near zero the two series are used, so that no digit of a small argument falls
// away, and further out the exponential function - arranged so that it can never
// overflow where the function itself stays finite.
// ---------------------------------------------------------------------------

template <unsigned Bits>
xreal<Bits> sinh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero() || v.is_infinity())
        return v;
    const w x(v);
    if (fabs(x) <= w(1))
        return xreal<Bits>(xdetail::sinh_series(x), mode);
    // (e^x - e^-x) / 2: both halves are at least a third apart in magnitude, so
    // nothing cancels, and an overflow here is an overflow of the function too.
    const w up = xdetail::exp_core(x);
    return xreal<Bits>((up - w(1) / up) / w(2), mode);
}

template <unsigned Bits>
xreal<Bits> cosh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return xreal<Bits>(1);
    if (v.is_infinity())
        return xreal<Bits>::infinity(); // either infinity, the cosine is even
    const w x(v);
    if (fabs(x) <= w(1))
        return xreal<Bits>(xdetail::cosh_series(x), mode);
    const w up = xdetail::exp_core(x);
    return xreal<Bits>((up + w(1) / up) / w(2), mode);
}

template <unsigned Bits>
xreal<Bits> tanh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        return xreal<Bits>(v.signbit() ? -1 : 1); // the two limits of the function
    const w a(fabs(w(v)));
    w r;
    if (a <= w(1)) {
        r = xdetail::sinh_series(a) / xdetail::cosh_series(a);
    } else {
        // (1 - e^-2a) / (1 + e^-2a) with a positive exponent, so the exponential
        // function can only ever run into a zero here, never into an infinity.
        const w back = xdetail::exp_core(-(a + a));
        r = (w(1) - back) / (w(1) + back);
    }
    return xreal<Bits>(v.signbit() ? -r : r, mode);
}

// The inverse hyperbolic functions, all of them logarithms.  The forms below are
// the ones that keep the leading digits of a small argument and stay inside the
// range of the type for an extraordinary large one.
template <unsigned Bits>
xreal<Bits> asinh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero() || v.is_infinity())
        return v;
    const w a(fabs(w(v)));
    w r;
    if (a <= w(1)) {
        // a + sqrt(1 + a^2) = 1 + (a + a^2 / (1 + sqrt(1 + a^2))), which log1p
        // then takes without losing the leading digits.
        const w sq = w::sqrt(w(1) + a * a);
        r = xdetail::log1p_core(a + (a * a) / (w(1) + sq));
    } else {
        // ln(2a) plus the tiny remainder, so that the square root of a * a of
        // the direct form never has to be taken.
        const w u = w(1) / (a * a); // may become a zero, which is the limit
        const w t = (w::sqrt(w(1) + u) - w(1)) / w(2);
        r = xdetail::log_core(a) + xdetail::ln_two<w>() + xdetail::log1p_core(t);
    }
    return xreal<Bits>(v.signbit() ? -r : r, mode);
}

template <unsigned Bits>
xreal<Bits> acosh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_infinity()) {
        // The positive infinity is the limit of the function; the negative one is
        // outside its domain, like any other value below one.
        if (v.signbit())
            throw std::domain_error("alt::acosh of a negative infinity has no real value");
        return v;
    }
    if (v < xreal<Bits>(0))
        throw std::domain_error("alt::acosh of a negative value has no real value");
    const w x(v);
    if (x < w(1))
        throw std::domain_error("alt::acosh below one has no real value");
    if (x == w(1))
        return xreal<Bits>(0);
    w r;
    if (x <= w(2)) {
        // x + sqrt(x^2 - 1) = 1 + (x - 1) + sqrt((x - 1)(x + 1)), the form that
        // keeps the digits of a value close to one.
        const w d = x - w(1);
        r = xdetail::log1p_core(d + w::sqrt(d * (x + w(1))));
    } else {
        const w u = w(1) / (x * x);
        const w t = (w::sqrt(w(1) - u) - w(1)) / w(2);
        r = xdetail::log_core(x) + xdetail::ln_two<w>() + xdetail::log1p_core(t);
    }
    return xreal<Bits>(r, mode);
}

template <unsigned Bits>
xreal<Bits> atanh(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_zero())
        return v;
    if (v.is_infinity())
        throw std::domain_error("alt::atanh of an infinity has no value");
    const w a(fabs(w(v)));
    if (a > w(1))
        throw std::domain_error("alt::atanh of a value beyond one has no real value");
    if (a == w(1))
        return xreal<Bits>::infinity(v.signbit()); // the two poles, of one sign each
    // ln((1 + a) / (1 - a)) / 2, both halves by log1p so that a small a keeps
    // its digits.
    const w r = (xdetail::log1p_core(a) - xdetail::log1p_core(-a)) / w(2);
    return xreal<Bits>(v.signbit() ? -r : r, mode);
}

// ---------------------------------------------------------------------------
// The gamma function and what grows out of it.
//
// Gamma(z + 1) = z Gamma(z) with Gamma(1) = 1 is all that defines it, so the
// whole function follows from its values of a short interval.  That interval is
// reached by lifting the argument with the recurrence - a few dozen
// multiplications - and taking the value there from Stirling's series, whose
// terms are given by the even Bernoulli numbers.  The series is asymptotic: it
// ends at its smallest term, and the size of that term is checked, never merely
// assumed, so the shift is repeated with a double length whenever the estimate
// of it turns out to be too short.
// ---------------------------------------------------------------------------

namespace xdetail {

// An exact fraction of two integers with no common divisor left in them.
struct bernoulli_of {
    xint numerator;
    xuint denominator;
};

// B_2, B_4, ... B_(2*count) as exact fractions.  The recurrence
//     sum over j from 0 to m of C(m + 1, j) B_j = 0
// is carried out with every value multiplied by one shared denominator, the
// least common multiple of 1 to 2*count + 1.  By von Staudt and Clausius the
// denominator of B_j is a product of primes below j + 2, so it divides that
// multiple, every value of the recurrence stays a whole number, every division
// is exact, and the greatest common divisor is only needed at the end, to reduce
// the published fractions.
inline std::vector<bernoulli_of> bernoulli_table(std::size_t count) {
    const std::size_t n = 2 * count;
    std::vector<bool> composite(n + 2, false);
    xuint shared(1u);
    for (std::size_t p = 2; p <= n + 1; ++p) {
        if (composite[p])
            continue;
        std::size_t power = p; // the highest power of p within the range
        for (std::size_t q = p + p; q <= n + 1; q += p)
            composite[q] = true;
        while (power <= (n + 1) / p)
            power *= p;
        shared *= xuint(power);
    }
    std::vector<xint> scaled(n + 1);
    scaled[0] = xint(shared);                // B_0 = 1
    scaled[1] = -(xint(shared) / xint(2));   // B_1 = -1/2
    for (std::size_t m = 2; m <= n; m += 2) {
        // Every odd index from three on is zero and never computed; the binomial
        // coefficient of the row runs along by its own recurrence.
        xint total(0);
        xuint choose(1u); // C(m + 1, 0)
        for (std::size_t j = 0; j < m; ++j) {
            if (j != 0)
                choose = choose * xuint(m + 2 - j) / xuint(j); // C(m + 1, j)
            if (!scaled[j].is_zero())
                total += xint(choose) * scaled[j];
        }
        scaled[m] = -(total / xint(m + 1u)); // exact, as said above
    }
    std::vector<bernoulli_of> out;
    out.reserve(count);
    for (std::size_t k = 1; k <= count; ++k) {
        const xint& value = scaled[2 * k];
        const xuint whole = alt::detail::magnitude_of(value);
        if (whole.is_zero()) {
            out.push_back({xint(0), xuint(1u)});
            continue;
        }
        const xuint g = alt::gcd(whole, shared);
        const xuint num = whole / g;
        out.push_back({value.is_negative() ? -xint(num) : xint(num), shared / g});
    }
    return out;
}

// The table above, kept once for the whole program and grown by doubling when a
// wider calculation needs more numbers.  A reference into it is only valid until
// the next call that asks for more of them.
inline const std::vector<bernoulli_of>& even_bernoulli(std::size_t count) {
    static std::vector<bernoulli_of> table;
    if (table.size() < count)
        table = bernoulli_table(count > 2 * table.size() ? count : 2 * table.size());
    return table;
}

// ln(2 pi) / 2, the constant of Stirling's series.
template <class T>
const T& ln_sqrt_two_pi() {
    static const T value = []() {
        using w = scratch_of<T>;
        return T(log_core<w>(w(2) * circular_pi<w>()) / w(2));
    }();
    return value;
}

// The series of Stirling for ln Gamma(z),
//     sum over k >= 1 of B_2k / (2k (2k - 1) z^(2k - 1)),
// added term by term with the ratio of two of them, which is a quotient of exact
// integers.  The loop ends where the terms stop shrinking or where one falls
// below the floor of the type, whichever comes first, and `rest` - when asked for
// - reports the first term left out, an upper bound of the error.
template <class T>
T stirling_ln(const T& z, T* rest) {
    const T floor = series_floor<T>();
    const T inv = T(1) / (z * z);
    T term = T(1) / (z * T(12)); // B_2 / (2 * 1 * z) with B_2 = 1 / 6
    T total = term;
    for (std::size_t k = 1;; ++k) {
        // The table holds B_2 first, so B_(2k) is the entry below k and
        // B_(2k+2) the one at k.
        const std::vector<bernoulli_of>& b = even_bernoulli(k + 1);
        // The ratio of two consecutive coefficients is
        //     B_(2k+2) / B_(2k) * (2k)(2k-1) / ((2k+1)(2k+2)),
        // kept as one exact fraction.  The sign of B_(2k) belongs to it too, so
        // that the alternating signs of the Bernoulli numbers survive.
        xint up = b[k].numerator * xint(2 * k) * xint(2 * k - 1) *
                  xint(b[k - 1].denominator);
        if (b[k - 1].numerator.is_negative())
            up = -up;
        const xuint down = b[k].denominator * xuint(2 * k + 1) *
                           xuint(2 * k + 2) *
                           alt::detail::magnitude_of(b[k - 1].numerator);
        const T next = term * (T(up) / T(down)) * inv;
        if (fabs(next) >= fabs(term) || fabs(next) <= floor) {
            if (rest != nullptr)
                *rest = next;
            break;
        }
        total += next;
        term = next;
    }
    return total;
}

// ln Gamma(z) of a z the series above serves.
template <class T>
T lgamma_stirling(const T& z, T* rest) {
    return (z - T(0.5)) * log_core<T>(z) - z + ln_sqrt_two_pi<T>() + stirling_ln(z, rest);
}

// ln Gamma(x) - ln Gamma(x - d) of two arguments the series serves.  A caller
// often wants the difference and not the two values: a binomial coefficient of
// a large top and a small lower index is the difference of two logarithms of
// the size of x ln x and an answer of the size of d ln x, so the subtraction
// itself throws away as many bits as the two differ in magnitude.  With y for
// x - d the identity
//     (x - 1/2) ln x - (y - 1/2) ln y - (x - y)
//         = (x - 1/2) ln(1 + d / y) + d ln y - d
// keeps every term at the size of the answer: the quotient d / y is what is
// small, and the logarithm of one plus it costs nothing, while d ln y is the
// answer itself, found once rather than as a remainder.  The constant and the
// two halves of the series are subtracted as they stand, being small already;
// of the two logarithms only ln y survives at full size, and it is taken once.
template <class T>
T lgamma_shift_core(const T& x, const T& d) {
    const T y = x - d;
    const T head = (x - T(0.5)) * log1p_core<T>(d / y);
    return head + d * log_core<T>(y) - d +
           (stirling_ln<T>(x, nullptr) - stirling_ln<T>(y, nullptr));
}

// Gamma of a whole positive number is a factorial, exact as long as the mantissa
// holds it; nothing to return when the value is not whole, or when the factorial
// would need more bits than the mantissa has.
template <class T>
std::optional<T> whole_factorial(const T& v) {
    if (const T whole = trunc(v); whole != v)
        return std::nullopt;
    const auto m = (v.trunc() - xint(1)).to<std::uint64_t>();
    if (!m.has_value() || *m > T::bits)
        return std::nullopt;
    const xuint facts = alt::factorial(*m);
    if (facts.bit_size() > T::bits)
        return std::nullopt;
    return T(facts);
}

// Gamma of a positive value.  Below a few dozen units the series is worth
// nothing, so the argument is lifted with
//     Gamma(z) = Gamma(z + n) / (z (z + 1) ... (z + n - 1))
// and the value is taken back down by the same product.  How far to lift is
// estimated from the width of the type - the smallest term of the series shrinks
// by about nine bits per unit of z - and the estimate is then checked against the
// term the series really left out.
template <class T>
T gamma_positive_core(const T& x) {
    // A whole number is a factorial, and that factorial is exact whenever the
    // mantissa holds it; the series has nothing to say afterwards, and, worse,
    // would blur even Gamma(1) and Gamma(2), the two places the function is one.
    if (const auto facts = whole_factorial<T>(x))
        return *facts;
    const T floor = series_floor<T>();
    std::ptrdiff_t target = (std::ptrdiff_t(T::bits) + 64) / 9;
    if (target < 16)
        target = 16;
    for (;;) {
        std::size_t steps = 0;
        T product(1);
        T z = x;
        if (x < T(target)) {
            if (const auto want = (T(target) - x).ceil().to<std::ptrdiff_t>())
                steps = std::size_t(*want);
            for (std::size_t i = 0; i < steps; ++i)
                product *= x + T(std::ptrdiff_t(i));
            z = x + T(std::ptrdiff_t(steps));
        }
        T rest(0);
        const T ln = lgamma_stirling(z, &rest);
        if (fabs(rest) <= floor)
            return exp_core(ln) / product;
        target = target * 2 + 8; // lift further and do it over
    }
}

// ln|Gamma(z)| of a positive value, in the logarithm all the way, so that neither
// the answer nor any intermediate value can leave the range of the type.  The
// lift of the function itself,
//     Gamma(z) = Gamma(z + n) / (z (z + 1) ... (z + n - 1)),
// is therefore read in the form
//     ln Gamma(z) = ln Gamma(z + n) - ln(z (z + 1) ... (z + n - 1)).
// Taking the logarithm of the value instead would build a Gamma of the size of
// z ln z and ask the type to hold it, which a wide exponent forgives and a narrow
// one does not: the product alone is what has to fit, and it is of the size of
// the lift, which follows from the width of the mantissa and not from the range
// of the exponent.  The lift and the check of the term the series left out are
// the ones the function above uses.
template <class T>
T lgamma_positive_core(const T& x) {
    // A whole number is a factorial, and the logarithm of that exact value is
    // what the function owes: the series would blur Gamma(1) and Gamma(2), the
    // two places it is exactly one.
    if (const auto facts = whole_factorial<T>(x))
        return log_core<T>(*facts);
    const T floor = series_floor<T>();
    std::ptrdiff_t target = (std::ptrdiff_t(T::bits) + 64) / 9;
    if (target < 16)
        target = 16;
    for (;;) {
        std::size_t steps = 0;
        T product(1);
        T z = x;
        if (x < T(target)) {
            if (const auto want = (T(target) - x).ceil().to<std::ptrdiff_t>())
                steps = std::size_t(*want);
            for (std::size_t i = 0; i < steps; ++i)
                product *= x + T(std::ptrdiff_t(i));
            z = x + T(std::ptrdiff_t(steps));
        }
        T rest(0);
        const T ln = lgamma_stirling(z, &rest);
        if (fabs(rest) <= floor)
            return steps == 0 ? ln : ln - log_core<T>(product);
        target = target * 2 + 8; // lift further and do it over
    }
}

// Gamma of a finite value: the exact factorial of a small whole number, the
// series of a positive one, and Euler's reflection
//     Gamma(x) Gamma(1 - x) = pi / sin(pi x)
// of a negative one.  A pole throws, because the two sides of a pole disagree
// about the sign and this type has no NaN to hide that in.
template <class T>
T gamma_core(const T& v) {
    if (v.is_zero())
        throw std::domain_error("alt::gamma: zero is a pole of the gamma function");
    if (const T whole = trunc(v); whole == v && v.signbit())
        throw std::domain_error("alt::gamma: a whole negative number is a pole");
    if (v.signbit()) {
        const T s = sine_cosine_of(circular_pi<T>() * v).first;
        if (s.is_zero())
            throw std::domain_error("alt::gamma: a pole of the gamma function");
        return circular_pi<T>() / (s * gamma_positive_core(T(1) - v));
    }
    return gamma_positive_core(v);
}

} // namespace xdetail

// The gamma function, which extends the factorial: Gamma(n) = (n - 1)! for a
// whole n of at least one, and that factorial comes back exactly whenever the
// mantissa can hold it.  Zero and the whole negative numbers are the poles and
// throw; a negative infinity has no value either, since the function oscillates
// there without a limit.
template <unsigned Bits>
xreal<Bits> tgamma(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_infinity()) {
        if (v.signbit())
            throw std::domain_error("alt::tgamma of a negative infinity has no value");
        return v;
    }
    return xreal<Bits>(xdetail::gamma_core(w(v)), mode);
}

// The logarithm of its magnitude, which stays finite long after the function
// itself has grown past the type.  The sign of Gamma is what gamma_sign()
// reports, the counterpart of the signgam of the C library.
template <unsigned Bits>
xreal<Bits> lgamma(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (v.is_infinity())
        return xreal<Bits>::infinity(); // the magnitude passes every bound
    if (v.is_zero())
        throw std::domain_error("alt::lgamma: zero is a pole of the gamma function");
    const w x(v);
    if (!x.signbit())
        return xreal<Bits>(xdetail::lgamma_positive_core(x), mode);
    if (const w whole = trunc(x); whole == x)
        throw std::domain_error("alt::lgamma: a whole negative number is a pole");
    // ln|Gamma(x)| = ln pi - ln|sin(pi x)| - ln Gamma(1 - x)
    const w s = fabs(xdetail::sine_cosine_of(xdetail::circular_pi<w>() * x).first);
    const w r = xdetail::log_core(xdetail::circular_pi<w>()) - xdetail::log_core(s) -
                xdetail::lgamma_positive_core(w(1) - x);
    return xreal<Bits>(r, mode);
}

// The sign of Gamma(v), which is negative exactly in the intervals (-2, -1),
// (-4, -3), and so on.  Where the function has a pole there is no sign and this
// throws.
template <any_xreal T>
int gamma_sign(const T& v) {
    if (v.is_zero())
        throw std::domain_error("alt::gamma_sign: zero is a pole of the gamma function");
    if (v.is_infinity()) {
        if (v.signbit())
            throw std::domain_error("alt::gamma_sign of a negative infinity has no value");
        return 1;
    }
    if (!v.signbit())
        return 1;
    if (const T whole = trunc(v); whole == v)
        throw std::domain_error("alt::gamma_sign: a whole negative number is a pole");
    return (-v).floor().is_even() ? -1 : 1; // (-1)^(floor(-v) + 1)
}

// The arithmetic-geometric mean of Gauss: the two sequences built from the
// arithmetic and the geometric mean of the pair converge to the same value, and
// they do so quadratically - the number of correct digits doubles per step.  The
// two elliptic integrals are short work with it.  A negative value has no mean,
// and a zero makes it a zero, as the limit says.
template <unsigned Bits>
xreal<Bits> agm(const xreal<Bits>& a, const xreal<Bits>& b,
                rnd mode = rnd::nearest_even) {
    if (a.is_infinity() || b.is_infinity())
        return xreal<Bits>::infinity();
    if (a.signbit() || b.signbit())
        throw std::domain_error("alt::agm of a negative value has no real value");
    if (a.is_zero() || b.is_zero())
        return xreal<Bits>(0);
    if (a == b)
        return a;
    using w = xdetail::scratch_of<xreal<Bits>>;
    w x(a), y(b);
    w gap = x > y ? x : y; // more than the difference of the first round
    for (;;) {
        // The two means, written so that neither the sum nor the product of two
        // values near the borders of the range can leave it on the way.
        const w sx = w::sqrt(x), sy = w::sqrt(y);
        const w next = x / w(2) + y / w(2);
        y = sx * sy;
        x = next;
        // The difference between the two means falls to about one step of the
        // type and then stays there, so the loop ends at the step itself and at
        // the point where the difference stops shrinking, whichever comes first.
        const w here = fabs(x - y);
        if (here <= x.scaled(-xdetail::exponent_type(w::bits)) || here >= gap)
            break;
        gap = here;
    }
    return xreal<Bits>(x, mode);
}

// The factorial of a value, which is the gamma function shifted by one; a whole
// value within the range of the mantissa is therefore exact.
template <unsigned Bits>
xreal<Bits> factorial(const xreal<Bits>& v, rnd mode = rnd::nearest_even) {
    if (v.is_infinity() && v.signbit())
        throw std::domain_error("alt::factorial of a negative infinity has no value");
    return tgamma(v + xreal<Bits>(1), mode);
}

// The binomial coefficient.  Two whole numbers are computed on the integers,
// which is exact and what a program means when it writes this; everything else
// is a quotient of three gamma functions, always built in the logarithm, since
// each of the three magnitudes can pass the range of the type while the
// quotient of them stays inside it.
//
// Two whole arguments are handled by the three rules that keep Pascal's triangle
// whole in every direction: a negative lower index mirrors onto n - k, a row
// below zero reflects onto the row above the axis with the sign (-1)^k, and
// anything that falls outside the triangle is zero.  A pole of one of the two
// gamma functions below makes the quotient zero, since 1 / Gamma is zero there;
// a pole of the gamma function above is where the function has no value at all,
// and no rule can invent one.
template <unsigned Bits>
xreal<Bits> binomial(const xreal<Bits>& n, const xreal<Bits>& k,
                     rnd mode = rnd::nearest_even) {
    using w = xdetail::scratch_of<xreal<Bits>>;
    if (n.is_infinity() || k.is_infinity())
        throw std::domain_error("alt::binomial of an infinity has no value");
    const w a(n), b(k);
    if (a == w(a.trunc()) && b == w(b.trunc())) {
        const auto nn = a.trunc().to<std::int64_t>();
        const auto kk = b.trunc().to<std::int64_t>();
        if (nn && kk) {
            std::int64_t top = *nn;
            std::int64_t pick = *kk;
            if (pick < 0) {
                if (top - pick < 0)
                    return xreal<Bits>(0); // below the axis on both sides
                pick = top - pick; // the symmetry of the lower index
            }
            if (top >= 0 && pick > top)
                return xreal<Bits>(0); // nothing to choose from fewer
            const std::int64_t steps =
                top < 0 ? pick : std::min(pick, top - pick);
            if (steps <= 65536) {
                if (top >= 0)
                    return xreal<Bits>::from_mantissa(
                        alt::binomial(std::uint64_t(top), std::uint64_t(pick)), 0,
                        false, mode);
                return xreal<Bits>::from_mantissa(
                    alt::binomial(std::uint64_t(pick - top - 1), std::uint64_t(pick)),
                    0, (pick & 1) != 0, mode);
            }
        }
    }
    const auto pole = [](const w& v) {
        return v.is_zero() || (v == w(v.trunc()) && v.signbit());
    };
    if (pole(b + w(1)) || pole(a - b + w(1))) {
        if (!pole(a + w(1)))
            return xreal<Bits>(0); // one over a pole of the gamma function
        throw std::domain_error("alt::binomial: a pole above and a pole below");
    }
    if (pole(a + w(1)))
        throw std::domain_error("alt::binomial: a pole of the gamma function above");
    if (a + w(1) > w(0) && b + w(1) > w(0) && a - b + w(1) > w(0)) {
        // The three logarithms are of the size of a ln a while the answer is of
        // the size of the smaller lower index, which can be forty bits worth
        // away from it.  The smaller index is therefore kept apart and the
        // larger one taken off the top by the shift, which leaves a difference
        // of two values of the size of the answer instead of two of the size of
        // the top; below the reach of the series the shift has nothing to say,
        // and there the plain difference loses only guard bits.
        const w smaller = b < a - b ? b : a - b;
        const w stir = w(1).scaled(40);
        w ln(0);
        if (a + w(1) >= stir && a - smaller + w(1) >= stir)
            ln = xdetail::lgamma_shift_core(a + w(1), smaller) -
                 xdetail::lgamma_positive_core(smaller + w(1));
        else
            ln = xdetail::lgamma_positive_core(a + w(1)) -
                 xdetail::lgamma_positive_core(b + w(1)) -
                 xdetail::lgamma_positive_core(a - b + w(1));
        return xreal<Bits>(xdetail::exp_core(ln), mode);
    }
    // Somewhere below zero, so the sign of the quotient matters and the three
    // magnitudes are no longer bounded by it: each of them alone can overflow
    // the type while the answer is small.  The logarithm of a magnitude is
    // finite everywhere but at a pole, which the checks above ruled out, so the
    // answer is built from the difference of the three logarithms and the
    // product of the three signs.
    const int sign = alt::gamma_sign(a + w(1)) * alt::gamma_sign(b + w(1)) *
                     alt::gamma_sign(a - b + w(1));
    const w ln = alt::lgamma(a + w(1)) - alt::lgamma(b + w(1)) -
                 alt::lgamma(a - b + w(1));
    w value = xdetail::exp_core(ln);
    if (sign < 0)
        value = -value;
    return xreal<Bits>(value, mode);
}

#if defined(AXREAL_ENABLE_FORMAT)

namespace xdetail {

// The part of a format specification this type understands.  The letters mean
// the same as in printf, and so does the precision, except that a precision of
// zero asks for every digit the value holds exactly.  Without a precision the
// general form prints as few digits as are needed to read the value back, and
// the other styles fall back to the six digits of printf.
struct real_specs {
    char fill = ' ';
    char align = '>';
    bool explicit_align = false;
    bool alternate = false;
    bool show_plus = false;
    bool show_space = false;
    bool pad_inside = false;
    std::size_t width = 0;
    unsigned digits = 0;
    bool has_digits = false;
    char type = 'g';

    template <class C>
    static constexpr char narrow(C c) noexcept {
        return static_cast<char>(static_cast<unsigned char>(c));
    }

    static constexpr bool align_of(char c, char& out) noexcept {
        switch (c) {
            case '<': out = '<'; return true;
            case '>': out = '>'; return true;
            case '^': out = '^'; return true;
            default: return false;
        }
    }

    // As in axint.h the specification ends either at '}' or at the first
    // character that is not part of it; the caller decides whether that is the
    // end of the specification or an error.
    template <class It>
    constexpr It parse(It it, It end) {
        if (it == end || narrow(*it) == '}')
            return it;
        It next = it;
        ++next;
        char kind = '>';
        if (next != end && align_of(narrow(*next), kind)) {
            fill = narrow(*it);
            align = kind;
            explicit_align = true;
            it = next;
            ++it;
        } else if (align_of(narrow(*it), kind)) {
            align = kind;
            explicit_align = true;
            ++it;
        }
        char sign = 0;
        bool seen_alternate = false;
        while (it != end) {
            const char c = narrow(*it);
            if (c == '#') {
                if (seen_alternate)
                    return it;
                alternate = true;
                seen_alternate = true;
            } else if (c == '+' || c == '-' || c == ' ') {
                if (sign != 0)
                    return it;
                sign = c;
            } else {
                break;
            }
            ++it;
        }
        show_plus = sign == '+';
        show_space = sign == ' ';
        if (it != end && narrow(*it) == '0' && !explicit_align) {
            pad_inside = true;
            fill = '0';
            align = '>';
            ++it;
        }
        while (it != end) {
            const char c = narrow(*it);
            if (c < '0' || c > '9')
                break;
            width = width * 10u + std::size_t(c - '0');
            ++it;
        }
        if (it != end && narrow(*it) == '.') {
            ++it;
            has_digits = true;
            while (it != end) {
                const char c = narrow(*it);
                if (c < '0' || c > '9')
                    break;
                digits = digits * 10u + unsigned(c - '0');
                ++it;
            }
        }
        if (it != end) {
            switch (narrow(*it)) {
                case 'e': case 'E': case 'f': case 'F':
                case 'g': case 'G': case 'x': case 'X':
                    type = narrow(*it);
                    ++it;
                    break;
                default:
                    break;
            }
        }
        return it;
    }
};

// Widens the ASCII result for a wide output character type.
template <class CharT, class OutIt>
OutIt widen_copy(const std::string& text, OutIt out) {
    for (const char c : text)
        *out++ = static_cast<CharT>(static_cast<unsigned char>(c));
    return out;
}

template <class CharT, class OutIt>
OutIt pad(const real_specs& s, const std::string& prefix, const std::string& body,
          OutIt out) {
    const std::size_t total = prefix.size() + body.size();
    if (total >= s.width) {
        out = widen_copy<CharT>(prefix, out);
        return widen_copy<CharT>(body, out);
    }
    const std::basic_string<CharT> fill(s.width - total, static_cast<CharT>(
                                                           static_cast<unsigned char>(s.fill)));
    if (s.align == '<') {
        out = widen_copy<CharT>(prefix, out);
        out = widen_copy<CharT>(body, out);
        return std::copy(fill.begin(), fill.end(), out);
    }
    if (s.align == '^') {
        const std::size_t left = (s.width - total) / 2;
        out = std::copy_n(fill.begin(), left, out);
        out = widen_copy<CharT>(prefix, out);
        out = widen_copy<CharT>(body, out);
        return std::copy(fill.begin() + std::ptrdiff_t(left), fill.end(), out);
    }
    const std::size_t skip = s.pad_inside ? 0u : fill.size();
    out = std::copy(fill.begin(), fill.begin() + std::ptrdiff_t(skip), out);
    out = widen_copy<CharT>(prefix, out);
    out = std::copy(fill.begin() + std::ptrdiff_t(skip), fill.end(), out);
    return widen_copy<CharT>(body, out);
}

} // namespace xdetail

#endif // AXREAL_ENABLE_FORMAT

} // namespace axreal_v1
} // namespace alt

namespace std {

template <unsigned Bits>
struct hash<alt::xreal<Bits>> {
    std::size_t operator()(const alt::xreal<Bits>& v) const noexcept {
        // Equal values hash equal, and the two signed zeros are equal, so zero
        // gets a value of its own.  Everything else is already in the single
        // normal form the class keeps, which makes the rest unique by itself.
        if (v.is_zero()) {
            // The golden ratio constant, and on a host with a 32 bit size_t the
            // two halves of it mixed into one, so that no constant has to be
            // squeezed into the type by dropping the bits it cannot hold.
            if constexpr (sizeof(std::size_t) >= 8)
                return std::size_t(0x9e3779b97f4a7c15ull);
            else
                return std::size_t(0x9e3779b9u ^ 0x7f4a7c15u);
        }
        std::uint64_t seed = v.is_infinity() ? 0xff51afd7ed558ccdull
                                             : std::uint64_t(v.exponent()) * 0xc4ceb9fe1a85ec53ull;
        if (v.signbit())
            seed ^= 0x85ebca77c2b2ae63ull;
        const alt::xuint& m = v.mantissa();
        return alt::detail::hash_magnitude(m.limb_data(), m.limb_size(), seed);
    }
};

// The limits of the type.  Two of them differ from what a built-in float
// reports: there is no NaN at all, which also keeps the type from being IEC
// 559, and the exponent range is measured in a std::ptrdiff_t, so far wider
// than an int can name - those two are held at the border of the reporting type
// instead of wrapping around.  The rounding style is the default of the class,
// and the explicit operations can pick any of the five.
template <unsigned Bits>
struct numeric_limits<alt::xreal<Bits>> {
    using real = alt::xreal<Bits>;

    // A value of the wide exponent type brought into an int without wrapping.
    static constexpr int narrow_exponent(std::int64_t v) noexcept {
        constexpr std::int64_t high = std::numeric_limits<int>::max();
        constexpr std::int64_t low = std::numeric_limits<int>::lowest();
        if (v > high)
            return int(high);
        if (v < low)
            return int(low);
        return int(v);
    }
    // log10 of 2 as a fraction of a million, exact for every width a program
    // can compile: the error below 1e-6 needs ten billion bits to matter.
    static constexpr std::int64_t whole_decimal_digits(std::uint64_t bits) noexcept {
        return std::int64_t((bits * 301030ull) / 1000000ull);
    }
    static constexpr std::int64_t rounded_decimal_digits(std::uint64_t bits) noexcept {
        return std::int64_t((bits * 301030ull + 999999ull) / 1000000ull);
    }

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    // A NaN is missing on purpose, and with it the IEC 559 conformance.
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    // The IEC 559 badge needs a NaN and a bounded exponent range of its own, and
    // this format deliberately has neither.
    static constexpr bool is_iec559 = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr std::float_round_style round_style = std::round_to_nearest;
    static constexpr int radix = 2;

    static constexpr int digits = int(Bits);
    // The decimal digits that always survive a round trip, and the fewest that
    // distinguish the value from both of its neighbours.
    static constexpr int digits10 = int(whole_decimal_digits(std::uint64_t(Bits) - 1ull));
    static constexpr int max_digits10 = 1 + int(rounded_decimal_digits(std::uint64_t(Bits)));

    // By the convention of the reporting type the smallest normal value of the
    // format is radix^(min_exponent - 1), which here is 2^(-exponent_limit); the
    // decimal twins of the two limits are far beyond an int and get clamped.
    static constexpr int min_exponent =
        narrow_exponent(-std::int64_t(real::exponent_limit) + 1);
    static constexpr int max_exponent = narrow_exponent(std::int64_t(real::exponent_limit) + 1);
    static constexpr int min_exponent10 =
        narrow_exponent(-whole_decimal_digits(std::uint64_t(real::exponent_limit)));
    static constexpr int max_exponent10 =
        narrow_exponent(whole_decimal_digits(std::uint64_t(real::exponent_limit)));

    // The value functions are not constexpr, because not one step of the class
    // is; a program reads them at run time, which is what they are for.
    static real min() noexcept { return real::min_value(); }
    static real max() noexcept { return real::max_value(); }
    static real lowest() noexcept { return real::lowest(); }
    static real epsilon() noexcept { return real::epsilon(); }
    static real round_error() noexcept { return real(0.5); }
    static real infinity() noexcept { return real::infinity(); }
    static real denorm_min() noexcept { return real::min_value(); }
    // Asking a type without NaN for one is a precondition violation; a zero is
    // handed back so that even the illegal call cannot look like a number.
    static real quiet_NaN() noexcept { return real::zero(); }
    static real signaling_NaN() noexcept { return real::zero(); }
};

} // namespace std

#if defined(AXREAL_ENABLE_FORMAT)

#if defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#if !defined(__cpp_lib_format)
#error "AXREAL_ENABLE_FORMAT is set but the standard library has no std::format"
#else

namespace std {

template <unsigned Bits, class CharT>
struct formatter<alt::xreal<Bits>, CharT> {
    alt::axreal_v1::xdetail::real_specs specs;

    constexpr auto parse(basic_format_parse_context<CharT>& ctx) {
        const auto it = specs.parse(ctx.begin(), ctx.end());
        if (it == ctx.end() || *it == CharT('}'))
            return it;
        throw std::format_error("alt::xreal: unsupported format specification");
    }

    template <class FormatContext>
    auto format(const alt::xreal<Bits>& v, FormatContext& ctx) const {
        const char kind = alt::axreal_v1::xdetail::lower_of(specs.type);
        std::string body;
        if (!specs.has_digits && kind == 'g') {
            // The general form without a precision prints as few digits as the
            // value needs to be recognised again, the way std::format treats the
            // types of the language.  A precision of zero on the other hand asks
            // for every digit the mantissa holds.
            body = v.to_string(0u, kind == specs.type ? 'p' : 'P');
            if (specs.alternate && !v.is_infinity()) {
                // The alternative form keeps a point in the text, even when no
                // digit of it means anything.
                if (body.find_first_of(".eEpP") == std::string::npos)
                    body += ".0";
                else if (body.find('.') == std::string::npos)
                    body.insert(body.find_first_of("eEpP"), ".0");
            }
        } else {
            // The other styles fall back to the six digits printf uses, and the
            // binary form is exact whatever the precision says.
            const unsigned digits = specs.has_digits ? specs.digits : 6u;
            body = v.to_string(digits, specs.type);
        }
        std::string prefix;
        if (!body.empty() && (body.front() == '-' || body.front() == '+')) {
            prefix = body.substr(0, 1);
            body.erase(body.begin());
        } else if (specs.show_plus) {
            prefix = "+";
        } else if (specs.show_space) {
            prefix = " ";
        }
        return alt::axreal_v1::xdetail::pad<CharT>(specs, prefix, body, ctx.out());
    }
};

} // namespace std

#endif // __cpp_lib_format
#endif // AXREAL_ENABLE_FORMAT

#endif // AXREAL_H_INCLUDED
