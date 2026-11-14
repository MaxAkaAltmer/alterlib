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
// axint.h - header-only arbitrary precision integer library (C++20).
//
// Namespace alt:
//   alt::xuint - arbitrary precision unsigned integer.
//   alt::xint  - arbitrary precision signed integer (sign + magnitude).
//
// Both types grow on demand (limited only by available memory) and offer the
// usual integral operator set, string conversion in bases 2..36 and accurate
// conversions to/from floating point types. Extra mathematics lives in
// namespace alt (pow, factorial, gcd, pow_mod, roots, primality tests, ...).
//
// Self contained: only the standard library is required. Native 128 bit
// integers or 128 bit intrinsics are used when available, fully portable
// fallbacks are provided otherwise (ARM, RISC-V, 32 bit hosts, ...).  Define
// AXINT_NO_FAST_PATHS to ignore the accelerators and build the portable paths
// even on a host that has them.
//
// Short example:
//
//     #include "axint.h"
//     #include <iostream>
//
//     const alt::xuint m = (alt::xuint(1) << 160) - 1;
//     std::cout << alt::factorial(40) << '\n';
//         // 815915283247897734345611269596115894272000000000
//     std::cout << alt::gcd(alt::factorial(40), alt::factorial(35)) << '\n';
//         // 10333147966386144929666651337523200000000
//     std::cout << alt::pow_mod(alt::xuint(7), 1000, m) << '\n';
//         // 1025210276655012912804608998818376312975254523726
//     std::cout << alt::next_prime(m).to_string(16) << '\n';
//         // 10000000000000000000000000000000000000007
//     std::cout << alt::xint("0xdeadbeefcafebabe") * 3 + 1 << '\n';
//
// Accompanying files, none of which the library itself needs:
//
//     axint_test.cpp    - unit tests, no framework, prints "all tests passed".
//     axint_example.cpp - a small demo program.
//     axint_fuzz.cpp    - an evaluator that reads "op operand..." lines on
//                         stdin and answers them with the library.
//     axint_fuzz.py     - feeds that evaluator thousands of random operands,
//                         recomputes every answer with Python's own integers
//                         and reports the differences, which makes the check
//                         independent of this implementation.  It also covers
//                         the std::formatter support of AXINT_ENABLE_FORMAT.

#ifndef AXINT_H_INCLUDED
#define AXINT_H_INCLUDED

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cctype>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <istream>
#include <limits>
#include <optional>
#include <ostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Feature detection.  Every accelerator here is optional: 128 bit integers and
// compiler intrinsics are used when the target offers them and the portable
// fallbacks below are used otherwise.  Define AXINT_NO_FAST_PATHS before
// including this header to ignore all accelerators, which is how the portable
// paths can be compiled and tested on any host.
#if !defined(AXINT_NO_FAST_PATHS)
#if defined(__SIZEOF_INT128__)
#define ALT_HAVE_U128 1
#endif
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#define ALT_HAVE_MSVC_INTRINSICS 1
#endif
#endif

namespace alt {

inline namespace axint_v1 {

namespace detail {

// ---------------------------------------------------------------------------
// Limb type and wide arithmetic helpers.
// ---------------------------------------------------------------------------

using limb_t = std::uint64_t;
using slimb_t = std::int64_t;

inline constexpr unsigned limb_bits = 64u;
inline constexpr unsigned half_bits = 32u;
inline constexpr limb_t limb_max = ~limb_t(0);
inline constexpr limb_t half_mask = limb_t(0xFFFFFFFFu);
inline constexpr limb_t half_base = limb_t(1) << half_bits;

#if defined(ALT_HAVE_U128)
using u128_t = unsigned __int128;
inline constexpr bool have_u128 = true;
#else
inline constexpr bool have_u128 = false;
#endif

// True when 128/64 -> 64 bit division is available as an intrinsic.
#if defined(ALT_HAVE_MSVC_INTRINSICS) && !defined(ALT_HAVE_U128)
inline constexpr bool have_udiv128 = true;
#else
inline constexpr bool have_udiv128 = false;
#endif

constexpr limb_t low_half(limb_t x) noexcept { return x & half_mask; }
constexpr limb_t high_half(limb_t x) noexcept { return x >> half_bits; }

// High half of a 64x64 bit product. (Not constexpr: compiler intrinsics used
// on some platforms are not usable in constant expressions.)
inline limb_t mulhi(limb_t a, limb_t b) noexcept {
#if defined(ALT_HAVE_U128)
    return limb_t((u128_t(a) * u128_t(b)) >> 64);
#elif defined(ALT_HAVE_MSVC_INTRINSICS)
    return __umulh(a, b);
#else
    const limb_t a0 = low_half(a), a1 = high_half(a);
    const limb_t b0 = low_half(b), b1 = high_half(b);
    const limb_t p0 = a0 * b0;
    const limb_t p1 = a1 * b0 + high_half(p0);
    const limb_t p2 = a1 * b1 + high_half(p1);
    return p2 + high_half(a0 * b1 + low_half(p1));
#endif
}

struct wide64 {
    limb_t lo;
    limb_t hi;
};

// Full 64x64 bit product.
inline wide64 mul_wide(limb_t a, limb_t b) noexcept {
#if defined(ALT_HAVE_U128)
    const u128_t p = u128_t(a) * u128_t(b);
    return wide64{limb_t(p), limb_t(p >> 64)};
#else
    return wide64{a * b, mulhi(a, b)};
#endif
}

constexpr unsigned clz64(limb_t x) noexcept { return unsigned(std::countl_zero(x)); }

constexpr unsigned ctz64(limb_t x) noexcept { return unsigned(std::countr_zero(x)); }

// ((n1 << 32) | n0) / d with a single 32 bit digit d, requires n1 < d.
constexpr limb_t half_div(limb_t n1, limb_t n0, limb_t d) noexcept {
    return ((n1 << half_bits) | low_half(n0)) / d;
}

constexpr limb_t half_mod(limb_t n1, limb_t n0, limb_t d) noexcept {
    return ((n1 << half_bits) | low_half(n0)) % d;
}

// k-th 32 bit digit of the n limb number at u (zero outside the number).
constexpr limb_t half_at(const limb_t* u, size_t n, size_t k) noexcept {
    return k < 2 * n ? ((k & 1) ? high_half(u[k >> 1]) : low_half(u[k >> 1]))
                     : limb_t(0);
}

// ---------------------------------------------------------------------------
// Limb array primitives. Little endian order, normal form means no leading
// zero limbs (the empty array is zero).
// ---------------------------------------------------------------------------

// r[0,n) = a[0,n) + b[0,n), returns the carry out. Regions must not overlap.
constexpr limb_t add_limbs(limb_t* r, const limb_t* a, const limb_t* b, size_t n) noexcept {
    limb_t c = 0;
    for (size_t i = 0; i < n; ++i) {
        const limb_t s = a[i] + c;
        const limb_t t = s + b[i];
        const limb_t c1 = limb_t(t < s);
        const limb_t c2 = limb_t(s < c);
        r[i] = t;
        c = c1 + c2;
    }
    return c;
}

// r[0,n) = a[0,n) + b, returns the carry out. a and r may be the same array.
constexpr limb_t add_limb(limb_t* r, const limb_t* a, size_t n, limb_t b) noexcept {
    limb_t c = b;
    for (size_t i = 0; i < n && c != 0; ++i) {
        const limb_t s = a[i] + c;
        c = limb_t(s < a[i]);
        r[i] = s;
    }
    return c;
}

// r[0,n) = a[0,n) - b[0,n), requires a >= b over n limbs, returns the borrow.
constexpr limb_t sub_limbs(limb_t* r, const limb_t* a, const limb_t* b, size_t n) noexcept {
    limb_t br = 0;
    for (size_t i = 0; i < n; ++i) {
        const limb_t t = a[i] - br;
        const limb_t d = t - b[i];
        const limb_t br1 = limb_t(a[i] < br);
        const limb_t br2 = limb_t(d > t);
        r[i] = d;
        br = br1 + br2;
    }
    return br;
}

// r[0,n) = a[0,n) - 1, returns the borrow out (1 when a was zero).  r and a may
// be the same array, so every source limb is latched before it is overwritten.
constexpr limb_t sub_limb(limb_t* r, const limb_t* a, size_t n) noexcept {
    limb_t br = 1;
    for (size_t i = 0; i < n && br != 0; ++i) {
        const limb_t v = a[i];
        r[i] = v - br;
        br = limb_t(v == 0);
    }
    return br;
}

// r[0,n) = a[0,n) - b[0,m) with n >= m and a >= b, returns the borrow.
// r must either be the very same array as a or not overlap it at all.
inline limb_t sub_wide(limb_t* r, const limb_t* a, size_t n, const limb_t* b, size_t m) {
    assert(n >= m);
    if (r == a) {
        const limb_t br = sub_limbs(r, a, b, m);
        if (br == 0 || n == m)
            return br;
        return sub_limb(r + m, a + m, n - m);
    }
    // Distinct buffers: the limbs above b's range are only copied, then the
    // borrow (if any) walks through them.
    if (n > m)
        std::copy_n(a + m, n - m, r + m);
    const limb_t br = sub_limbs(r, a, b, m);
    return br == 0 ? limb_t(0) : sub_limb(r + m, r + m, n - m);
}

// Number of significant bits, the array is not required to be normalised.
inline size_t mag_bit_size(const limb_t* a, size_t n) noexcept {
    while (n != 0 && a[n - 1] == 0)
        --n;
    return n == 0 ? 0 : (n - 1) * limb_bits + (limb_bits - clz64(a[n - 1]));
}

inline size_t mag_size(const limb_t* a, size_t n) noexcept {
    while (n != 0 && a[n - 1] == 0)
        --n;
    return n;
}

// Three way comparison of magnitudes (leading zero limbs allowed).
constexpr int cmp_wide(const limb_t* a, size_t an, const limb_t* b, size_t bn) noexcept {
    size_t i = an > bn ? an : bn;
    int r = 0;
    for (; i != 0; --i) {
        const limb_t av = i <= an ? a[i - 1] : limb_t(0);
        const limb_t bv = i <= bn ? b[i - 1] : limb_t(0);
        if (av != bv) {
            r = av > bv ? 1 : -1;
            break;
        }
    }
    return r;
}

// r[0,n) = a[0,n) >> sh, the shifted out low bits are lost. sh < limb_bits.
inline void rshift(limb_t* r, const limb_t* a, size_t n, unsigned sh) noexcept {
    assert(sh < limb_bits);
    if (sh == 0) {
        if (r != a)
            std::copy_n(a, n, r);
        return;
    }
    for (size_t i = 0; i + 1 < n; ++i)
        r[i] = (a[i] >> sh) | (a[i + 1] << (limb_bits - sh));
    if (n != 0)
        r[n - 1] = a[n - 1] >> sh;
}

// r[0,n) = a[0,n) << sh, the shifted out high bits are returned. sh < 64.
inline limb_t lshift(const limb_t* a, size_t n, limb_t* r, unsigned sh) noexcept {
    assert(sh < limb_bits);
    if (sh == 0) {
        if (r != a)
            std::copy_n(a, n, r);
        return 0;
    }
    limb_t prev = 0;
    for (size_t i = 0; i < n; ++i) {
        const limb_t next = a[i] >> (limb_bits - sh);
        r[i] = (a[i] << sh) | prev;
        prev = next;
    }
    return prev;
}

// ---------------------------------------------------------------------------
// Multiplication: schoolbook for small sizes, Karatsuba above the threshold.
// ---------------------------------------------------------------------------

inline constexpr size_t karatsuba_threshold = 32;

// r[0,an+bn) = a[0,an) * b[0,bn). r must not overlap a or b.
inline void mul_schoolbook(limb_t* r, const limb_t* a, size_t an, const limb_t* b,
                           size_t bn) {
    std::fill_n(r, an + bn, limb_t(0));
    for (size_t i = 0; i < bn; ++i) {
        limb_t carry = 0;
        for (size_t j = 0; j < an; ++j) {
            const wide64 w = mul_wide(b[i], a[j]);
            const limb_t s1 = r[i + j] + carry;
            const limb_t c1 = limb_t(s1 < carry);
            const limb_t s2 = s1 + w.lo;
            const limb_t c2 = limb_t(s2 < s1);
            r[i + j] = s2;
            carry = w.hi + c1 + c2;
        }
        r[i + an] = carry;
    }
}

inline void mul_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn);

// r[0,an+bn) = a[0,an) * b[0,bn), overlap between the result and the inputs is
// handled by copying them first.
inline void mul_limbs(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn) {
    if (an == 0 || bn == 0) {
        return;
    }
    const bool overlap = (a >= r && a < r + an + bn) || (b >= r && b < r + an + bn) ||
                         (r >= a && r < a + an) || (r >= b && r < b + bn);
    if (overlap) {
        const std::vector<limb_t> aa(a, a + an), bb(b, b + bn);
        mul_rec(r, aa.data(), an, bb.data(), bn);
    } else {
        mul_rec(r, a, an, b, bn);
    }
}

inline void mul_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn) {
    if (an < bn) {
        std::swap(an, bn);
        std::swap(a, b);
    }
    if (bn < karatsuba_threshold) {
        mul_schoolbook(r, a, an, b, bn);
        return;
    }
    // Karatsuba, both halves get the same (even limb count) size.
    size_t n = an;
    if (n & 1)
        ++n;
    std::vector<limb_t> aa(n, limb_t(0)), bb(n, limb_t(0));
    std::copy_n(a, an, aa.data());
    std::copy_n(b, bn, bb.data());
    const size_t h = n / 2;

    std::vector<limb_t> al(2 * (h + 1), limb_t(0));
    std::vector<limb_t> ah(2 * (h + 1), limb_t(0));
    std::vector<limb_t> am(2 * (h + 1), limb_t(0));

    mul_rec(al.data(), aa.data(), h, bb.data(), h);         // a0 * b0
    mul_rec(ah.data(), aa.data() + h, h, bb.data() + h, h); // a1 * b1

    std::vector<limb_t> s1(h + 1, limb_t(0)), s2(h + 1, limb_t(0));
    s1[h] = add_limbs(s1.data(), aa.data() + h, aa.data(), h);
    s2[h] = add_limbs(s2.data(), bb.data() + h, bb.data(), h);
    const size_t s1n = mag_size(s1.data(), h + 1);
    const size_t s2n = mag_size(s2.data(), h + 1);
    if (s1n != 0 && s2n != 0)
        mul_rec(am.data(), s1.data(), s1n, s2.data(), s2n);

    // The middle term is am - al - ah = a0*b1 + a1*b0, which is non negative.
    sub_limbs(am.data(), am.data(), al.data(), 2 * (h + 1));
    sub_limbs(am.data(), am.data(), ah.data(), 2 * (h + 1));

    // r = al + (am << h) + (ah << 2h). All terms are non negative and the sum
    // fits into an + bn limbs, so no transient overflow is possible.
    const size_t rn = an + bn;
    std::fill_n(r, rn, limb_t(0));
    const size_t aln = mag_size(al.data(), 2 * (h + 1));
    for (size_t i = 0; i < aln && i < rn; ++i)
        r[i] = al[i];

    const size_t amn = mag_size(am.data(), 2 * (h + 1));
    limb_t carry = 0;
    for (size_t i = 0; i < amn && h + i < rn; ++i) {
        const limb_t s = r[h + i] + am[i];
        const limb_t t = s + carry;
        r[h + i] = t;
        carry = limb_t(t < s) + limb_t(s < am[i]);
    }
    for (size_t i = h + amn; i < rn && carry != 0; ++i) {
        const limb_t t = r[i] + carry;
        r[i] = t;
        carry = limb_t(t < carry);
    }

    const size_t ahn = mag_size(ah.data(), 2 * (h + 1));
    carry = 0;
    for (size_t i = 0; i < ahn && 2 * h + i < rn; ++i) {
        const limb_t s = r[2 * h + i] + ah[i];
        const limb_t t = s + carry;
        r[2 * h + i] = t;
        carry = limb_t(t < s) + limb_t(s < ah[i]);
    }
    for (size_t i = 2 * h + ahn; i < rn && carry != 0; ++i) {
        const limb_t t = r[i] + carry;
        r[i] = t;
        carry = limb_t(t < carry);
    }
    assert(carry == 0);
}

// ---------------------------------------------------------------------------
// Division.
// ---------------------------------------------------------------------------

// Knuth algorithm D executed with 32 bit digits: only 64/32 bit divisions are
// needed, which every C++ platform has. On entry u[0,un) >= d[0,dn) with
// un >= dn >= 1 and d normalised. Quotient digits (base 2^32, least
// significant first) are written to qd, the remainder to rem[0,dn).
inline void knuth_d32(std::vector<limb_t>& qd, limb_t* rem, const limb_t* u, size_t un,
                      const limb_t* d, size_t dn) {
    const unsigned s = clz64(d[dn - 1]);
    std::vector<limb_t> V(dn), U(un + 1, limb_t(0));
    std::copy_n(d, dn, V.data());
    std::copy_n(u, un, U.data());
    if (s != 0) {
        lshift(V.data(), dn, V.data(), s);
        U[un] = lshift(U.data(), un, U.data(), s);
    }

    const size_t nv = 2 * dn;         // divisor digit count
    size_t nd = 2 * un + 2;           // dividend digit count (upper bound)
    while (nd > 1 && half_at(U.data(), un + 1, nd - 1) == 0)
        --nd;
    assert(nd >= nv);

    std::vector<limb_t> vd(nv);
    for (size_t i = 0; i < nv; ++i)
        vd[i] = half_at(V.data(), dn, i);
    std::vector<limb_t> w(nd + 1);
    for (size_t i = 0; i <= nd; ++i)
        w[i] = half_at(U.data(), un + 1, i);

    const limb_t v1 = vd[nv - 1];
    const limb_t v0 = nv >= 2 ? vd[nv - 2] : limb_t(0);
    const size_t m = nd - nv;
    qd.assign(m + 1, limb_t(0));

    for (size_t j = m + 1; j-- > 0;) {
        // D1/D2: estimate the quotient digit.
        limb_t qh, r1;
        if (w[j + nv] >= v1) {
            qh = half_base - 1;
            r1 = (w[j + nv] << half_bits) + w[j + nv - 1] - qh * v1;
        } else {
            qh = half_div(w[j + nv], w[j + nv - 1], v1);
            r1 = half_mod(w[j + nv], w[j + nv - 1], v1);
        }
        // Refine the estimate with the second most significant divisor digit:
        // the estimate is too large as soon as qh * v0 exceeds the digits of the
        // window that the estimate did not take into account.
        if (r1 < half_base) {
            while (qh >= half_base || qh * v0 > ((r1 << half_bits) | w[j + nv - 2])) {
                qh -= 1;
                r1 += v1;
                if (r1 >= half_base)
                    break;
            }
        }
        // D3: subtract qh * divisor from w[j .. j + nv). Digits stay masked to
        // 32 bits; the amount carried into the next digit is a whole number of
        // base units, which fits easily in a signed limb.
        int64_t borrow = 0;
        for (size_t i = 0; i < nv; ++i) {
            const limb_t p = qh * vd[i];
            const int64_t t = int64_t(w[j + i]) - int64_t(low_half(p)) - borrow;
            w[j + i] = limb_t(t) & half_mask;
            borrow = -(t >> half_bits) + int64_t(high_half(p));
        }
        int64_t top = int64_t(w[j + nv]) - borrow;
        if (top < 0) {
            // D5: the estimate was one too large, add the divisor back.
            qh -= 1;
            limb_t carry = 0;
            for (size_t i = 0; i < nv; ++i) {
                const limb_t t = w[j + i] + vd[i] + carry;
                w[j + i] = t & half_mask;
                carry = t >> half_bits;
            }
            top += int64_t(carry);
        }
        assert(top >= 0 && top < limb_t(1) << (half_bits + 1));
        w[j + nv] = limb_t(top);
        qd[j] = qh;
    }

    // D6: pack the remainder and undo the normalisation shift.
    for (size_t k = 0; k < dn; ++k)
        rem[k] = low_half(w[2 * k]) | (low_half(w[2 * k + 1]) << half_bits);
    if (s != 0)
        rshift(rem, rem, dn, s);
}

// u[0,un) is replaced by the remainder, q (un - dn + 2 limbs, may be null)
// receives the quotient. Requires d normalised and non zero.
inline void divrem_multi(limb_t* q, limb_t* u, size_t un, const limb_t* d, size_t dn) {
    assert(dn >= 1 && un >= dn && d[dn - 1] != 0);
    const size_t un_norm = mag_size(u, un);
    if (cmp_wide(u, un_norm, d, dn) < 0) {
        if (q != nullptr)
            std::fill_n(q, un - dn + 2, limb_t(0));
        std::fill_n(u + un_norm, un - un_norm, limb_t(0));
        return;
    }
    std::vector<limb_t> qd;
    std::vector<limb_t> rem(dn, limb_t(0));
    knuth_d32(qd, rem.data(), u, un_norm, d, dn);
    const size_t qdn = qd.size();
    const size_t qn = (qdn + 1) / 2;
    if (q != nullptr) {
        for (size_t k = 0; k < qn; ++k) {
            const limb_t lo = low_half(qd[2 * k]);
            const limb_t hi = (2 * k + 1 < qdn) ? low_half(qd[2 * k + 1]) : limb_t(0);
            q[k] = lo | (hi << half_bits);
        }
        const size_t qtrim = mag_size(q, qn);
        if (qtrim < qn)
            std::fill_n(q + qtrim, qn - qtrim, limb_t(0));
    }
    std::copy_n(rem.data(), dn, u);
    if (dn < un)
        std::fill_n(u + dn, un - dn, limb_t(0));
}

// q[0,un) = u[0,un) / d, returns the remainder. d != 0, q may be null.
inline limb_t divrem_limb(limb_t* q, const limb_t* u, size_t un, limb_t d) {
    assert(d != 0);
    const size_t n = mag_size(u, un);
    if (n == 0) {
        if (q != nullptr)
            std::fill_n(q, un, limb_t(0));
        return 0;
    }
    if constexpr (have_u128 || have_udiv128) {
        limb_t r = 0;
        for (size_t i = n; i-- > 0;) {
#if defined(ALT_HAVE_U128)
            const u128_t cur = (u128_t(r) << 64) | u[i];
            if (q != nullptr)
                q[i] = limb_t(cur / u128_t(d));
            r = limb_t(cur % u128_t(d));
#elif defined(ALT_HAVE_MSVC_INTRINSICS)
            limb_t rr = 0;
            const limb_t qq = _udiv128(r, u[i], d, &rr);
            if (q != nullptr)
                q[i] = qq;
            r = rr;
#else
            // No accelerator: the condition above is then false, so the general
            // divrem_multi path below is taken and this body is never executed.
#endif
        }
        if (q != nullptr && n < un)
            std::fill_n(q + n, un - n, limb_t(0));
        return r;
    } else {
        std::vector<limb_t> uu(u, u + un);
        std::vector<limb_t> dd(1, d), qq(un + 1, limb_t(0));
        divrem_multi(qq.data(), uu.data(), un, dd.data(), 1);
        if (q != nullptr)
            std::copy_n(qq.data(), un, q);
        return uu[0];
    }
}

// Remainder of u[0,un) divided by a single limb.
inline limb_t mod_limb(const limb_t* u, size_t un, limb_t d) {
    return divrem_limb(nullptr, u, un, d);
}

// r[0,n) = a[0,n) * b, the limb carried out of the top is returned. r may alias
// a.
inline limb_t mul_limb(limb_t* r, const limb_t* a, size_t n, limb_t b) noexcept {
    limb_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        const wide64 w = mul_wide(a[i], b);
        const limb_t s = w.lo + carry;
        r[i] = s;
        carry = w.hi + limb_t(s < carry);
    }
    return carry;
}

// v = v * m + c for a whole number v, growing it by the single limb the result
// can possibly need.
inline void muladd_limb(std::vector<limb_t>& v, limb_t m, limb_t c) {
    const limb_t hi = mul_limb(v.data(), v.data(), v.size(), m);
    const limb_t carry = add_limb(v.data(), v.data(), v.size(), c);
    if (const limb_t top = hi + carry; top != 0)
        v.push_back(top);
}

// ---------------------------------------------------------------------------
// Small vector adapters, the convenient form of the primitives above.
// ---------------------------------------------------------------------------

inline std::vector<limb_t> add_vec(const limb_t* a, size_t an, const limb_t* b,
                                   size_t bn) {
    if (an < bn) {
        std::swap(a, b);
        std::swap(an, bn);
    }
    std::vector<limb_t> r(an + 1, limb_t(0));
    const limb_t carry = add_limbs(r.data(), a, b, bn);
    if (an > bn)
        std::copy_n(a + bn, an - bn, r.data() + bn);
    if (carry != 0)
        add_limb(r.data() + bn, r.data() + bn, an - bn + 1, carry);
    r.resize(mag_size(r.data(), r.size()));
    return r;
}

// a - b, requires a >= b.
inline std::vector<limb_t> sub_vec(const limb_t* a, size_t an, const limb_t* b,
                                   size_t bn) {
    assert(cmp_wide(a, an, b, bn) >= 0);
    std::vector<limb_t> r(an, limb_t(0));
    sub_wide(r.data(), a, an, b, bn);
    r.resize(mag_size(r.data(), an));
    return r;
}

inline std::vector<limb_t> mul_vec(const limb_t* a, size_t an, const limb_t* b,
                                   size_t bn) {
    if (an == 0 || bn == 0)
        return std::vector<limb_t>();
    std::vector<limb_t> r(an + bn, limb_t(0));
    mul_limbs(r.data(), a, an, b, bn);
    r.resize(mag_size(r.data(), r.size()));
    return r;
}

// Quotient of u / d, the remainder (when requested) is written to *rem. Either
// result may be omitted by passing null. d must not be zero.
inline std::vector<limb_t> divrem_vec(const limb_t* u, size_t un, const limb_t* d,
                                      size_t dn, std::vector<limb_t>* rem) {
    un = mag_size(u, un);
    dn = mag_size(d, dn);
    assert(dn != 0);
    if (rem != nullptr)
        rem->clear();
    if (cmp_wide(u, un, d, dn) < 0) {
        if (rem != nullptr)
            rem->assign(u, u + un);
        return std::vector<limb_t>();
    }
    if (dn == 1) {
        std::vector<limb_t> q(un, limb_t(0));
        const limb_t r = divrem_limb(q.data(), u, un, d[0]);
        if (rem != nullptr && r != 0)
            rem->push_back(r);
        q.resize(mag_size(q.data(), un));
        return q;
    }
    // Normalise the divisor so that the top bit is set, then run Knuth D.
    const unsigned sh = clz64(d[dn - 1]);
    std::vector<limb_t> dd(d, d + dn), uu(un + 1, limb_t(0));
    std::copy_n(u, un, uu.data());
    if (sh != 0) {
        uu[un] = lshift(uu.data(), un, uu.data(), sh);
        lshift(dd.data(), dn, dd.data(), sh);
    }
    const size_t un2 = mag_size(uu.data(), un + 1);
    std::vector<limb_t> q(un2 - dn + 2, limb_t(0));
    divrem_multi(q.data(), uu.data(), un2, dd.data(), dn);
    std::vector<limb_t> r(dn, limb_t(0));
    std::copy_n(uu.data(), dn, r.data());
    if (sh != 0)
        rshift(r.data(), r.data(), dn, sh);
    r.resize(mag_size(r.data(), dn));
    q.resize(mag_size(q.data(), q.size()));
    if (rem != nullptr)
        *rem = std::move(r);
    return q;
}

// base^e for a small base, computed by binary exponentiation.
inline std::vector<limb_t> pow_ui_vec(limb_t base, size_t e) {
    std::vector<limb_t> result(1, limb_t(1)), factor(1, base);
    for (; e != 0; e >>= 1) {
        if (e & 1)
            result = mul_vec(result.data(), result.size(), factor.data(), factor.size());
        if (e != 1)
            factor = mul_vec(factor.data(), factor.size(), factor.data(), factor.size());
    }
    return result;
}

// ---------------------------------------------------------------------------
// Radix conversion helpers.
// ---------------------------------------------------------------------------

struct radix_info {
    limb_t big_base; // the largest power of the radix that fits in a limb
    int chars;       // number of digits that power represents
};

constexpr radix_info make_radix_info(unsigned base) noexcept {
    radix_info r{limb_t(1), 0};
    while (r.big_base <= limb_max / base) {
        r.big_base *= base;
        ++r.chars;
    }
    return r;
}

inline const radix_info& radix(unsigned base) noexcept {
    struct table_holder {
        std::array<radix_info, 37> t;
        table_holder() noexcept : t{} {
            for (unsigned b = 2; b <= 36; ++b)
                t[b] = make_radix_info(b);
        }
    };
    static const table_holder holder{};
    assert(base >= 2 && base <= 36);
    return holder.t[base];
}

constexpr int digit_value(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return -1;
}

constexpr char digit_char(int d, bool upper = false) noexcept {
    return d < 10 ? char('0' + d) : char((upper ? 'A' : 'a') + d - 10);
}

// Parses a string of digits in the given radix (no sign, no prefix, at least
// one digit, every character must be a valid digit).
inline void parse_radix(std::vector<limb_t>& v, std::string_view s, unsigned base) {
    const radix_info info = radix(base);
    v.clear();
    size_t i = 0;
    size_t first = s.size() % size_t(info.chars);
    if (first == 0)
        first = size_t(info.chars);
    while (i < s.size()) {
        limb_t chunk = 0;
        const size_t n = i == 0 ? first : size_t(info.chars);
        for (size_t k = 0; k < n; ++k) {
            const int d = digit_value(s[i + k]);
            assert(d >= 0 && unsigned(d) < base);
            chunk = chunk * base + limb_t(d);
        }
        i += n;
        muladd_limb(v, info.big_base, chunk);
    }
    v.resize(mag_size(v.data(), v.size()));
}

// Repeated division by the largest representable power of the radix. The
// result is written least significant digit first.
inline void format_radix_simple(const limb_t* u, size_t un, unsigned base,
                                std::string& out) {
    const radix_info info = radix(base);
    std::vector<limb_t> v(u, u + un);
    std::string digits;
    do {
        limb_t rem = divrem_limb(v.data(), v.data(), v.size(), info.big_base);
        v.resize(mag_size(v.data(), v.size()));
        for (int k = 0; k < info.chars; ++k) {
            digits.push_back(digit_char(int(rem % base)));
            rem /= base;
        }
    } while (!v.empty());
    while (digits.size() > 1 && digits.back() == '0')
        digits.pop_back();
    out.append(digits.rbegin(), digits.rend());
}

// Number of digits of u[0,un) in the given radix, at least 1.
inline size_t digits_in_base(const limb_t* u, size_t un, unsigned base) {
    un = mag_size(u, un);
    if (un == 0)
        return 1;
    const size_t bits = mag_bit_size(u, un);
    const long double per_digit = std::log2(static_cast<long double>(base));
    size_t est = static_cast<size_t>(static_cast<long double>(bits - 1) / per_digit) + 1;
    if (est == 0)
        est = 1;
    std::vector<limb_t> p = pow_ui_vec(base, est);
    while (cmp_wide(u, un, p.data(), p.size()) >= 0) {
        muladd_limb(p, base, limb_t(0));
        ++est;
    }
    while (est > 1) {
        divrem_limb(p.data(), p.data(), p.size(), base);
        p.resize(mag_size(p.data(), p.size()));
        if (cmp_wide(u, un, p.data(), p.size()) >= 0)
            break;
        --est;
    }
    return est;
}

// Writes exactly `digits` digits of v (which must be < radix^digits), zero
// padded, most significant digit first.
inline void format_radix_dc(std::string& out, const std::vector<limb_t>& v,
                            unsigned base, size_t digits) {
    if (digits == 0)
        return;
    if (v.size() <= 8 || digits <= 32) {
        std::string part;
        format_radix_simple(v.data(), v.size(), base, part);
        assert(part.size() <= digits);
        part.insert(part.begin(), part.size() <= digits ? digits - part.size() : 0, '0');
        out += part;
        return;
    }
    const size_t low = digits / 2;
    const std::vector<limb_t> p = pow_ui_vec(base, low);
    std::vector<limb_t> rem;
    const std::vector<limb_t> q = divrem_vec(v.data(), v.size(), p.data(), p.size(), &rem);
    format_radix_dc(out, q, base, digits - low);
    format_radix_dc(out, rem, base, low);
}

inline std::string to_string_radix(const limb_t* u, size_t un, unsigned base) {
    if (mag_size(u, un) == 0)
        return std::string(1, '0');
    std::string out;
    format_radix_dc(out, std::vector<limb_t>(u, u + mag_size(u, un)), base,
                    digits_in_base(u, un, base));
    return out;
}

} // namespace detail
namespace detail {

// Floating point formats the conversions below understand: an IEC 60559 type
// whose mantissa fits into one limb (float, double, x87 long double, ...).
template <class F>
concept binary_float = std::floating_point<F> && std::numeric_limits<F>::is_iec559 &&
                       std::numeric_limits<F>::digits <= int(limb_bits) && sizeof(F) <= 16;

inline unsigned stream_radix(std::ios_base& s) noexcept {
    const std::ios_base::fmtflags f = s.flags() & std::ios_base::basefield;
    if (f == std::ios_base::hex)
        return 16;
    if (f == std::ios_base::oct)
        return 8;
    return 10;
}

// Shared implementation of the two stream output operators: prefix carries the
// sign and the radix mark, digits the number itself.
inline void write_padded(std::ostream& os, const std::string& prefix,
                         const std::string& digits) {
    const std::streamsize total = std::streamsize(prefix.size() + digits.size());
    const std::streamsize w = os.width();
    os.width(0);
    if (w > total) {
        const std::string pad(std::size_t(w - total), os.fill());
        const auto adj = os.flags() & std::ios_base::adjustfield;
        if (adj == std::ios_base::left)
            os << prefix << digits << pad;
        else if (adj == std::ios_base::internal)
            os << prefix << pad << digits;
        else
            os << pad << prefix << digits;
    } else {
        os << prefix << digits;
    }
    if (!os.good())
        os.setstate(std::ios_base::failbit);
}

inline std::string upper_digits(std::string digits) {
    for (char& c : digits)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return digits;
}

} // namespace detail

class xint;

// Generates the mixed big integer / built in integer overloads of a binary
// operator.  The built in operand is converted first, therefore a negative
// value handed to an unsigned class is reported instead of wrapping around.
// The helpers are undefined again once both classes are complete.
#define ALT_AXINT_MIXED_ARITH(cls, op)                                                       \
    template <class T>                                                                       \
        requires std::integral<T>                                                            \
    friend cls operator op(const cls& a, T b) {                                              \
        return a op cls(b);                                                                  \
    }                                                                                        \
    template <class T>                                                                       \
        requires std::integral<T>                                                            \
    friend cls operator op(T a, const cls& b) {                                              \
        return cls(a) op b;                                                                  \
    }

// Same for the relational operators.  A class that declares operator== does not
// get the rewritten form for free, so both have to be offered.
#define ALT_AXINT_MIXED_COMPARE(cls)                                                         \
    template <class T>                                                                       \
        requires std::integral<T>                                                            \
    friend std::strong_ordering operator<=>(const cls& a, T b) {                             \
        return a <=> cls(b);                                                                 \
    }                                                                                        \
    template <class T>                                                                       \
        requires std::integral<T>                                                            \
    friend bool operator==(const cls& a, T b) {                                              \
        return a == cls(b);                                                                  \
    }

// ---------------------------------------------------------------------------
// xuint - arbitrary precision unsigned integer.
//
// The magnitude is a little endian limb vector without leading zero limbs, so
// a default constructed object is zero. Operations that have no unsigned
// result (subtraction below zero, division by zero, taking an unsigned value
// from a negative one) throw std::domain_error rather than wrapping around.
// ---------------------------------------------------------------------------

class xuint {
  public:
    using limb_type = detail::limb_t;
    static constexpr std::size_t bits_per_limb = detail::limb_bits;

    xuint() noexcept = default;
    xuint(const xuint&) = default;
    xuint(xuint&&) noexcept = default;
    xuint& operator=(const xuint&) = default;
    xuint& operator=(xuint&&) noexcept = default;
    ~xuint() = default;

    // Built-in integer conversions are explicit, so the intent stays visible.
    template <class T>
        requires std::unsigned_integral<T>
    explicit xuint(T v) noexcept {
        assign_unsigned(v);
    }

    template <class T>
        requires std::signed_integral<T>
    explicit xuint(T v) {
        if (v < 0)
            throw std::domain_error("alt::xuint constructed from a negative value");
        assign_unsigned(static_cast<std::make_unsigned_t<T>>(v));
    }

    // Floating point values are truncated towards zero; NaN, infinities and
    // negative values are errors.
    explicit xuint(float v) : xuint(from_floating(v)) {}
    explicit xuint(double v) : xuint(from_floating(v)) {}
    explicit xuint(long double v) : xuint(from_floating(v)) {}

    // Digits in the given radix; base 0 derives the radix from a 0x / 0b / 0
    // prefix and defaults to decimal.
    explicit xuint(std::string_view s, unsigned base = 10) { *this = from_string(s, base); }
    explicit xuint(const char* s, unsigned base = 10) : xuint(std::string_view(s), base) {}

    // Narrowing conversion from the signed class, an error when v is negative.
    explicit xuint(const xint& v);

    static xuint from_string(std::string_view s, unsigned base = 10);
    template <detail::binary_float F>
    static xuint from_floating(F v);
    static xuint from_float(float v) { return from_floating(v); }
    static xuint from_double(double v) { return from_floating(v); }
    static xuint from_long_double(long double v) { return from_floating(v); }

    // Raw little endian limb access for interoperability.
    static xuint from_limbs(const limb_type* data, std::size_t count) {
        xuint r;
        r.l_.assign(data, data + detail::mag_size(data, count));
        return r;
    }
    const limb_type* limb_data() const noexcept { return l_.data(); }

    // --- observers
    bool is_zero() const noexcept { return l_.empty(); }
    explicit operator bool() const noexcept { return !l_.empty(); }
    std::size_t bit_size() const noexcept { return detail::mag_bit_size(l_.data(), l_.size()); }
    std::size_t limb_size() const noexcept { return l_.size(); }
    limb_type limb_at(std::size_t i) const noexcept { return i < l_.size() ? l_[i] : limb_type(0); }
    limb_type low_limb() const noexcept { return l_.empty() ? limb_type(0) : l_.front(); }
    bool is_even() const noexcept { return (low_limb() & limb_type(1)) == 0; }
    bool is_odd() const noexcept { return !is_even(); }
    bool test_bit(std::size_t i) const noexcept {
        return ((limb_at(i / bits_per_limb) >> (i % bits_per_limb)) & limb_type(1)) != 0;
    }
    void swap(xuint& o) noexcept { l_.swap(o.l_); }
    void clear() noexcept { l_.clear(); }

    // --- conversions
    // Truncating conversion, like a cast between built-in types.
    template <std::integral T>
        requires(sizeof(T) <= sizeof(limb_type))
    explicit operator T() const noexcept {
        return static_cast<T>(static_cast<std::make_unsigned_t<T>>(low_limb()));
    }

    template <std::integral T>
        requires(sizeof(T) > sizeof(limb_type))
    explicit operator T() const noexcept {
        std::make_unsigned_t<T> r = 0;
        for (std::size_t i = l_.size(); i-- > 0;) {
            r <<= bits_per_limb;
            r |= std::make_unsigned_t<T>(l_[i]);
        }
        return static_cast<T>(r);
    }

    // Checked conversion, empty when the value does not fit into T.
    template <std::integral T>
    [[nodiscard]] std::optional<T> to() const noexcept {
        if (bit_size() > std::size_t(std::numeric_limits<T>::digits))
            return std::nullopt;
        return static_cast<T>(*this);
    }

    template <detail::binary_float F>
    [[nodiscard]] F to_floating() const noexcept;
    [[nodiscard]] float to_float() const noexcept { return to_floating<float>(); }
    [[nodiscard]] double to_double() const noexcept { return to_floating<double>(); }
    [[nodiscard]] long double to_long_double() const noexcept { return to_floating<long double>(); }

    explicit operator float() const noexcept { return to_float(); }
    explicit operator double() const noexcept { return to_double(); }
    explicit operator long double() const noexcept { return to_long_double(); }

    [[nodiscard]] std::string to_string(unsigned base = 10) const {
        if (base < 2 || base > 36) // same radix range as from_string
            throw std::invalid_argument("alt::xuint::to_string: unsupported radix");
        return detail::to_string_radix(l_.data(), l_.size(), base);
    }

    // --- compound assignment
    xuint& operator+=(const xuint& b);
    xuint& operator-=(const xuint& b);
    xuint& operator*=(const xuint& b);
    xuint& operator/=(const xuint& b);
    xuint& operator%=(const xuint& b);
    xuint& operator&=(const xuint& b);
    xuint& operator|=(const xuint& b);
    xuint& operator^=(const xuint& b);
    xuint& operator<<=(std::size_t count);
    xuint& operator>>=(std::size_t count);

    xuint& operator++() { return *this += xuint(1u); }
    xuint& operator--() { return *this -= xuint(1u); }
    xuint operator++(int) {
        xuint t(*this);
        ++*this;
        return t;
    }
    xuint operator--(int) {
        xuint t(*this);
        --*this;
        return t;
    }

    xuint operator+() const noexcept { return *this; }
    // A bitwise complement of an unsigned number of unlimited width is not
    // representable, so ~ is only provided for xint.

    // --- quotient and remainder
    static void divrem(const xuint& a, const xuint& b, xuint& q, xuint& r) {
        check_divisor(b);
        std::vector<detail::limb_t> qq, rr;
        qq = detail::divrem_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), &rr);
        q.l_.swap(qq);
        r.l_.swap(rr);
    }

    // --- the xuint / xuint operator set (hidden friends)
    friend xuint operator+(const xuint& a, const xuint& b) {
        xuint r;
        r.l_ = detail::add_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size());
        return r;
    }

    friend xuint operator-(const xuint& a, const xuint& b) {
        if (detail::cmp_wide(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size()) < 0)
            throw std::domain_error("alt::xuint subtraction would be negative");
        xuint r;
        r.l_ = detail::sub_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size());
        return r;
    }

    friend xuint operator*(const xuint& a, const xuint& b) {
        xuint r;
        r.l_ = detail::mul_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size());
        return r;
    }

    friend xuint operator/(const xuint& a, const xuint& b) { return quotient_of(a, b); }

    friend xuint operator%(const xuint& a, const xuint& b) { return remainder_of(a, b); }

    // Both operands are non negative, so plain limb wise logic is exact.
    friend xuint operator&(const xuint& a, const xuint& b) {
        xuint r;
        const std::size_t n = a.l_.size() < b.l_.size() ? a.l_.size() : b.l_.size();
        r.l_.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            r.l_[i] = a.l_[i] & b.l_[i];
        r.normalize();
        return r;
    }

    friend xuint operator|(const xuint& a, const xuint& b) {
        xuint r;
        const std::size_t n = a.l_.size() > b.l_.size() ? a.l_.size() : b.l_.size();
        r.l_.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            r.l_[i] = a.limb_at(i) | b.limb_at(i);
        return r;
    }

    friend xuint operator^(const xuint& a, const xuint& b) {
        xuint r;
        const std::size_t n = a.l_.size() > b.l_.size() ? a.l_.size() : b.l_.size();
        r.l_.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            r.l_[i] = a.limb_at(i) ^ b.limb_at(i);
        r.normalize();
        return r;
    }

    friend xuint operator<<(const xuint& a, std::size_t count) {
        xuint r(a);
        r <<= count;
        return r;
    }

    friend xuint operator>>(const xuint& a, std::size_t count) {
        xuint r(a);
        r >>= count;
        return r;
    }

    friend std::strong_ordering operator<=>(const xuint& a, const xuint& b) noexcept {
        const int c = detail::cmp_wide(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size());
        if (c < 0)
            return std::strong_ordering::less;
        return c > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
    }

    friend bool operator==(const xuint& a, const xuint& b) noexcept {
        return detail::cmp_wide(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size()) == 0;
    }

    // Mixed arithmetic with built in integers.  Note that xuint combined with
    // xint always yields xint, because the unsigned operand is promoted by the
    // implicit xint(const xuint&) constructor.
    ALT_AXINT_MIXED_ARITH(xuint, +)
    ALT_AXINT_MIXED_ARITH(xuint, -)
    ALT_AXINT_MIXED_ARITH(xuint, *)
    ALT_AXINT_MIXED_ARITH(xuint, /)
    ALT_AXINT_MIXED_ARITH(xuint, %)
    ALT_AXINT_MIXED_ARITH(xuint, &)
    ALT_AXINT_MIXED_ARITH(xuint, |)
    ALT_AXINT_MIXED_ARITH(xuint, ^)
    ALT_AXINT_MIXED_COMPARE(xuint)

  private:
    static void check_divisor(const xuint& b) {
        if (b.is_zero())
            throw std::domain_error("alt::xuint division by zero");
    }

    static xuint quotient_of(const xuint& a, const xuint& b) {
        check_divisor(b);
        xuint q;
        q.l_ = detail::divrem_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), nullptr);
        return q;
    }

    static xuint remainder_of(const xuint& a, const xuint& b) {
        check_divisor(b);
        std::vector<detail::limb_t> rem;
        detail::divrem_vec(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), &rem);
        xuint r;
        r.l_.swap(rem);
        return r;
    }

    void normalize() noexcept { l_.resize(detail::mag_size(l_.data(), l_.size())); }

    template <class U>
        requires std::unsigned_integral<U>
    void assign_unsigned(U v) noexcept {
        if constexpr (sizeof(U) <= sizeof(limb_type)) {
            if (v != 0)
                l_.push_back(static_cast<limb_type>(v));
        } else {
            while (v != 0) {
                l_.push_back(static_cast<limb_type>(v & detail::limb_max));
                v >>= bits_per_limb;
            }
        }
    }

    // The most significant 64 bits together with a sticky flag telling whether
    // anything non zero hides below them.
    struct window {
        limb_type hi;
        bool sticky;
    };
    window top_window() const noexcept {
        const std::size_t bits = bit_size();
        if (bits < bits_per_limb)
            return window{low_limb() << (bits_per_limb - bits), false};
        const std::size_t pos = bits - bits_per_limb;
        const std::size_t idx = pos / bits_per_limb;
        const unsigned off = unsigned(pos % bits_per_limb);
        limb_type hi = off == 0 ? l_[idx]
                                : (l_[idx] >> off) | (l_[idx + 1] << (bits_per_limb - off));
        bool sticky = off != 0 && (l_[idx] & ((limb_type(1) << off) - limb_type(1))) != 0;
        for (std::size_t i = 0; i < idx && !sticky; ++i)
            sticky = l_[i] != 0;
        return window{hi, sticky};
    }

    std::vector<limb_type> l_;
};

// ---------------------------------------------------------------------------
// xuint out of line parts.
// ---------------------------------------------------------------------------

template <detail::binary_float F>
xuint xuint::from_floating(F v) {
    if (std::isnan(v))
        throw std::domain_error("alt::xuint from NaN");
    if (v < 0)
        throw std::domain_error("alt::xuint constructed from a negative value");
    if (std::isinf(v))
        throw std::overflow_error("alt::xuint from infinity");
    xuint r;
    int e = 0;
    F m = std::frexp(v, &e); // v == m * 2^e with 0.5 <= m < 1
    constexpr int digits = std::numeric_limits<F>::digits;
    while (e > 0) {
        const int k = e < digits ? e : digits;
        F whole = F(0);
        const F rest = std::modf(std::ldexp(m, k), &whole);
        r <<= unsigned(k);
        r += xuint(static_cast<std::uint64_t>(whole));
        m = rest;
        e -= k;
    }
    return r;
}

template <detail::binary_float F>
F xuint::to_floating() const noexcept {
    if (is_zero())
        return F(0);
    constexpr int digits = std::numeric_limits<F>::digits;
    std::size_t bits = bit_size();
    const window w = top_window();
    limb_type mant = w.hi;
    // Round the 64 bit window to the mantissa width, ties to even.
    constexpr int extra = int(bits_per_limb) - digits;
    if constexpr (extra > 0) {
        const limb_type unit = limb_type(1) << extra;
        const limb_type rest = mant & (unit - limb_type(1));
        const limb_type half = unit >> 1;
        mant >>= extra;
        if (rest > half || (rest == half && (w.sticky || (mant & limb_type(1)) != 0))) {
            mant += limb_type(1);
            if ((mant >> digits) != 0) { // the rounding carried out of the mantissa
                mant >>= 1;
                ++bits;
            }
        }
    }
    const long long exp2 = static_cast<long long>(bits) - digits;
    if (exp2 > 100000LL)
        return std::numeric_limits<F>::infinity();
    return std::ldexp(static_cast<F>(mant), int(exp2)); // ldexp rounds the overflow away
}

inline xuint xuint::from_string(std::string_view s, unsigned base) {
    bool negative = false;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    if (base == 0) {
        base = 10;
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s.remove_prefix(2);
        } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
            base = 2;
            s.remove_prefix(2);
        } else if (s.size() > 1 && s[0] == '0') {
            base = 8;
            s.remove_prefix(1);
        }
    } else if (s.size() > 2 && s[0] == '0' &&
               ((base == 16 && (s[1] == 'x' || s[1] == 'X')) ||
                (base == 2 && (s[1] == 'b' || s[1] == 'B')))) {
        s.remove_prefix(2);
    }
    if (base < 2 || base > 36)
        throw std::invalid_argument("alt::xuint::from_string: unsupported radix");
    if (s.empty())
        throw std::invalid_argument("alt::xuint::from_string: empty number");
    for (const char c : s) {
        const int d = detail::digit_value(c);
        if (d < 0 || unsigned(d) >= base)
            throw std::invalid_argument("alt::xuint::from_string: invalid digit");
    }
    if (negative)
        throw std::domain_error("alt::xuint::from_string: negative value");
    xuint r;
    detail::parse_radix(r.l_, s, base);
    return r;
}

inline xuint& xuint::operator+=(const xuint& b) {
    if (b.l_.empty())
        return *this;
    if (l_.empty()) {
        l_ = b.l_;
        return *this;
    }
    const std::size_t an = l_.size(), bn = b.l_.size();
    if (an < bn)
        l_.resize(bn, limb_type(0));
    detail::limb_t c = detail::add_limbs(l_.data(), l_.data(), b.l_.data(), bn);
    for (std::size_t i = bn; i < an && c != 0; ++i) {
        const limb_type t = l_[i] + c;
        c = detail::limb_t(t < l_[i]);
        l_[i] = t;
    }
    if (c != 0)
        l_.push_back(c);
    return *this;
}

inline xuint& xuint::operator-=(const xuint& b) {
    if (b.l_.empty())
        return *this;
    if (detail::cmp_wide(l_.data(), l_.size(), b.l_.data(), b.l_.size()) < 0)
        throw std::domain_error("alt::xuint subtraction would be negative");
    detail::sub_wide(l_.data(), l_.data(), l_.size(), b.l_.data(), b.l_.size());
    normalize();
    return *this;
}

inline xuint& xuint::operator*=(const xuint& b) {
    if (b.l_.empty()) {
        l_.clear();
        return *this;
    }
    if (b.l_.size() == 1) {
        const detail::limb_t hi = detail::mul_limb(l_.data(), l_.data(), l_.size(), b.low_limb());
        if (hi != 0)
            l_.push_back(hi);
        normalize();
        return *this;
    }
    l_ = detail::mul_vec(l_.data(), l_.size(), b.l_.data(), b.l_.size());
    return *this;
}

inline xuint& xuint::operator/=(const xuint& b) {
    xuint q = quotient_of(*this, b);
    l_.swap(q.l_);
    return *this;
}

inline xuint& xuint::operator%=(const xuint& b) {
    xuint r = remainder_of(*this, b);
    l_.swap(r.l_);
    return *this;
}

inline xuint& xuint::operator&=(const xuint& b) {
    if (l_.size() > b.l_.size())
        l_.resize(b.l_.size());
    for (std::size_t i = 0; i < l_.size(); ++i)
        l_[i] &= b.l_[i];
    normalize();
    return *this;
}

inline xuint& xuint::operator|=(const xuint& b) {
    if (b.l_.empty())
        return *this;
    const std::size_t n = l_.size() > b.l_.size() ? l_.size() : b.l_.size();
    l_.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        l_[i] |= b.limb_at(i);
    return *this;
}

inline xuint& xuint::operator^=(const xuint& b) {
    if (b.l_.empty())
        return *this;
    const std::size_t n = l_.size() > b.l_.size() ? l_.size() : b.l_.size();
    l_.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        l_[i] ^= b.limb_at(i);
    normalize();
    return *this;
}

inline xuint& xuint::operator<<=(std::size_t count) {
    if (count == 0 || l_.empty())
        return *this;
    if (count > (std::size_t(1) << 32)) // more than four billion bits is a bug
        throw std::length_error("alt::xuint shift count is too large");
    const std::size_t words = count / bits_per_limb;
    const unsigned bits = unsigned(count % bits_per_limb);
    if (words != 0)
        l_.insert(l_.begin(), words, limb_type(0));
    if (bits != 0) {
        limb_type carry = limb_type(0);
        for (std::size_t i = words; i < l_.size(); ++i) {
            const limb_type v = l_[i];
            l_[i] = (v << bits) | carry;
            carry = v >> (bits_per_limb - bits);
        }
        if (carry != 0)
            l_.push_back(carry);
    }
    return *this;
}

inline xuint& xuint::operator>>=(std::size_t count) {
    if (count == 0 || l_.empty())
        return *this;
    const std::size_t words = count / bits_per_limb;
    const unsigned bits = unsigned(count % bits_per_limb);
    if (words >= l_.size()) {
        l_.clear();
        return *this;
    }
    l_.erase(l_.begin(), l_.begin() + std::ptrdiff_t(words));
    if (bits != 0)
        detail::rshift(l_.data(), l_.data(), l_.size(), bits);
    normalize();
    return *this;
}

inline std::ostream& operator<<(std::ostream& os, const xuint& v) {
    const unsigned base = detail::stream_radix(os);
    const bool upper = (os.flags() & std::ios_base::uppercase) != 0;
    std::string digits = v.to_string(base);
    if (upper)
        digits = detail::upper_digits(std::move(digits));
    std::string prefix;
    if ((os.flags() & std::ios_base::showbase) != 0 && !v.is_zero()) {
        if (base == 16)
            prefix = upper ? "0X" : "0x";
        else if (base == 8)
            prefix = "0";
    }
    detail::write_padded(os, prefix, digits);
    return os;
}

inline std::istream& operator>>(std::istream& is, xuint& v) {
    std::string token;
    if (!(is >> token)) {
        is.setstate(std::ios_base::failbit);
        return is;
    }
    try {
        v = xuint::from_string(token, detail::stream_radix(is));
    } catch (const std::exception&) {
        is.setstate(std::ios_base::failbit);
    }
    return is;
}

inline void swap(xuint& a, xuint& b) noexcept {
    a.swap(b);
}

// ---------------------------------------------------------------------------
// detail helpers shared by the signed class.
// ---------------------------------------------------------------------------

namespace detail {

// The two's complement of (-1)^negative * |a| written into t[0,limbs). The
// caller reserves one limb more than the largest magnitude involved, so the
// sign bit of the result is always inside the window.
inline void to_twos_complement(limb_t* t, size_t limbs, const limb_t* a, size_t an,
                               bool negative) noexcept {
    if (!negative) {
        size_t i = 0;
        for (; i < an && i < limbs; ++i)
            t[i] = a[i];
        for (; i < limbs; ++i)
            t[i] = limb_t(0);
        return;
    }
    limb_t c = 1;
    for (size_t i = 0; i < limbs; ++i) {
        const limb_t v = ~((i < an) ? a[i] : limb_t(0));
        const limb_t s = v + c;
        t[i] = s;
        c = limb_t(s < v);
    }
}

// Replaces a two's complement window by its magnitude and reports the sign.
inline bool from_twos_complement(limb_t* t, size_t limbs) noexcept {
    const bool negative = (t[limbs - 1] >> (limb_bits - 1)) != 0;
    if (negative) {
        limb_t c = 1;
        for (size_t i = 0; i < limbs; ++i) {
            const limb_t v = ~t[i];
            const limb_t s = v + c;
            t[i] = s;
            c = limb_t(s < v);
        }
    }
    return negative;
}

enum class bit_op { bit_and, bit_or, bit_xor };

inline limb_t apply_bit_op(limb_t a, limb_t b, bit_op o) noexcept {
    switch (o) {
        case bit_op::bit_and: return a & b;
        case bit_op::bit_or:  return a | b;
        default:              return a ^ b;
    }
}

// Bitwise operation on sign magnitude operands using the two's complement of an
// infinitely wide register.  The magnitude ends up in r, the function reports
// the sign.
inline bool bitwise_signed(std::vector<limb_t>& r, const limb_t* a, size_t an, bool aneg,
                           const limb_t* b, size_t bn, bool bneg, bit_op op) {
    const size_t limbs = (an > bn ? an : bn) + 1;
    std::vector<limb_t> ta(limbs, limb_t(0)), tb(limbs, limb_t(0));
    to_twos_complement(ta.data(), limbs, a, an, aneg);
    to_twos_complement(tb.data(), limbs, b, bn, bneg);
    r.assign(limbs, limb_t(0));
    for (size_t i = 0; i < limbs; ++i)
        r[i] = apply_bit_op(ta[i], tb[i], op);
    const bool negative = from_twos_complement(r.data(), limbs);
    r.resize(mag_size(r.data(), limbs));
    return negative;
}

// True when at least one of the lowest count bits of a[0,an) is set.
inline bool any_low_bits(const limb_t* a, size_t an, size_t count) noexcept {
    const size_t words = count / limb_bits;
    const unsigned bits = unsigned(count % limb_bits);
    for (size_t i = 0; i < an && i < words; ++i)
        if (a[i] != 0)
            return true;
    return bits != 0 && words < an && (a[words] & ((limb_t(1) << bits) - limb_t(1))) != 0;
}

} // namespace detail

// ---------------------------------------------------------------------------
// xint - arbitrary precision signed integer.
//
// Sign magnitude representation: one flag plus an xuint magnitude. Zero always
// carries the positive sign, so a value has exactly one representation.
// Division truncates towards zero and the remainder follows the sign of the
// dividend, exactly like the built in C++ operators; floor_div and floor_mod
// offer the number theoretic flavour. The bitwise operators implement the
// two's complement of an infinitely wide register, hence ~x == -x - 1 and >>
// shifts arithmetically.
// ---------------------------------------------------------------------------

class xint {
  public:
    using limb_type = xuint::limb_type;
    static constexpr std::size_t bits_per_limb = xuint::bits_per_limb;

    xint() noexcept = default;
    xint(const xint&) = default;
    xint(xint&&) noexcept = default;
    xint& operator=(const xint&) = default;
    xint& operator=(xint&&) noexcept = default;
    ~xint() = default;

    // An unsigned big integer is always a valid signed one, so this promotion
    // stays implicit; the opposite direction is explicit and can throw.
    xint(const xuint& v) : m_(v) {}
    xint(xuint&& v) : m_(std::move(v)) {}
    xint& operator=(const xuint& v) noexcept {
        neg_ = false;
        m_ = v;
        return *this;
    }
    xint& operator=(xuint&& v) noexcept {
        neg_ = false;
        m_ = std::move(v);
        return *this;
    }

    template <class T>
        requires std::integral<T>
    explicit xint(T v) {
        assign_int(v);
    }
    template <class T>
        requires std::integral<T>
    xint& operator=(T v) {
        return *this = xint(v);
    }

    // Floating point values are truncated towards zero; NaN is an
    // std::domain_error, an infinity an std::overflow_error.
    explicit xint(float v) : xint(from_floating(v)) {}
    explicit xint(double v) : xint(from_floating(v)) {}
    explicit xint(long double v) : xint(from_floating(v)) {}

    // Digits in the given radix; base 0 derives the radix from a 0x / 0b / 0
    // prefix and defaults to decimal. A leading sign is accepted.
    explicit xint(std::string_view s, unsigned base = 10) { *this = from_string(s, base); }
    explicit xint(const char* s, unsigned base = 10) : xint(std::string_view(s), base) {}

    static xint from_string(std::string_view s, unsigned base = 10);
    template <detail::binary_float F>
    static xint from_floating(F v);
    static xint from_float(float v) { return from_floating(v); }
    static xint from_double(double v) { return from_floating(v); }
    static xint from_long_double(long double v) { return from_floating(v); }

    // Raw little endian limb access for interoperability.
    static xint from_limbs(const limb_type* data, std::size_t count, bool negative = false) {
        xint r;
        r.assign(negative, xuint::from_limbs(data, count));
        return r;
    }
    const limb_type* limb_data() const noexcept { return m_.limb_data(); }

    // --- observers
    bool is_zero() const noexcept { return m_.is_zero(); }
    explicit operator bool() const noexcept { return !m_.is_zero(); }
    int sign() const noexcept { return m_.is_zero() ? 0 : (neg_ ? -1 : 1); }
    bool is_negative() const noexcept { return neg_; }
    bool is_positive() const noexcept { return !m_.is_zero() && !neg_; }
    bool is_even() const noexcept { return (m_.low_limb() & limb_type(1)) == 0; }
    bool is_odd() const noexcept { return !is_even(); }
    const xuint& magnitude() const noexcept { return m_; }
    std::size_t bit_size() const noexcept { return m_.bit_size(); }
    std::size_t limb_size() const noexcept { return m_.limb_size(); }
    limb_type limb_at(std::size_t i) const noexcept { return m_.limb_at(i); }
    limb_type low_limb() const noexcept { return m_.low_limb(); }
    bool test_bit(std::size_t i) const noexcept { return m_.test_bit(i); }
    void negate() noexcept {
        if (!m_.is_zero())
            neg_ = !neg_;
    }
    void swap(xint& o) noexcept {
        m_.swap(o.m_);
        const bool t = neg_;
        neg_ = o.neg_;
        o.neg_ = t;
    }
    void clear() noexcept {
        neg_ = false;
        m_.clear();
    }

    // --- conversions
    // Truncating conversion, like a cast between built in types.
    template <std::integral T>
        requires(sizeof(T) <= sizeof(limb_type))
    explicit operator T() const noexcept {
        using U = std::make_unsigned_t<T>;
        const U raw = static_cast<U>(m_.low_limb());
        return static_cast<T>(neg_ ? static_cast<U>(U(0) - raw) : raw);
    }

    template <std::integral T>
        requires(sizeof(T) > sizeof(limb_type))
    explicit operator T() const noexcept {
        using U = std::make_unsigned_t<T>;
        U u = U(0);
        for (std::size_t i = m_.limb_size(); i-- > 0;) {
            u <<= bits_per_limb;
            u |= static_cast<U>(m_.limb_at(i));
        }
        return static_cast<T>(neg_ ? U(0) - u : u);
    }

    // Checked conversion, empty when the value does not fit into T.
    template <std::integral T>
    [[nodiscard]] std::optional<T> to() const noexcept {
        constexpr std::size_t digits =
            static_cast<std::size_t>(std::numeric_limits<T>::digits);
        if (neg_ && std::is_unsigned_v<T>)
            return std::nullopt;
        const std::size_t bits = m_.bit_size();
        if (bits > digits + (neg_ ? std::size_t(1) : std::size_t(0)))
            return std::nullopt;
        if (neg_ && bits == digits + 1 && !(m_ == (xuint(1u) << digits)))
            return std::nullopt; // only -2^digits of that width is representable
        return static_cast<T>(*this);
    }

    template <detail::binary_float F>
    [[nodiscard]] F to_floating() const noexcept {
        const F v = m_.template to_floating<F>();
        return neg_ ? -v : v;
    }
    [[nodiscard]] float to_float() const noexcept { return to_floating<float>(); }
    [[nodiscard]] double to_double() const noexcept { return to_floating<double>(); }
    [[nodiscard]] long double to_long_double() const noexcept { return to_floating<long double>(); }

    explicit operator float() const noexcept { return to_float(); }
    explicit operator double() const noexcept { return to_double(); }
    explicit operator long double() const noexcept { return to_long_double(); }

    [[nodiscard]] std::string to_string(unsigned base = 10) const {
        std::string s = m_.to_string(base);
        if (neg_)
            s.insert(s.begin(), '-');
        return s;
    }

    // --- compound assignment
    xint& operator+=(const xint& b);
    xint& operator-=(const xint& b);
    xint& operator*=(const xint& b);
    xint& operator/=(const xint& b);
    xint& operator%=(const xint& b);
    xint& operator&=(const xint& b);
    xint& operator|=(const xint& b);
    xint& operator^=(const xint& b);
    xint& operator<<=(std::size_t count);
    xint& operator>>=(std::size_t count);

    template <class T>
        requires std::integral<T>
    xint& operator+=(T b) {
        return *this += xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator-=(T b) {
        return *this -= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator*=(T b) {
        return *this *= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator/=(T b) {
        return *this /= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator%=(T b) {
        return *this %= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator&=(T b) {
        return *this &= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator|=(T b) {
        return *this |= xint(b);
    }
    template <class T>
        requires std::integral<T>
    xint& operator^=(T b) {
        return *this ^= xint(b);
    }

    xint& operator++() { return *this += xint(1); }
    xint& operator--() { return *this -= xint(1); }
    xint operator++(int) {
        xint t(*this);
        ++*this;
        return t;
    }
    xint operator--(int) {
        xint t(*this);
        --*this;
        return t;
    }

    xint operator+() const noexcept { return *this; }
    xint operator-() const {
        xint r(*this);
        r.negate();
        return r;
    }
    // ~x == -x - 1, the two's complement identity of the built in types.
    friend xint operator~(const xint& a) {
        xint r(a);
        ++r;
        r.negate();
        return r;
    }

    // --- quotient and remainder, both truncate towards zero
    static void divrem(const xint& a, const xint& b, xint& q, xint& r);

    // --- the xint / xint operator set (hidden friends)
    friend xint operator+(const xint& a, const xint& b) {
        xint r(a);
        r += b;
        return r;
    }
    friend xint operator-(const xint& a, const xint& b) {
        xint r(a);
        r -= b;
        return r;
    }
    friend xint operator*(const xint& a, const xint& b) {
        xuint m = a.m_ * b.m_;
        return make(a.neg_ != b.neg_, std::move(m));
    }
    friend xint operator/(const xint& a, const xint& b) {
        xuint q, r;
        xuint::divrem(a.m_, b.m_, q, r);
        return make(a.neg_ != b.neg_, std::move(q));
    }
    friend xint operator%(const xint& a, const xint& b) {
        xuint q, r;
        xuint::divrem(a.m_, b.m_, q, r);
        return make(a.neg_, std::move(r));
    }

    friend xint operator&(const xint& a, const xint& b) {
        return bitwise(a, b, detail::bit_op::bit_and);
    }
    friend xint operator|(const xint& a, const xint& b) {
        return bitwise(a, b, detail::bit_op::bit_or);
    }
    friend xint operator^(const xint& a, const xint& b) {
        return bitwise(a, b, detail::bit_op::bit_xor);
    }

    friend xint operator<<(const xint& a, std::size_t count) {
        xint r(a);
        r <<= count;
        return r;
    }
    friend xint operator>>(const xint& a, std::size_t count) {
        xint r(a);
        r >>= count;
        return r;
    }

    friend std::strong_ordering operator<=>(const xint& a, const xint& b) noexcept {
        if (a.neg_ != b.neg_)
            return a.neg_ ? std::strong_ordering::less : std::strong_ordering::greater;
        const int c = detail::cmp_wide(a.m_.limb_data(), a.m_.limb_size(), b.m_.limb_data(),
                                     b.m_.limb_size());
        if (c == 0)
            return std::strong_ordering::equal;
        const bool greater = c > 0 ? !a.neg_ : a.neg_;
        return greater ? std::strong_ordering::greater : std::strong_ordering::less;
    }

    friend bool operator==(const xint& a, const xint& b) noexcept {
        return a.neg_ == b.neg_ && a.m_ == b.m_;
    }

    friend std::ostream& operator<<(std::ostream& os, const xint& v);
    friend std::istream& operator>>(std::istream& is, xint& v);

    // Mixed arithmetic with built in integers.  xint combined with xuint
    // results in xint as well, because the unsigned value is promoted.
    ALT_AXINT_MIXED_ARITH(xint, +)
    ALT_AXINT_MIXED_ARITH(xint, -)
    ALT_AXINT_MIXED_ARITH(xint, *)
    ALT_AXINT_MIXED_ARITH(xint, /)
    ALT_AXINT_MIXED_ARITH(xint, %)
    ALT_AXINT_MIXED_ARITH(xint, &)
    ALT_AXINT_MIXED_ARITH(xint, |)
    ALT_AXINT_MIXED_ARITH(xint, ^)
    ALT_AXINT_MIXED_COMPARE(xint)

    friend void swap(xint& a, xint& b) noexcept { a.swap(b); }

  private:
    static xint make(bool negative, xuint m) {
        xint r;
        r.assign(negative, std::move(m));
        return r;
    }

    static xint bitwise(const xint& a, const xint& b, detail::bit_op op) {
        std::vector<detail::limb_t> r;
        const bool negative =
            detail::bitwise_signed(r, a.m_.limb_data(), a.m_.limb_size(), a.neg_,
                                   b.m_.limb_data(), b.m_.limb_size(), b.neg_, op);
        return make(negative, xuint::from_limbs(r.data(), r.size()));
    }

    void assign(bool negative, xuint m) noexcept {
        if (m.is_zero()) {
            neg_ = false;
            m_.clear();
            return;
        }
        neg_ = negative;
        m_ = std::move(m);
    }

    // Signed addition of a magnitude, the heart of += and -=.
    xint& add_signed(bool bneg, const xuint& bm);

    template <class T>
        requires std::integral<T>
    void assign_int(T v) {
        clear();
        if (v == T(0))
            return;
        if constexpr (std::is_signed_v<T>) {
            using U = std::make_unsigned_t<T>;
            if (v < T(0)) {
                // Negate in the unsigned domain, so the lowest value stays
                // representable.
                assign(true, xuint(static_cast<U>(U(0) - static_cast<U>(v))));
                return;
            }
            assign(false, xuint(static_cast<U>(v)));
        } else {
            assign(false, xuint(v));
        }
    }

    xuint m_;
    bool neg_ = false;
};

// ---------------------------------------------------------------------------
// xint out of line parts, plus the narrowing xuint conversion.
// ---------------------------------------------------------------------------

inline xuint::xuint(const xint& v) {
    if (v.is_negative())
        throw std::domain_error("alt::xuint constructed from a negative value");
    *this = v.magnitude();
}

inline xint& xint::add_signed(bool bneg, const xuint& bm) {
    if (bm.is_zero())
        return *this;
    if (m_.is_zero()) {
        assign(bneg, bm);
        return *this;
    }
    if (neg_ == bneg) {
        m_ += bm; // equal signs, only the magnitude grows
        return *this;
    }
    const int c =
        detail::cmp_wide(m_.limb_data(), m_.limb_size(), bm.limb_data(), bm.limb_size());
    if (c == 0)
        clear();
    else if (c < 0) {
        m_ = bm - m_;
        neg_ = bneg;
    } else {
        m_ -= bm;
    }
    return *this;
}

inline xint& xint::operator+=(const xint& b) { return add_signed(b.neg_, b.m_); }

inline xint& xint::operator-=(const xint& b) { return add_signed(!b.neg_, b.m_); }

inline xint& xint::operator*=(const xint& b) {
    neg_ = neg_ != b.neg_;
    m_ *= b.m_;
    if (m_.is_zero())
        neg_ = false;
    return *this;
}

inline xint& xint::operator/=(const xint& b) { return *this = *this / b; }

inline xint& xint::operator%=(const xint& b) { return *this = *this % b; }

inline xint& xint::operator&=(const xint& b) { return *this = *this & b; }

inline xint& xint::operator|=(const xint& b) { return *this = *this | b; }

inline xint& xint::operator^=(const xint& b) { return *this = *this ^ b; }

inline xint& xint::operator<<=(std::size_t count) {
    m_ <<= count;
    if (m_.is_zero())
        neg_ = false;
    return *this;
}

inline xint& xint::operator>>=(std::size_t count) {
    if (count == 0 || m_.is_zero())
        return *this;
    // An arithmetic shift rounds towards minus infinity, therefore a negative
    // value is rounded away from zero before the magnitude is shifted.
    const bool round_up =
        neg_ && detail::any_low_bits(m_.limb_data(), m_.limb_size(), count);
    m_ >>= count;
    if (round_up)
        m_ += xuint(1u);
    if (m_.is_zero())
        neg_ = false;
    return *this;
}

inline void xint::divrem(const xint& a, const xint& b, xint& q, xint& r) {
    xuint mq, mr;
    xuint::divrem(a.m_, b.m_, mq, mr);
    q.assign(a.neg_ != b.neg_, std::move(mq));
    r.assign(a.neg_, std::move(mr));
}

template <detail::binary_float F>
xint xint::from_floating(F v) {
    if (std::isnan(v))
        throw std::domain_error("alt::xint from NaN");
    if (v == F(0))
        return xint();
    const bool negative = v < F(0);
    return make(negative, xuint::from_floating(negative ? -v : v));
}

inline xint xint::from_string(std::string_view s, unsigned base) {
    bool negative = false;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    return make(negative, xuint::from_string(s, base));
}

inline std::ostream& operator<<(std::ostream& os, const xint& v) {
    const unsigned base = detail::stream_radix(os);
    const bool upper = (os.flags() & std::ios_base::uppercase) != 0;
    std::string digits = v.magnitude().to_string(base);
    if (upper)
        digits = detail::upper_digits(std::move(digits));
    std::string prefix;
    if (!v.is_zero()) {
        if (v.is_negative())
            prefix = "-";
        else if ((os.flags() & std::ios_base::showpos) != 0)
            prefix = "+";
        if ((os.flags() & std::ios_base::showbase) != 0) {
            if (base == 16)
                prefix += upper ? "0X" : "0x";
            else if (base == 8)
                prefix += "0";
        }
    }
    detail::write_padded(os, prefix, digits);
    return os;
}

inline std::istream& operator>>(std::istream& is, xint& v) {
    std::string token;
    if (!(is >> token)) {
        is.setstate(std::ios_base::failbit);
        return is;
    }
    try {
        v = xint::from_string(token, detail::stream_radix(is));
    } catch (const std::exception&) {
        is.setstate(std::ios_base::failbit);
    }
    return is;
}

#undef ALT_AXINT_MIXED_ARITH
#undef ALT_AXINT_MIXED_COMPARE

// ---------------------------------------------------------------------------
// Mathematics: divisibility, powers, combinatorics, roots, primality.
//
// Everything below is built on the public interface of the two classes, so it
// stays easy to follow and to extend.  Where a function accepts any of the
// three number flavours (built in integer, xuint, xint) the constraint is the
// integral_like concept and the conversion happens through a direct
// initialisation, which keeps the explicit constructors honest.
// ---------------------------------------------------------------------------

namespace detail {

template <class T>
concept big_int = std::is_same_v<std::remove_cvref_t<T>, xuint> ||
                  std::is_same_v<std::remove_cvref_t<T>, xint>;

// Everything the free functions below are willing to take as a number.
template <class T>
concept integral_like = std::integral<T> || big_int<T>;

// Absolute value as an unsigned big integer.
inline xuint magnitude_of(const xuint& v) noexcept { return v; }
inline xuint magnitude_of(const xint& v) noexcept { return v.magnitude(); }

template <std::integral T>
inline xuint magnitude_of(T v) {
    if constexpr (std::is_signed_v<T>) {
        using U = std::make_unsigned_t<T>;
        // The negation happens in the unsigned domain, so the lowest value of
        // the type stays representable.
        return v < T(0) ? xuint(static_cast<U>(U(0) - static_cast<U>(v)))
                        : xuint(static_cast<U>(v));
    } else {
        return xuint(v);
    }
}

// Primes below 200, used for the trial division in front of Miller Rabin.
inline constexpr std::array<std::uint64_t, 46> small_primes{
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,
    59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199};

inline std::size_t trailing_zeros_of(const xuint& v) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < v.limb_size(); ++i) {
        const limb_t w = v.limb_at(i);
        if (w != 0)
            return n + ctz64(w);
        n += limb_bits;
    }
    return std::numeric_limits<std::size_t>::max(); // no bit set at all
}

inline std::uint64_t popcount_of(const xuint& v) noexcept {
    std::uint64_t c = 0;
    for (std::size_t i = 0; i < v.limb_size(); ++i)
        c += static_cast<std::uint64_t>(std::popcount(v.limb_at(i)));
    return c;
}

// Splitmix style mixer, used to derive Miller Rabin bases from the value.
inline std::uint64_t mix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

inline void check_radix(unsigned base) {
    if (base < 2 || base > 36)
        throw std::invalid_argument("alt: unsupported radix");
}

} // namespace detail

// ---------------------------------------------------------------------------
// Small helpers.
// ---------------------------------------------------------------------------

inline xint abs(const xint& v) noexcept { return v.is_negative() ? -v : v; }
inline xuint abs(const xuint& v) noexcept { return v; }
inline int sign(const xint& v) noexcept { return v.sign(); }
inline int sign(const xuint& v) noexcept { return v.is_zero() ? 0 : 1; }

// Number of set bits of the magnitude.
inline std::uint64_t popcount(const xuint& v) noexcept { return detail::popcount_of(v); }
inline std::uint64_t popcount(const xint& v) noexcept { return detail::popcount_of(v.magnitude()); }

// Index of the lowest set bit of the magnitude.  A value without any bit
// reports the largest representable index, the "not found" marker.
inline std::size_t trailing_zeros(const xuint& v) noexcept { return detail::trailing_zeros_of(v); }
inline std::size_t trailing_zeros(const xint& v) noexcept {
    return detail::trailing_zeros_of(v.magnitude());
}

// Number of digits needed in the given radix, at least one.
inline std::size_t sizeinbase(const xuint& v, unsigned base) {
    detail::check_radix(base);
    return detail::digits_in_base(v.limb_data(), v.limb_size(), base);
}
inline std::size_t sizeinbase(const xint& v, unsigned base) {
    return sizeinbase(v.magnitude(), base);
}

// Position of the highest set bit, an error for zero.
inline std::size_t ilog2(const xuint& v) {
    if (v.is_zero())
        throw std::domain_error("alt::ilog2 of zero");
    return v.bit_size() - 1;
}
inline std::size_t ilog2(const xint& v) { return ilog2(v.magnitude()); }

// ---------------------------------------------------------------------------
// Division flavours.  The operators of xint truncate towards zero like the
// built in types; these two round towards minus infinity and plus infinity.
// ---------------------------------------------------------------------------

template <detail::integral_like A, detail::integral_like B>
inline xint floor_div(const A& a, const B& b) {
    const xint x(a), y(b);
    xint q, r;
    xint::divrem(x, y, q, r);
    if (!r.is_zero() && (r.is_negative() != y.is_negative()))
        --q;
    return q;
}

template <detail::integral_like A, detail::integral_like B>
inline xint floor_mod(const A& a, const B& b) {
    const xint x(a), y(b);
    xint q, r;
    xint::divrem(x, y, q, r);
    if (!r.is_zero() && (r.is_negative() != y.is_negative()))
        r += y;
    return r;
}

template <detail::integral_like A, detail::integral_like B>
inline xint ceil_div(const A& a, const B& b) {
    const xint x(a), y(b);
    xint q, r;
    xint::divrem(x, y, q, r);
    if (!r.is_zero() && (r.is_negative() == y.is_negative()))
        ++q;
    return q;
}

template <detail::integral_like A, detail::integral_like B>
inline xint ceil_mod(const A& a, const B& b) {
    const xint x(a), y(b);
    xint q, r;
    xint::divrem(x, y, q, r);
    if (!r.is_zero() && (r.is_negative() == y.is_negative()))
        r -= y;
    return r;
}

// ---------------------------------------------------------------------------
// Powers.
// ---------------------------------------------------------------------------

// Binary ladder, the squaring of the last round is skipped.
inline xuint pow(const xuint& base, unsigned long long exponent) {
    if (exponent == 0)
        return xuint(1u);
    if (base.is_zero())
        return xuint();
    xuint result(1u), b(base);
    while (exponent != 0) {
        if ((exponent & 1ULL) != 0)
            result *= b;
        exponent >>= 1;
        if (exponent != 0)
            b *= b;
    }
    return result;
}

inline xint pow(const xint& base, unsigned long long exponent) {
    const xuint m = pow(base.magnitude(), exponent);
    if (base.is_negative() && (exponent & 1ULL) != 0)
        return -xint(m);
    return xint(m);
}

// Lets built in integers be used as the base, the result is always signed.
template <detail::integral_like B>
inline xint pow(const B& base, unsigned long long exponent) {
    const xint b(base);
    const xuint m = pow(b.magnitude(), exponent);
    if (b.is_negative() && (exponent & 1ULL) != 0)
        return -xint(m);
    return xint(m);
}

// 10^exponent, written as 5^exponent << exponent because that is cheaper.
inline xuint pow10(unsigned long long exponent) {
    if (exponent > (1ULL << 20))
        throw std::length_error("alt::pow10: exponent is too large");
    return pow(xuint(5u), exponent) << static_cast<std::size_t>(exponent);
}

// base^exponent mod modulus, the result lies in [0, modulus).
template <detail::integral_like B, detail::integral_like E, detail::integral_like M>
inline xuint pow_mod(const B& base, const E& exponent, const M& modulus) {
    const xuint m(modulus); // throws for a negative modulus
    if (m.is_zero())
        throw std::domain_error("alt::pow_mod: zero modulus");
    const xint e_check(exponent);
    if (e_check.is_negative())
        throw std::domain_error("alt::pow_mod: negative exponent");
    if (m == xuint(1u))
        return xuint();

    const xuint e = detail::magnitude_of(exponent);
    const xint b(base);
    xuint result(1u), acc = b.magnitude() % m;
    const std::size_t bits = e.bit_size();
    for (std::size_t i = 0; i < bits; ++i) {
        if (e.test_bit(i))
            result = (result * acc) % m;
        acc = (acc * acc) % m;
    }
    // A negative base with an odd exponent mirrors the residue.
    if (b.is_negative() && bits != 0 && e.test_bit(0) && !result.is_zero())
        result = m - result;
    return result;
}

// ---------------------------------------------------------------------------
// Greatest common divisor and friends.
// ---------------------------------------------------------------------------

template <detail::integral_like A, detail::integral_like B>
inline xuint gcd(A a, B b) {
    xuint u = detail::magnitude_of(a), v = detail::magnitude_of(b);
    while (!v.is_zero()) {
        xuint q, r;
        xuint::divrem(u, v, q, r);
        u = std::move(v);
        v = std::move(r);
    }
    return u;
}

template <detail::integral_like A, detail::integral_like B>
inline xuint lcm(const A& a, const B& b) {
    xuint u = detail::magnitude_of(a);
    const xuint v = detail::magnitude_of(b);
    if (u.is_zero() || v.is_zero())
        return xuint();
    const xuint g = gcd(u, v);
    u /= g;
    return u * v;
}

// The inverse of a modulo m, in [0, m).  Throws when gcd(a, m) is not one.
template <detail::integral_like A, detail::integral_like M>
inline xint inv_mod(const A& a, const M& modulus) {
    const xuint m(modulus);
    if (m.is_zero())
        throw std::domain_error("alt::inv_mod: zero modulus");
    if (m == xuint(1u))
        return xint();
    const xint mod(m);
    xint r(mod), new_r(xint(a) % mod); // the remainder follows the sign of a,
    if (new_r.is_negative())           // so it has to be folded into [0, m)
        new_r += mod;
    xint t, new_t(1);
    while (!new_r.is_zero()) {
        const xint q = r / new_r;
        xint s = t - q * new_t;
        t = new_t;
        new_t = std::move(s);
        xint u = r - q * new_r;
        r = new_r;
        new_r = std::move(u);
    }
    if (r != xint(1))
        throw std::domain_error("alt::inv_mod: value is not invertible");
    xint res = t % mod; // the coefficient can leave [0, m) in both directions
    if (res.is_negative())
        res += mod;
    return res;
}

// ---------------------------------------------------------------------------
// Combinatorics.
// ---------------------------------------------------------------------------

namespace detail {

// Product of the half open range [lo, hi) as a balanced tree, which keeps the
// operand sizes of the multiplications close to each other.
inline xuint product_tree(std::uint64_t lo, std::uint64_t hi) {
    if (lo >= hi)
        return xuint(1u);
    if (hi - lo <= 16) {
        xuint p(1u);
        for (std::uint64_t i = lo; i < hi; ++i)
            p *= xuint(i);
        return p;
    }
    const std::uint64_t mid = lo + (hi - lo) / 2;
    return product_tree(lo, mid) * product_tree(mid, hi);
}

// F(n) and F(n+1) by fast doubling.
inline void fib_pair(std::uint64_t n, xuint& f, xuint& g) {
    if (n == 0) {
        f = xuint();
        g = xuint(1u);
        return;
    }
    fib_pair(n >> 1, f, g);
    xuint a = f * ((g << 1) - f); // F(2k)   = F(k) * (2*F(k+1) - F(k))
    xuint b = f * f + g * g;      // F(2k+1) = F(k)^2 + F(k+1)^2
    if ((n & 1ULL) == 0) {
        f = std::move(a);
        g = std::move(b);
    } else {
        xuint c = a + b; // F(2k) + F(2k+1), computed before b is moved away
        f = std::move(b);
        g = std::move(c);
    }
}

} // namespace detail

inline xuint factorial(std::uint64_t n) { return detail::product_tree(2, n + 1); }

// n * (n-1) * ... * (n - k + 1) / k!, exact at every step.
inline xuint falling_factorial(std::uint64_t n, std::uint64_t k) {
    if (k > n)
        return xuint();
    xuint p(1u);
    for (std::uint64_t i = 0; i < k; ++i)
        p *= xuint(n - i);
    return p;
}

inline xuint binomial(std::uint64_t n, std::uint64_t k) {
    if (k > n)
        return xuint();
    if (k > n - k)
        k = n - k;
    xuint r(1u);
    for (std::uint64_t i = 1; i <= k; ++i) {
        r *= xuint(n - k + i);
        r /= xuint(i); // the intermediate value is C(n-k+i, i), an integer
    }
    return r;
}

inline xuint fibonacci(std::uint64_t n) {
    xuint f, g;
    detail::fib_pair(n, f, g);
    return f;
}

// ---------------------------------------------------------------------------
// Integer roots.
// ---------------------------------------------------------------------------

// Floor square root together with the remainder, so that v == r*r + rem.
inline std::pair<xuint, xuint> sqrtrem(const xuint& v) {
    if (v.is_zero())
        return std::make_pair(xuint(), xuint());
    // Newton's method starts from an overestimate and decreases monotonically.
    xuint x = xuint(1u) << ((v.bit_size() + 1) / 2);
    for (;;) {
        xuint q, r;
        xuint::divrem(v, x, q, r);
        xuint y = (x + q) >> 1;
        if (y >= x)
            break;
        x = std::move(y);
    }
    const xuint one(1u);
    while (x * x > v)
        x -= one;
    while ((x + one) * (x + one) <= v)
        x += one;
    return std::make_pair(x, v - x * x);
}

inline xuint sqrt(const xuint& v) { return sqrtrem(v).first; }
inline xuint sqrt(const xint& v) {
    if (v.is_negative())
        throw std::domain_error("alt::sqrt of a negative value");
    return sqrt(v.magnitude());
}

// Floor n-th root for a positive degree.
inline xuint iroot(const xuint& v, unsigned n) {
    if (n == 0)
        throw std::domain_error("alt::iroot: degree must be positive");
    if (n == 1 || v.is_zero() || v == xuint(1u))
        return v;
    if (n == 2)
        return sqrt(v);
    xuint x = xuint(1u) << ((v.bit_size() + n - 1) / n);
    const xuint deg(n);
    for (;;) {
        xuint q, r;
        xuint::divrem(v, pow(x, n - 1), q, r);
        xuint y = (x * xuint(n - 1u) + q) / deg;
        if (y >= x)
            break;
        x = std::move(y);
    }
    const xuint one(1u);
    while (!x.is_zero() && pow(x, n) > v)
        x -= one;
    while (pow(x + one, n) <= v)
        x += one;
    return x;
}

// Signed values need an odd degree. The result is truncated towards zero, so
// iroot(-30, 3) == -3 because the magnitude is floored, exactly like the
// unsigned overload floors a positive value.
inline xint iroot(const xint& v, unsigned n) {
    if (v.is_negative()) {
        if ((n & 1u) == 0)
            throw std::domain_error("alt::iroot: even degree of a negative value");
        return -xint(iroot(v.magnitude(), n));
    }
    return iroot(v.magnitude(), n);
}

inline xuint cbrt(const xuint& v) { return iroot(v, 3); }
inline xint cbrt(const xint& v) { return iroot(v, 3); }

// ---------------------------------------------------------------------------
// Primality.
// ---------------------------------------------------------------------------

namespace detail {

// One Miller Rabin round: true when a proves n composite.
inline bool is_composite_witness(const xuint& n, const xuint& a, const xuint& d,
                                 unsigned s) {
    xuint x = pow_mod(a, d, n);
    const xuint nm1 = n - xuint(1u);
    if (x == xuint(1u) || x == nm1)
        return false;
    for (unsigned i = 1; i < s; ++i) {
        x = (x * x) % n;
        if (x == nm1)
            return false;
        if (x == xuint(1u))
            return true; // a non trivial square root of one
    }
    return true;
}

} // namespace detail

// Miller Rabin.  The small prime bases make the test deterministic below
// 3.3 * 10^24; the additional bases are derived from the value itself, which
// keeps the chance of calling a composite prime at roughly 4^-rounds.
inline bool is_probable_prime(const xuint& n, unsigned rounds = 24) {
    if (n < xuint(2u))
        return false;
    for (const std::uint64_t p : detail::small_primes) {
        const xuint pp(p);
        if (n == pp)
            return true;
        if ((n % pp).is_zero())
            return false;
    }
    const xuint nm1 = n - xuint(1u);
    const unsigned s = static_cast<unsigned>(detail::trailing_zeros_of(nm1));
    const xuint d = nm1 >> s;
    const unsigned total = rounds < detail::small_primes.size()
                               ? static_cast<unsigned>(detail::small_primes.size())
                               : rounds;
    const xuint spread = n - xuint(3u); // base = 2 + hash mod spread, so 2..n-2
    for (unsigned i = 0; i < total; ++i) {
        xuint a(0u);
        if (i < detail::small_primes.size()) {
            a = xuint(detail::small_primes[i]);
        } else {
            std::uint64_t h = detail::mix64(static_cast<std::uint64_t>(i));
            for (std::size_t k = 0; k < n.limb_size(); ++k)
                h = detail::mix64(h ^ n.limb_at(k));
            a = xuint(h) % spread + xuint(2u);
        }
        if (detail::is_composite_witness(n, a, d, s))
            return false;
    }
    return true;
}

inline bool is_probable_prime(const xint& n, unsigned rounds = 24) {
    return !n.is_negative() && is_probable_prime(n.magnitude(), rounds);
}

inline xuint next_prime(const xuint& from) {
    xuint n = from + xuint(1u);
    if (n <= xuint(2u))
        return xuint(2u);
    if (n.is_even())
        ++n;
    while (!is_probable_prime(n))
        n += xuint(2u);
    return n;
}

inline xuint prev_prime(const xuint& from) {
    if (from <= xuint(2u))
        throw std::domain_error("alt::prev_prime: no prime below the argument");
    if (from == xuint(3u))
        return xuint(2u);
    xuint n = from - xuint(1u);
    if (n.is_even())
        --n;
    while (!is_probable_prime(n))
        n -= xuint(2u);
    return n;
}

// ---------------------------------------------------------------------------
// Random numbers.  Any uniform random bit generator whose result_type covers
// its whole range works, for example std::mt19937 or std::mt19937_64.
// ---------------------------------------------------------------------------

template <class Generator>
inline xuint random_bits(Generator& g, std::size_t bits) {
    using raw = typename Generator::result_type;
    constexpr std::size_t raw_bits = sizeof(raw) * 8;
    if (bits == 0)
        return xuint();
    xuint r;
    std::size_t made = 0;
    while (made < bits) {
        const std::size_t take = (bits - made < raw_bits) ? bits - made : raw_bits;
        std::uint64_t v = static_cast<std::uint64_t>(g());
        if (take < sizeof(std::uint64_t) * 8)
            v &= (std::uint64_t(1) << take) - std::uint64_t(1);
        r = (r << take) | xuint(v);
        made += take;
    }
    return r;
}

// Uniform value in [0, bound) by rejection sampling.
template <class Generator>
inline xuint random_below(Generator& g, const xuint& bound) {
    if (bound.is_zero())
        throw std::domain_error("alt::random_below: empty range");
    const std::size_t bits = bound.bit_size();
    for (;;) {
        xuint v = random_bits(g, bits);
        if (v < bound)
            return v;
    }
}

} // namespace axint_v1

} // namespace alt

// ---------------------------------------------------------------------------
// Hooks into namespace std.
//
// std::numeric_limits is deliberately not specialised.  Both types are
// unbounded, so digits(), min(), max() and friends have no meaningful value,
// and a half filled specialisation is worse than none.  Use alt::bit_size and
// alt::sizeinbase to ask how wide a value is, and xint::to<T>() /
// xuint::to<T>() to check whether it fits a fixed width type exactly.
//
// Define AXINT_ENABLE_FORMAT before including this header (and link against a
// standard library with std::format) to also get std::formatter support.  The
// accepted grammar is a subset of the standard one:
//
//     [[fill]align] sign ['#'] ['0'] [width] [type],  sign in + - ' ',
//                                                      type in b B o d x X
//
// an alignment may appear without a fill character, the sign, the '#' and the
// '0' have to appear in that order and each of them at most once (a fill and an
// alignment always come first), and the fill applies outside the sign and the
// radix prefix: "{:*^#10x}" yields "**0xfffb**".  The '0' flag
// inserts the padding between the sign (and the radix prefix) and the digits,
// and it is ignored when an alignment was written down explicitly, exactly as
// std::format does for the built in integers.  The '#' flag prepends "0x" or
// "0X" to hexadecimal and "0b" or "0B" to binary always, and a single "0" to
// octal only when the value is not zero, so "{:#o}" of 0 is "0" while "{:#x}"
// of 0 is "0x0".  Precision, thousands grouping, the 'n' and 'c' types and the
// dynamic width/precision forms are not supported; the '=' alignment and the
// ',' and '_' grouping characters are rejected too.
// ---------------------------------------------------------------------------

namespace alt {
inline namespace axint_v1 {
namespace detail {

// Hash of a magnitude.  Leading zero limbs are skipped so that equal values
// always hash alike, no matter how they were built.
inline std::size_t hash_magnitude(const limb_t* a, size_t n, std::uint64_t seed) noexcept {
    while (n != 0 && a[n - 1] == 0)
        --n;
    std::uint64_t h = seed ^ (static_cast<std::uint64_t>(n) * 0x9e3779b97f4a7c15ull);
    for (size_t i = 0; i < n; ++i)
        h = mix64(h ^ static_cast<std::uint64_t>(a[i]));
    return static_cast<std::size_t>(h);
}

#if defined(AXINT_ENABLE_FORMAT)

// Parsed part of a format specification, shared by both formatters.
struct format_specs {
    char fill = ' ';
    char align = '>'; // numbers are right aligned unless told otherwise
    bool explicit_align = false; // an alignment really was written down
    bool alternate = false;
    bool show_plus = false;
    bool show_space = false;
    bool pad_inside = false; // the '0' flag: padding between sign and digits
    std::size_t width = 0;
    unsigned base = 10;
    bool upper = false;

    static constexpr bool align_of(char c, char& out) noexcept {
        switch (c) {
            case '<': out = '<'; return true;
            case '>': out = '>'; return true;
            case '^': out = '^'; return true;
            default: return false;
        }
    }

    // The specification characters are all ASCII, so the character type of the
    // format string plays no role while parsing.
    template <class C>
    static constexpr char narrow(C c) noexcept {
        return static_cast<char>(static_cast<unsigned char>(c));
    }

    // Note on the closing brace: the standard lets an implementation hand the
    // specification alone to a formatter, but MSVC's compile-time checker passes
    // the whole format string instead, so a '}' has to end the specification here
    // (the caller is the one that validates it).
    template <class It>
    constexpr It parse(It it, It end) {
        if (it == end || narrow(*it) == '}')
            return it;
        It next = it;
        ++next;
        char kind = '>';
        if (next != end && align_of(narrow(*next), kind)) {
            fill = narrow(*it); // the character before an alignment is the fill
            align = kind;
            explicit_align = true;
            it = next;
            ++it;
        } else if (align_of(narrow(*it), kind)) {
            align = kind; // an alignment without a fill character
            explicit_align = true;
            ++it;
        }
        // Every flag may appear at most once and the three sign characters are
        // mutually exclusive; anything else is left for the caller to reject.
        // Note that '-' is a legal sign: it simply never prints a plus.
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
        if (it != end && narrow(*it) == '0') {
            // The '0' flag pads with zeroes between the sign (and the radix
            // prefix) and the digits.  An explicitly written alignment means
            // the caller knows what he wants and the flag is ignored then.
            if (!explicit_align) {
                pad_inside = true;
                fill = '0';
                align = '>';
            }
            ++it;
        }
        while (it != end) {
            const char c = narrow(*it);
            if (c < '0' || c > '9')
                break;
            width = width * 10u + static_cast<std::size_t>(c - '0');
            ++it;
        }
        if (it != end) {
            switch (narrow(*it)) {
                case 'b': base = 2; break;
                case 'B': base = 2; upper = true; break;
                case 'o': base = 8; break; // there is no 'O' in the standard
                case 'd': base = 10; break;
                case 'x': base = 16; break;
                case 'X': base = 16; upper = true; break;
                default: return it; // left for the caller to reject
            }
            ++it;
        }
        return it;
    }
};

// True when the iterator stopped at the end of a specification: either the real
// end (libstdc++ style) or the closing brace (MSVC style).
template <class It, class End>
constexpr bool spec_finished(It it, End end) noexcept {
    return it == end || format_specs::narrow(*it) == '}';
}

// Renders a magnitude (plus an optional sign) into ASCII, padding applied.
inline std::string format_magnitude(const xuint& m, const format_specs& s, bool negative) {
    std::string body = m.to_string(s.base);
    if (s.upper)
        body = upper_digits(std::move(body));
    std::string prefix;
    if (negative)
        prefix = "-";
    else if (s.show_plus)
        prefix = "+";
    else if (s.show_space)
        prefix = " ";
    if (s.alternate) {
        // Hexadecimal and binary are always prefixed, octal only when the
        // value has at least one digit, which is what the '0' would stand for.
        if (s.base == 16)
            prefix += s.upper ? "0X" : "0x";
        else if (s.base == 2)
            prefix += s.upper ? "0B" : "0b";
        else if (s.base == 8 && !m.is_zero())
            prefix += "0";
    }
    const std::size_t content = prefix.size() + body.size();
    if (s.width > content) {
        const std::size_t pad = s.width - content;
        if (s.pad_inside) {
            body.insert(body.begin(), pad, '0'); // -0x000ff keeps the sign first
        } else if (s.align == '<') {
            body.append(pad, s.fill);
        } else if (s.align == '^') {
            const std::size_t left = pad / 2;
            body.insert(body.end(), pad - left, s.fill);
            prefix.insert(prefix.begin(), left, s.fill);
        } else {
            prefix.insert(prefix.begin(), pad, s.fill);
        }
    }
    return prefix + body;
}

// Widens the ASCII result when the output character type is not char.
template <class CharT, class OutIt>
OutIt copy_widen(const std::string& text, OutIt out) {
    for (const char c : text)
        *out++ = static_cast<CharT>(static_cast<unsigned char>(c));
    return out;
}

#endif // AXINT_ENABLE_FORMAT

} // namespace detail
} // namespace axint_v1
} // namespace alt

namespace std {

template <>
struct hash<alt::xuint> {
    std::size_t operator()(const alt::xuint& v) const noexcept {
        return alt::detail::hash_magnitude(v.limb_data(), v.limb_size(), 0ull);
    }
};

template <>
struct hash<alt::xint> {
    std::size_t operator()(const alt::xint& v) const noexcept {
        // Mixing the sign in keeps the hash consistent with operator== while
        // still separating x from -x in most cases.
        const std::uint64_t seed =
            v.is_negative() ? 0x59a373799cfc28a8ull : 0ull;
        const alt::xuint& m = v.magnitude();
        return alt::detail::hash_magnitude(m.limb_data(), m.limb_size(), seed);
    }
};

} // namespace std

#if defined(AXINT_ENABLE_FORMAT)

#if defined(__has_include)
#if __has_include(<format>)
#include <format>
#endif
#endif

#if !defined(__cpp_lib_format)
#error "AXINT_ENABLE_FORMAT is set but the standard library has no std::format"
#else

namespace std {

template <class CharT>
struct formatter<alt::xuint, CharT> {
    alt::detail::format_specs specs;

    constexpr auto parse(basic_format_parse_context<CharT>& ctx) {
        const auto it = specs.parse(ctx.begin(), ctx.end());
        if (!alt::detail::spec_finished(it, ctx.end()))
            throw std::format_error("alt: unsupported format specification");
        return it;
    }

    template <class FormatContext>
    auto format(const alt::xuint& v, FormatContext& ctx) const {
        const std::string text = alt::detail::format_magnitude(v, specs, false);
        return alt::detail::copy_widen<CharT>(text, ctx.out());
    }
};

template <class CharT>
struct formatter<alt::xint, CharT> {
    alt::detail::format_specs specs;

    constexpr auto parse(basic_format_parse_context<CharT>& ctx) {
        auto it = specs.parse(ctx.begin(), ctx.end());
        if (!alt::detail::spec_finished(it, ctx.end()))
            throw std::format_error("alt: unsupported format specification");
        return it;
    }

    template <class FormatContext>
    auto format(const alt::xint& v, FormatContext& ctx) const {
        const std::string text =
            alt::detail::format_magnitude(v.magnitude(), specs, v.is_negative());
        return alt::detail::copy_widen<CharT>(text, ctx.out());
    }
};

} // namespace std

#endif // __cpp_lib_format
#endif // AXINT_ENABLE_FORMAT

#endif // AXINT_H_INCLUDED
