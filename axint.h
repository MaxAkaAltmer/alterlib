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

// ---------------------------------------------------------------------------
// Limb array primitives. Little endian order, normal form means no leading
// zero limbs (the empty array is zero).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The carry flag forms of the two limb kernels.
//
// Adding two limbs and a carry is one ADC instruction, and the carry it leaves
// behind is already in the place the next ADC wants it.  Written as values - a
// sum, a comparison, the sum of two comparisons - the same limb costs the
// compiler ten instructions and a five link dependency chain, because the carry
// travels in a register as a 0 or a 1 rather than in a flag.  Measured on MSVC
// x64 at /O2 that is 1.14 ns per limb against 0.38 for a chain of ADCs, and it
// is a difference in code generation rather than in algorithm: both forms compute
// the same word and return the same 0 or 1.  Only 64 bit x86 gets the intrinsic
// form, because that is where the instruction exists; everywhere else the
// portable form below is compiled, and AXINT_NO_CARRY_FLAG chooses it here too,
// which is how the two were compared (BENCHMARKS.md).  AXINT_NO_FAST_PATHS turns
// it off as it turns off every other accelerator.
//
// The shape of the loop then mattered as much as the flag.  Driven by an index into
// three arrays the pass costs 60.0 ns for 128 limbs and 239.2 for 512; with pointers
// that walk and four limbs per round the same pass costs 45.3 and 180.6, and one limb
// per round is worse than either (90.4 and 359.0), because the bookkeeping of a loop
// that turns once per limb outweighs the two loads, the add and the store.  Above the
// last level of cache all shapes agree, where the pass is limited by memory.  The rows
// are in build\kshape.exe, which measures the kernel the header compiles beside five
// rewrites of it on the same operands; the two forms the header considered first are
// also equal there, which is what makes the difference a property of the code rather
// than of the harness.  Kernels are only ever given regions that do not overlap, so
// writing ahead of the read within a round is allowed.
// ---------------------------------------------------------------------------
#if !defined(AXINT_NO_CARRY_FLAG) && defined(ALT_HAVE_MSVC_INTRINSICS) && defined(_M_X64)
#define ALT_HAVE_CARRY_FLAG 1
#endif

// r[0,n) = a[0,n) + b[0,n), returns the carry out. Regions must not overlap.
#if defined(ALT_HAVE_CARRY_FLAG)
inline limb_t add_limbs(limb_t* r, const limb_t* a, const limb_t* b, size_t n) noexcept {
    unsigned char carry = 0;
    for (; n >= 4; n -= 4, r += 4, a += 4, b += 4) {
        carry = _addcarry_u64(carry, a[0], b[0], r);
        carry = _addcarry_u64(carry, a[1], b[1], r + 1);
        carry = _addcarry_u64(carry, a[2], b[2], r + 2);
        carry = _addcarry_u64(carry, a[3], b[3], r + 3);
    }
    for (; n != 0; --n, ++r, ++a, ++b)
        carry = _addcarry_u64(carry, a[0], b[0], r);
    return limb_t(carry);
}
#else
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
#endif

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
#if defined(ALT_HAVE_CARRY_FLAG)
inline limb_t sub_limbs(limb_t* r, const limb_t* a, const limb_t* b, size_t n) noexcept {
    unsigned char borrow = 0;
    for (; n >= 4; n -= 4, r += 4, a += 4, b += 4) {
        borrow = _subborrow_u64(borrow, a[0], b[0], r);
        borrow = _subborrow_u64(borrow, a[1], b[1], r + 1);
        borrow = _subborrow_u64(borrow, a[2], b[2], r + 2);
        borrow = _subborrow_u64(borrow, a[3], b[3], r + 3);
    }
    for (; n != 0; --n, ++r, ++a, ++b)
        borrow = _subborrow_u64(borrow, a[0], b[0], r);
    return limb_t(borrow);
}
#else
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
#endif

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

// The three binary bitwise operations.  Which of the three is a compile time
// choice wherever one of them is actually being computed - the sign cases of
// the signed class all reduce to one pass (see bitwise_pass), and a loop that
// combines limbs has no business branching on the operation per limb - so the
// operation is a template parameter and the value below exists for the callers
// that carry it around as a value.
enum class bit_op { bit_and, bit_or, bit_xor };

template <bit_op Op>
constexpr limb_t bit_combine(limb_t a, limb_t b) noexcept {
    if constexpr (Op == bit_op::bit_and)
        return a & b;
    else if constexpr (Op == bit_op::bit_or)
        return a | b;
    else
        return a ^ b;
}

inline limb_t apply_bit_op(limb_t a, limb_t b, bit_op o) noexcept {
    switch (o) {
        case bit_op::bit_and: return bit_combine<bit_op::bit_and>(a, b);
        case bit_op::bit_or:  return bit_combine<bit_op::bit_or>(a, b);
        default:              return bit_combine<bit_op::bit_xor>(a, b);
    }
}

// r[0,max(an,bn)) = a[0,an) op b[0,bn), for the two operations a limb the
// shorter operand does not have changes nothing - combining with the zero it
// implies leaves the other limb as it stands.  So the limbs both operands have
// are combined and what is left of the longer one is moved, each part being one
// pass with one bound.  Reading a limb that might not be there costs a test per
// limb, and one loop over max(an,bn) does exactly that: measured over an octave
// ladder it made these two operations two and a half times the cost of the and
// at the same size, which is a property of the loop rather than of the
// operation (build\bit_fold.txt).  r must not overlap a or b; the in place form
// below is the one for growing a magnitude where it lies.
template <bit_op Op>
inline void bit_wide(limb_t* r, const limb_t* a, size_t an, const limb_t* b,
                     size_t bn) noexcept {
    static_assert(Op != bit_op::bit_and, "an and does not reach past the shorter operand");
    if (an < bn) {
        std::swap(an, bn);
        std::swap(a, b); // a is the longer operand from here on
    }
    for (size_t i = 0; i < bn; ++i)
        r[i] = bit_combine<Op>(a[i], b[i]);
    if (an > bn)
        std::copy_n(a + bn, an - bn, r + bn);
}

// The same operation with the result landing on the first operand, which is
// where an |= b or an ^= b wants it: r has room for max(an,bn) limbs already,
// an is how many of them were there before, and anything the resize added is
// zero - which is the limb a shorter operand stands for, so the limbs b has on
// its own simply move in.  r and b must not overlap; an of r's own length being
// what it was, a |= a is the shared part combined with itself and nothing else.
template <bit_op Op>
inline void bit_wide_in(limb_t* r, size_t an, const limb_t* b, size_t bn) noexcept {
    static_assert(Op != bit_op::bit_and, "an and never grows the magnitude it is given");
    const size_t shared = an < bn ? an : bn;
    for (size_t i = 0; i < shared; ++i)
        r[i] = bit_combine<Op>(r[i], b[i]);
    if (bn > an)
        std::copy_n(b + an, bn - an, r + an);
}

// r = a + b for magnitudes of any two lengths, r having room for one limb more
// than the longer operand and coming back with the length of the sum.  r may
// alias either operand, which is what lets a kernel add where the answer belongs
// instead of into a buffer of its own.
inline size_t add_wide(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn) {
    if (an < bn) {
        std::swap(an, bn);
        std::swap(a, b); // a is the longer operand from here on
    }
    limb_t c = add_limbs(r, a, b, bn);
    if (an > bn) {
        if (r != a)
            std::copy_n(a + bn, an - bn, r + bn);
        if (c != 0)
            c = add_limb(r + bn, r + bn, an - bn, c);
    }
    if (c != 0) {
        r[an] = c;
        return an + 1;
    }
    return mag_size(r, an);
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
// Scratch memory.
//
// Every kernel below knows the size of what it is about to produce before it
// starts: a product is an + bn limbs, a quotient un - dn + 1, a Karatsuba tree
// a fixed multiple of the operand. A block of exactly that size is therefore
// taken once, at the top of the operation, and carved up by the parts beneath
// it. That matters because the kernels are used inside loops - Euclid's,
// Newton's, the radix conversion - where a fresh std::vector per round means
// both an allocation and a pass that zeroes memory the kernel overwrites
// anyway; measured, that was four fifths of the time of a one limb gcd and a
// third of a Karatsuba product.
// ---------------------------------------------------------------------------

// A mark inside such a block. A kernel that recurses passes the mark on by
// value: each level carves its own region from it, so what is live at any
// moment is one region per level of the recursion instead of one per node, and
// siblings share the space their predecessor released.
struct mark {
    limb_t* p = nullptr;
    size_t left = 0;

    limb_t* take(size_t n) noexcept {
        assert(n <= left);
        left -= n;
        limb_t* r = p;
        p += n;
        return r;
    }
    mark below() const noexcept { return mark{p, left}; }
};

// A block of limbs, uninitialised: the kernels write what they need. Blocks up
// to a few hundred bits stay on the stack, which is the size range most
// programs live in.
class scratch {
  public:
    explicit scratch(size_t n) : n_(n) {
        if (n <= inline_limbs) {
            p_ = store_;
        } else {
            p_ = static_cast<limb_t*>(::operator new(n * sizeof(limb_t)));
            if (p_ == nullptr)
                throw std::bad_alloc();
        }
    }
    scratch(const scratch&) = delete;
    scratch& operator=(const scratch&) = delete;
    ~scratch() {
        if (p_ != store_)
            ::operator delete(p_);
    }

    limb_t* data() const noexcept { return p_; }
    size_t size() const noexcept { return n_; }
    mark top() const noexcept { return mark{p_, n_}; }

  private:
    static constexpr size_t inline_limbs = 64; // half a KiB on the stack

    limb_t store_[inline_limbs];
    limb_t* p_;
    size_t n_;
};

// ---------------------------------------------------------------------------
// Multiplication: a schoolbook product of rows while the factors are short, a
// schoolbook of columns in between, the Karatsuba tree above that, and a Toom-3
// one on top of the tree, where the operands are wide enough for its
// interpolation to stop minding the work it saves.
// ---------------------------------------------------------------------------

// Where the tree stops splitting: below this many limbs in the shorter factor a
// leaf runs the schoolbook instead, because the three half products cost more in
// added and moved limbs than they save, until the operand is wide enough for the
// third of them to pay for itself.  It is a build time knob (define
// AXINT_KARATSUBA_THRESHOLD before this header) because the balance is a property
// of the machine's multiplier and its memory, not of the numbers, and the
// crossover moves by a factor of two between compilers.
#ifndef AXINT_KARATSUBA_THRESHOLD
#define AXINT_KARATSUBA_THRESHOLD 32
#endif

inline constexpr size_t karatsuba_threshold = AXINT_KARATSUBA_THRESHOLD;

// A product only enters the tree once it is several leaves wide, which is a
// second threshold of its own: a node spends a pass over its operands padding
// them, a pass summing the halves and a pass recombining the three products, so
// near the bottom those passes cost more than the third of a product the split
// saves.  Measured on this machine the tree starts to pay a little under two
// leaves, and only once its children are given the operands' real widths - with
// padded ones the same crossover sat at four leaves, because a node of operands
// of unequal width spends its products on zero limbs.  Between the two
// thresholds the plain schoolbook runs, in its column shape.
#ifndef AXINT_KARATSUBA_ENTRY
#define AXINT_KARATSUBA_ENTRY 64
#endif

inline constexpr size_t karatsuba_entry = AXINT_KARATSUBA_ENTRY;

// Whether a product of an and bn limbs is wide enough for the tree to be worth
// its overhead at all.  Inside the tree the leaves still stop at
// karatsuba_threshold, so entry and leaf are two different sizes and the
// schoolbook in between runs in its column shape.
constexpr bool use_karatsuba(size_t an, size_t bn) noexcept {
    return (an < bn ? an : bn) >= karatsuba_entry;
}

static_assert(karatsuba_entry >= karatsuba_threshold,
              "a tree cannot have leaves wider than the product that enters it");

// Toom-Cook 3-way, the shape the tree takes well above the Karatsuba entry: each
// operand is cut into thirds and the five coefficients of the product of the two
// resulting polynomials are recovered from the five values that product takes at
// 0, 1, -1, 2 and infinity.  Five multiplications of a third of the size then
// replace the three of a half size a Karatsuba node spends, n^1.465 against
// n^1.585, but they also replace the two sums of a Karatsuba node with a dozen
// passes over the same limbs, which is why the shape only pays far above the
// point Karatsuba itself starts to.  A sixth point (at -2, say) would make the
// divisions kinder and the exponent worse - log3(6) is 1.63, above Karatsuba's own
// 1.585 - so the fifth point is the last one, and the interpolation pays for it
// with one exact division by three.
//
// Where it starts is measured rather than inherited, and the measurement is a wide
// one: over randomised sizes this shape cost three percent at sixty four limbs,
// saved five at a hundred and twenty eight and eight above two hundred and fifty
// six, so the entry sits a power of two above the Karatsuba one, at the eight
// thousand bits where the sums stop costing more than the product they save.  It
// is a build time knob for the same reason Karatsuba's is.
#ifndef AXINT_TOOM3_ENTRY
#define AXINT_TOOM3_ENTRY 128
#endif

inline constexpr size_t toom3_entry = AXINT_TOOM3_ENTRY;

// How much of the longer factor the shorter one has to reach, in percent: fifty
// six accepts a product whose factors are up to one and three quarters apart.  A
// third of the longer factor is what a node cuts both operands into, so the shorter
// the second factor is, the more of those thirds come out empty and the more of the
// node's five products are spent on padding - and the tree it falls back to halves
// the long factor and trims the short one, which is a better answer to the shape
// the narrower it gets.  Where the two change places is measured, and measured on
// the shape rather than on the size, because a random pair lands on no ratio
// exactly: comparing the two bounds over randomised sizes, a pair between one and a
// half and one and three quarters apart was 8.6% quicker with the node, a pair
// between one and three quarters and two was 5.9% quicker without it and a pair
// exactly twice apart 4.7% quicker without it, while the shapes both bounds treat
// the same way - the squares, the three to four pairs, the ones past two - moved by
// about a percent, which is what the same difference in two builds is worth when it
// is not a difference in the policy at all.  Fifty six lies between the last shape
// measured worth a node and the first measured not worth one; it is a knob like the
// entry because where two shapes change places is a property of the machine.  Well
// past this bound what answers the shape is not a smaller bound but the uneven
// splits, which cut the long factor into fewer parts than the short one; they are
// not here yet.
#ifndef AXINT_TOOM3_MIN_PC
#define AXINT_TOOM3_MIN_PC 56
#endif

inline constexpr size_t toom3_min_pc = AXINT_TOOM3_MIN_PC;

// Whether a product of an and bn limbs, an >= bn, is worth a Toom-3 node.  The
// second half of the test is the bound above, written so that no division and no
// floating point enters it.
constexpr bool use_toom3(size_t an, size_t bn) noexcept {
    // Both sides are multiplied in sixty four bit arithmetic.  On the sixty four
    // bit build that is what the limbs are anyway; on the thirty two bit one a
    // size_t is thirty two bits and an operand of three gigabits - ninety four
    // million limbs - would wrap the test, silently, into accepting anything.
    return bn >= toom3_entry &&
           std::uint64_t(bn) * 100 >= std::uint64_t(an) * toom3_min_pc;
}

static_assert(toom3_entry >= karatsuba_entry,
              "a Toom-3 node is a node of the tree, not a shape below it");
static_assert(toom3_entry >= 16,
              "a Toom-3 node has to be wide enough for its temporaries to fit the "
              "six limbs per limb of operand the scratch below books for a node");
static_assert(toom3_min_pc != 0 && toom3_min_pc <= 100,
              "the bound is a share of the longer factor: zero accepts every shape "
              "and more than a hundred accepts none, neither being what a bound says");

// r[0,an+bn) = a[0,an) * b[0,bn), the schoolbook, column by column.
//
// Walking a column instead of a row is what keeps the products off the critical
// path: the running total of a row is one long carry chain through memory, one
// limb per product, while a column merely counts how often the sum of the low
// halves of its products wrapped around and how often the sum of the high halves
// did, two independent counts a processor overlaps freely.  A column holds at
// most min(an,bn) products, so both counts stay far below a limb and what one
// column hands to the next is two limbs: the limb it could not place and the
// wraps of the high halves plus the wrap of the sum.  The result array is
// written once per limb and never read back, which on its own is about a third
// of the difference the two shapes make.
inline void mul_schoolbook_cols(limb_t* r, const limb_t* a, size_t an, const limb_t* b,
                                size_t bn) {
    limb_t low = 0, high = 0; // what the columns before this one still owe
    for (size_t k = 0; k < an + bn; ++k) {
        const size_t from = k + 1 > bn ? k - bn + 1 : 0; // the pairs (i, k - i)
        const size_t upto = k < an ? k : an - 1;         // with both in range
        limb_t s = 0, h = 0, sw = 0, hw = 0;             // sums and their wraps
        for (size_t cnt = upto >= from ? upto - from + 1 : 0; cnt != 0; --cnt) {
            const size_t i = from + cnt - 1;
            const wide64 p = mul_wide(a[i], b[k - i]);
            const limb_t s2 = s + p.lo;
            sw += limb_t(s2 < s);
            s = s2;
            const limb_t h2 = h + p.hi;
            hw += limb_t(h2 < h);
            h = h2;
        }
        // The column is s + (h + sw) * base + hw * base^2 and the columns before
        // it still owe low + high * base, so one limb goes to the result and the
        // rest, three sums and the wrap of the limb, is what the next column
        // starts from.
        limb_t x = s + low;
        limb_t debt = limb_t(x < low);
        limb_t y = h + sw;
        limb_t carry = limb_t(y < h);
        const limb_t y1 = y + high;
        carry += limb_t(y1 < y);
        const limb_t y2 = y1 + debt;
        carry += limb_t(y2 < y1);
        r[k] = x;
        low = y2;
        high = hw + carry;
    }
    assert(low == 0 && high == 0); // a product never overflows its own array
}

// The same product row by row, the shape that wins once the operands are short
// enough for the per column bookkeeping above to outweigh the chains it saves.
inline void mul_schoolbook_rows(limb_t* r, const limb_t* a, size_t an, const limb_t* b,
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

// From how many limbs in the shorter factor the column shape wins.  Below it the
// four sums and the two range checks of a column cost more than the carry chains
// they remove, and a column of a short operand only holds a product or two.
#ifndef AXINT_COLUMN_PRODUCT_THRESHOLD
#define AXINT_COLUMN_PRODUCT_THRESHOLD 32
#endif

inline constexpr size_t column_product_threshold = AXINT_COLUMN_PRODUCT_THRESHOLD;

// r[0,an+bn) = a[0,an) * b[0,bn). r must not overlap a or b.
inline void mul_schoolbook(limb_t* r, const limb_t* a, size_t an, const limb_t* b,
                           size_t bn) {
    if (an == 0 || bn == 0) {
        std::fill_n(r, an + bn, limb_t(0));
    } else if ((an < bn ? an : bn) < column_product_threshold) {
        mul_schoolbook_rows(r, a, an, b, bn);
    } else {
        mul_schoolbook_cols(r, a, an, b, bn);
    }
}

// ---------------------------------------------------------------------------
// Squaring.
//
// A square is a product whose two factors are one and the same number, and half of
// a product's partial products are the mirror of the other half.  Both shapes below
// collect the pairs (i, j) with i < j once, count each of them twice, and lay the
// diagonal a[i] * a[i] down where the mirror of that pair would have put the same
// sum again: n(n-1)/2 multiplications where a product of the same width spends n^2.
// What they cannot save is the traffic - the mirrored half is added twice rather
// than multiplied twice, which is why a square is a good deal quicker than a product
// of its width rather than twice as quick.
//
// What says the two factors are one number is their being the same array, asked at
// every node by the predicate below.  A second object holding an equal value is not
// seen: the alternative would be a pass over the limbs of every product the library
// makes, asking a question the caller answered when it wrote a * a.  It also keeps
// the two cases measurable apart, since a product of two objects of equal value is
// then still the product, and what the mirror was worth is the distance between them.
// ---------------------------------------------------------------------------

constexpr bool is_square(const limb_t* a, size_t an, const limb_t* b,
                         size_t bn) noexcept {
    return an == bn && a == b;
}

// From how many limbs the column shape wins for a square.  It starts where a product
// starts and is a knob of its own because the mirror changes what the two shapes are
// made of: half the multiplications and the doubling, which a product never pays for,
// falls in different places in each of them.
#ifndef AXINT_SQR_COLUMN_THRESHOLD
#define AXINT_SQR_COLUMN_THRESHOLD AXINT_COLUMN_PRODUCT_THRESHOLD
#endif

inline constexpr size_t sqr_column_threshold = AXINT_SQR_COLUMN_THRESHOLD;

// r[0,2n) = a[0,n)^2, row by row.
//
// Row i is a[i] against the limbs above it, so it holds each pair of the upper triangle
// once and nothing else: the product kernel walks the whole rectangle, this one the
// triangle above its diagonal.  What the mirror is worth is then paid for once for the
// whole result instead of once per row - the triangle is twice the sum of the off
// diagonal pairs, so the array is shifted up by a bit at the end, which is 2n limbs
// rather than n rows of up to n of them.  A doubling per row was the first shape of this
// kernel and it is what made it no cheaper than the product: half the multiplications,
// but two passes over every row to place them, where the product manages with one.
//
// The diagonal is what the doubling must not touch, since a[i] * a[i] has no mirror; it
// is therefore laid down afterwards, each limb at 2i where the pair below it and its
// mirror meet.
inline void sqr_schoolbook_rows(limb_t* r, const limb_t* a, size_t n) {
    std::fill_n(r, 2 * n, limb_t(0));
    for (size_t i = 0; i + 1 < n; ++i) {
        limb_t carry = 0;
        for (size_t j = i + 1; j < n; ++j) {
            const wide64 w = mul_wide(a[i], a[j]);
            const limb_t s1 = r[i + j] + carry;
            const limb_t c1 = limb_t(s1 < carry);
            const limb_t s2 = s1 + w.lo;
            const limb_t c2 = limb_t(s2 < s1);
            r[i + j] = s2;
            carry = w.hi + c1 + c2;
        }
        // What the row could not place belongs at limb i + n, which is a limb no
        // earlier row reached (a row above this one starts below it, and its own carry
        // lands further still), so it can be written rather than added - the same
        // argument the product kernel makes about its r[i + an].
        r[i + n] = carry;
    }
    // Twice the triangle.  The limb the top loses is the bit the result gains, and a
    // square fits its 2n limbs, so that bit has nowhere to go but off the end.
    limb_t spill = 0;
    for (size_t k = 0; k < 2 * n; ++k) {
        const limb_t v = r[k];
        r[k] = (v << 1) | spill;
        spill = v >> (limb_bits - 1);
    }
    assert(spill == 0);
    for (size_t i = 0; i < n; ++i) {
        const wide64 d = mul_wide(a[i], a[i]);
        const limb_t s = r[2 * i] + d.lo;
        limb_t owe = d.hi + limb_t(s < d.lo);
        r[2 * i] = s;
        for (size_t k = 2 * i + 1; k < 2 * n && owe != 0; ++k) {
            const limb_t t = r[k] + owe;
            owe = limb_t(t < owe);
            r[k] = t;
        }
        assert(owe == 0);
    }
}

// The same square column by column, the shape that wins once the operand is wide
// enough for the per column bookkeeping to stop minding the chains it saves.  A column
// of the upper triangle is half a column of the product - the pairs (i, k - i) with i
// below k - i, the other half being their mirrors - so it is summed once, and twice the
// column is taken at the end, which is two shifts of a running total rather than four
// extra sums per pair.  What a product's column does not have is the diagonal limb an
// even column owns, the one pair whose mirror is itself; it goes in after the doubling,
// since doubling it twice would count it twice over.  The three sums a column hands to
// the next are the product's, unchanged.
inline void sqr_schoolbook_cols(limb_t* r, const limb_t* a, size_t n) {
    limb_t low = 0, high = 0; // what the columns before this one still owe
    for (size_t k = 0; k < 2 * n; ++k) {
        const size_t from = k + 1 > n ? k - n + 1 : 0; // the pairs (i, k - i)
        limb_t s = 0, h = 0, sw = 0, hw = 0;           // sums and their wraps
        if (k != 0) {                                  // column 0 owns no pair at all
            const size_t lim = (k - 1) / 2;            // the last i with i below k - i
            const size_t upto = lim < n ? lim : n - 1; // and still inside the operand
            for (size_t cnt = upto >= from ? upto - from + 1 : 0; cnt != 0; --cnt) {
                const size_t i = from + cnt - 1;
                const wide64 p = mul_wide(a[i], a[k - i]);
                const limb_t s2 = s + p.lo;
                sw += limb_t(s2 < s);
                s = s2;
                const limb_t h2 = h + p.hi;
                hw += limb_t(h2 < h);
                h = h2;
            }
        }
        // Twice the mirrored half.  A column is s + (h + sw) * base + hw * base^2, so
        // doubling it doubles all four: the bit the doubled s loses at the top is a
        // wrap of that doubling on top of the two the pairs left there, and the same
        // for h - the counts are part of the value, not a note about it.
        const limb_t sw1 = s >> (limb_bits - 1);
        const limb_t hw1 = h >> (limb_bits - 1);
        s <<= 1;
        h <<= 1;
        sw = (sw << 1) + sw1;
        hw = (hw << 1) + hw1;
        if ((k & 1) == 0 && k / 2 < n) { // the one limb of an even column, its own
            const wide64 d = mul_wide(a[k / 2], a[k / 2]);
            const limb_t s2 = s + d.lo;
            sw += limb_t(s2 < s);
            s = s2;
            const limb_t h2 = h + d.hi;
            hw += limb_t(h2 < h);
            h = h2;
        }
        // The column is s + (h + sw) * base + hw * base^2 and the columns before it
        // still owe low + high * base, exactly as they do for a product.
        limb_t x = s + low;
        limb_t debt = limb_t(x < low);
        limb_t y = h + sw;
        limb_t carry = limb_t(y < h);
        const limb_t y1 = y + high;
        carry += limb_t(y1 < y);
        const limb_t y2 = y1 + debt;
        carry += limb_t(y2 < y1);
        r[k] = x;
        low = y2;
        high = hw + carry;
    }
    assert(low == 0 && high == 0); // a square never overflows its own array
}

// From how many limbs a square is worth a kernel of its own.  Below it the triangle is
// too short to pay for the pass that doubles it: a product spends one multiply and one
// add per cell of the rectangle, this spends one multiply and one add per cell of the
// triangle plus a shift of the result and a diagonal, and at a few limbs the last two
// are the whole cost.  The plain product is therefore handed the square again, which is
// neither a win nor a loss - it is the width where the mirror stops being worth asking
// about.
#ifndef AXINT_SQR_MIN_LIMBS
#define AXINT_SQR_MIN_LIMBS 8
#endif

inline constexpr size_t sqr_min_limbs = AXINT_SQR_MIN_LIMBS;

static_assert(sqr_min_limbs >= 1,
              "a bound of zero would be no bound, the shape below it being the product");

// r[0,2n) = a[0,n)^2, the schoolbook, in whichever shape wins at this width.
inline void sqr_schoolbook(limb_t* r, const limb_t* a, size_t n) {
    if (n == 0)
        return;
    if (n < sqr_min_limbs)
        mul_schoolbook_rows(r, a, n, a, n); // the two factors being one number is a
                                            // question this width cannot answer
    else if (n < sqr_column_threshold)
        sqr_schoolbook_rows(r, a, n);
    else
        sqr_schoolbook_cols(r, a, n);
}

inline void mul_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn,
                    mark m);

inline void toom3_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn,
                      mark m);

// The one place the multiplication section below needs the division section is
// the exact division by three that ends a Toom-3 interpolation; the kernel itself
// is defined with the rest of them, under "Division".
inline limb_t div128(limb_t hi, limb_t lo, limb_t d, limb_t* rem) noexcept;


// Limbs of scratch a product of an and bn limbs asks for, or zero when the
// product does not enter the tree at all - which is how mul_limbs below learns
// which of the two it is calling. A node of n limbs keeps its padded operands,
// its three half products and its two sums in the block, six n plus a little,
// and a depth first recursion has one such node live per level while the sizes
// halve - hence the geometric sum below, with the +1 because the sum of two
// halves is a limb longer than the halves.  The children are given the operands'
// real widths, never wider than the padded geometry counted here.
//
// A Toom-3 node is counted the same way, and comes out cheaper rather than
// dearer: its six product slots and two evaluation operands are fourteen limbs
// per third, just over five per limb of operand, against the six a Karatsuba node
// of the same width books.  So the loop below charges every level the Karatsuba
// figure whether or not that level splits three ways, and it walks the sizes down
// by half whether or not they do, which is the slower of the two shrinkings.  Both
// are deliberate overestimates: the bound has to hold for whatever mixture of the
// two shapes a recursion happens to turn into, and a trimmed child that drops
// below the Toom-3 entry continues in a tree that halves, not thirds, its width.
inline size_t mul_scratch_size(size_t an, size_t bn) {
    if (!use_karatsuba(an, bn)) {
        return 0; // the schoolbook needs nothing
    }
    size_t big = an > bn ? an : bn;
    size_t small = an > bn ? bn : an;
    size_t total = 0;
    while (small >= karatsuba_threshold) {
        const size_t n = big + (big & 1); // halves of a whole number of limbs
        total += 6 * n + 16;
        big = n / 2 + 1;
        small = big;
    }
    return total;
}

// r[0,an+bn) = a[0,an) * b[0,bn), overlap between the result and the inputs is
// handled by copying them first. One block of mul_scratch_size limbs serves the
// whole recursion.  Two operands that are the same array are a square: the tree
// below notices at every node, and the copy below is made once rather than twice.
inline void mul_limbs(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn) {
    if (an == 0 || bn == 0) {
        return;
    }
    const bool square = is_square(a, an, b, bn);
    const bool overlap = (a >= r && a < r + an + bn) || (b >= r && b < r + an + bn) ||
                         (r >= a && r < a + an) || (r >= b && r < b + bn);
    const size_t need = mul_scratch_size(an, bn);
    if (!overlap) {
        if (need == 0)
            square ? sqr_schoolbook(r, a, an) : mul_schoolbook(r, a, an, b, bn);
        else {
            const scratch block(need);
            mul_rec(r, a, an, b, bn, block.top());
        }
        return;
    }
    // Writing a product over one of its own factors (x *= y) is the common
    // case here, and the copies ride along in the same block.  A square written
    // over itself (x *= x) has one factor to copy, not two.
    const scratch block(square ? need + an : need + an + bn);
    mark m = block.top();
    limb_t* aa = m.take(an);
    limb_t* bb = square ? aa : m.take(bn);
    std::copy_n(a, an, aa);
    if (!square)
        std::copy_n(b, bn, bb);
    if (need == 0)
        square ? sqr_schoolbook(r, aa, an) : mul_schoolbook(r, aa, an, bb, bn);
    else
        mul_rec(r, aa, an, bb, bn, m.below());
}

// r[0,2n) = a[0,n)^2, spelled to the tree as one array held in both hands - which is
// how every node of it learns that the two factors are one.  The block of scratch a
// product of n by n books is the bound here too and is more than a square asks for:
// a node of a square keeps one padded copy rather than two, one sum rather than two,
// and one evaluation operand rather than two.
inline void sqr_limbs(limb_t* r, const limb_t* a, size_t n) { mul_limbs(r, a, n, a, n); }

inline void mul_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn,
                    mark m) {
    if (an < bn) {
        std::swap(an, bn);
        std::swap(a, b);
    }
    if (bn < karatsuba_threshold) {
        if (is_square(a, an, b, bn))
            sqr_schoolbook(r, a, an);
        else
            mul_schoolbook(r, a, an, b, bn);
        return;
    }
    if (use_toom3(an, bn)) {
        toom3_rec(r, a, an, b, bn, m);
        return;
    }
    // Karatsuba, both halves get the same (even limb count) size.  A square keeps
    // one padded copy of its operand and, further down, one sum of its halves: the
    // three children of a square are squares too, and each of them is given the same
    // array twice, which is what tells it so.
    const bool square = is_square(a, an, b, bn);
    size_t n = an;
    if (n & 1)
        ++n;
    const size_t h = n / 2;
    limb_t* aa = m.take(n);
    limb_t* bb = square ? aa : m.take(n);
    std::fill_n(aa, n, limb_t(0)); // the limbs above an are the zero padding
    if (!square)
        std::fill_n(bb, n, limb_t(0));
    std::copy_n(a, an, aa);
    if (!square)
        std::copy_n(b, bn, bb);

    // A half product is 2h limbs wide and the slot is 2(h + 1), because the
    // three of them are added and subtracted as whole numbers; the two limbs a
    // child leaves over are cleared here rather than by zeroing the slots
    // upfront, which is the pass this whole section is trying to avoid.
    const size_t pn = 2 * (h + 1);
    limb_t* al = m.take(pn);
    limb_t* ah = m.take(pn);
    limb_t* am = m.take(pn);

    // The three children get the operands' real widths instead of the padded
    // half width.  The shorter factor rarely reaches the top half of a node, so
    // a third of its products would be against zero limbs, and an upper half
    // that is empty altogether is worth skipping rather than timing.  A product
    // of a and b limbs is a + b limbs wide, which is what the slot below it has
    // to be cleared from.
    const size_t b0n = bn > h ? h : bn; // b0 = b mod B^h
    const size_t b1n = bn > h ? bn - h : 0;
    const size_t a1n = an - h; // an > h here, an being at least twice h at worst

    mul_rec(al, aa, h, bb, b0n, m.below()); // a0 * b0
    std::fill_n(al + h + b0n, pn - (h + b0n), limb_t(0));
    if (b1n != 0) {
        mul_rec(ah, aa + h, a1n, bb + h, b1n, m.below()); // a1 * b1
        std::fill_n(ah + a1n + b1n, pn - (a1n + b1n), limb_t(0));
    } else {
        std::fill_n(ah, pn, limb_t(0));
    }

    // The middle child multiplies the sum of the halves by itself for a square, so
    // one sum is built and handed over twice - the child then sees one array in both
    // hands and squares it.
    limb_t* s1 = m.take(h + 1);
    limb_t* s2 = square ? s1 : m.take(h + 1);
    s1[h] = add_limbs(s1, aa + h, aa, h);
    if (!square)
        s2[h] = add_limbs(s2, bb + h, bb, h);
    const size_t s1n = mag_size(s1, h + 1);
    const size_t s2n = square ? s1n : mag_size(s2, h + 1);
    if (s1n != 0 && s2n != 0) {
        mul_rec(am, s1, s1n, s2, s2n, m.below());
        std::fill_n(am + s1n + s2n, pn - (s1n + s2n), limb_t(0));
    } else {
        std::fill_n(am, pn, limb_t(0));
    }

    // The middle term is am - al - ah = a0*b1 + a1*b0, which is non negative.
    sub_limbs(am, am, al, pn);
    sub_limbs(am, am, ah, pn);

    // r = al + (am << h) + (ah << 2h). All terms are non negative and the sum
    // fits into an + bn limbs, so no transient overflow is possible.
    const size_t rn = an + bn;
    std::fill_n(r, rn, limb_t(0));
    const size_t aln = mag_size(al, pn);
    for (size_t i = 0; i < aln && i < rn; ++i)
        r[i] = al[i];

    const size_t amn = mag_size(am, pn);
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

    const size_t ahn = mag_size(ah, pn);
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
// Toom-Cook 3-way.
//
// With A(x) = a0 + a1 x + a2 x^2 and B(x) = b0 + b1 x + b2 x^2, taking x for
// base^h, the product is the polynomial C(x) = c0 + c1 x + c2 x^2 + c3 x^3 +
// c4 x^4 and the product of the two operands is C(base^h).  Six numbers pin C
// down: its values at 0, at 1, at -1, at 2, at -2 and its leading coefficient,
// and each of the six is one multiplication of a third of the width.
//
// Only the two negative points are awkward.  A(-1) = a0 - a1 + a2 can come out
// below zero, so each of those six values is a magnitude plus, for two of them,
// a sign.  The signs then cancel on their own: adding and subtracting the pair
// at 1 and -1 gives twice (c0 + c2 + c4) and twice (c1 + c3), and the pair at
// 2 and -2 gives four times (c1 + 4 c3), all three being sums of coefficients of
// a product of polynomials with non negative coefficients, hence magnitudes.
// Nothing here is ever negative, and the only division left is the exact one by
// three that separates c3 from c1.
// ---------------------------------------------------------------------------

// r[0,n) += a[0,an) << sh for a shift below a limb, the length of the sum coming
// back.  The regions must not overlap.  An interpolation shifts by one and two
// bits only, the powers of the base being already there as the offset of each
// third within the operand.
inline size_t add_shifted(limb_t* r, size_t n, const limb_t* a, size_t an, unsigned sh) {
    assert(an <= n && sh < limb_bits);
    limb_t carry = 0;
    limb_t spill = 0; // what limb i of the operand loses at the top is what limb
                      // i + 1 gains at the bottom, and the top limb of all of them
                      // lands above the operand rather than disappearing with it
    for (size_t i = 0; i < an; ++i) {
        const limb_t v = (a[i] << sh) | spill;
        spill = sh == 0 ? limb_t(0) : a[i] >> (limb_bits - sh);
        const limb_t s = r[i] + carry;
        const limb_t t = s + v;
        carry = limb_t(s < carry) + limb_t(t < s);
        r[i] = t;
    }
    for (size_t i = an; i < n && (carry != 0 || spill != 0); ++i) {
        const limb_t s = r[i] + carry;
        limb_t c = limb_t(s < carry);
        const limb_t t = s + spill;
        c += limb_t(t < s);
        r[i] = t;
        carry = c;
        spill = limb_t(0);
    }
    assert(carry == 0 && spill == 0); // the sum was meant to fit
    return mag_size(r, n);
}

// r[0,n) -= a[0,an) << sh, the borrow out of the top coming back, which is how a
// negative evaluation point learns that it is one.
inline limb_t sub_shifted(limb_t* r, size_t n, const limb_t* a, size_t an, unsigned sh) {
    assert(an <= n && sh < limb_bits);
    limb_t br = 0;
    limb_t spill = 0; // the same spill across the top of the operand as above
    for (size_t i = 0; i < an; ++i) {
        const limb_t v = (a[i] << sh) | spill;
        spill = sh == 0 ? limb_t(0) : a[i] >> (limb_bits - sh);
        const limb_t t = r[i] - br;
        br = limb_t(r[i] < br) + limb_t(t - v > t);
        r[i] = t - v;
    }
    for (size_t i = an; i < n && (br != 0 || spill != 0); ++i) {
        const limb_t t = r[i] - br;
        br = limb_t(r[i] < br) + limb_t(t - spill > t);
        r[i] = t - spill;
        spill = limb_t(0);
    }
    assert(spill == 0); // the operand was meant to fit under the top of r
    return br;
}

// r[0,n) = base^n - r[0,n), which is the magnitude of a value that came out of an
// evaluation below zero, the array being wide enough for it by construction.  Every
// limb is touched, not just the ones the carry reaches: the limbs above it are all
// ones to begin with - that is what makes the value negative - and a caller that
// measures the result by its top limb would count them otherwise.
inline void negate_limbs(limb_t* r, size_t n) {
    limb_t c = 1;
    for (size_t i = 0; i < n; ++i) {
        const limb_t v = ~r[i] + c;
        c = limb_t(c != 0 && v == 0);
        r[i] = v;
    }
}

// r[0,n) /= 3, the value being known to be a multiple of three.  The remainder
// never grows past two, so it crosses a limb boundary as a single limb and one
// division per limb is all the exactness costs.  The array is read from the top,
// so it can be the array the quotient goes back into.
inline void divexact_by3(limb_t* r, size_t n) {
    limb_t rem = 0;
    for (size_t i = n; i-- > 0;)
        r[i] = div128(rem, r[i], limb_t(3), &rem);
    assert(rem == 0);
}

// r[off,rn) += t[0,tn) shifted up by off limbs.  Every term is a magnitude and
// the sum of all of them is known to fit in rn limbs, so the carry has nowhere
// to overflow to.
inline void add_at(limb_t* r, size_t rn, size_t off, const limb_t* t, size_t tn) {
    limb_t carry = 0;
    size_t i = 0;
    for (; i < tn && off + i < rn; ++i) {
        const limb_t s = r[off + i] + t[i];
        const limb_t c1 = limb_t(s < t[i]);
        const limb_t v = s + carry;
        r[off + i] = v;
        carry = c1 + limb_t(v < s);
    }
    for (size_t k = off + i; k < rn && carry != 0; ++k) {
        const limb_t v = r[k] + carry;
        carry = limb_t(v < carry);
        r[k] = v;
    }
    assert(carry == 0 && (i == tn || mag_size(t + i, tn - i) == 0));
}

// r[0,an+bn) = a[0,an) * b[0,bn) the three way way, an >= bn and both at least as
// wide as toom3_entry, the shorter one reaching toom3_min_pc percent of the longer.
inline void toom3_rec(limb_t* r, const limb_t* a, size_t an, const limb_t* b, size_t bn,
                      mark m) {
    const size_t h = (an + 2) / 3; // three of these hold the longer operand
    const size_t te = h + 2;       // an evaluation point of one operand, 7 * base^h
                                   // at the widest
    const size_t pn = 2 * h + 2;   // a product of two of them, 49 * base^2h at the
                                   // widest, and what a child writes down regardless

    // The three thirds of each operand, by pointer and by real length: an empty
    // third is a null pointer with no limbs, so a product against one is skipped
    // rather than multiplied out.
    const size_t a0n = an > h ? h : an;
    const size_t a1n = an > h ? (an > 2 * h ? h : an - h) : 0;
    const size_t a2n = an > 2 * h ? an - 2 * h : 0;
    const size_t b0n = bn > h ? h : bn;
    const size_t b1n = bn > h ? (bn > 2 * h ? h : bn - h) : 0;
    const size_t b2n = bn > 2 * h ? bn - 2 * h : 0;
    const limb_t* const a0 = a;
    const limb_t* const a1 = a1n != 0 ? a + h : nullptr;
    const limb_t* const a2 = a2n != 0 ? a + 2 * h : nullptr;
    const limb_t* const b0 = b;
    const limb_t* const b1 = b1n != 0 ? b + h : nullptr;
    const limb_t* const b2 = b2n != 0 ? b + 2 * h : nullptr;

    limb_t* w0 = m.take(pn);  // C(0)   = c0
    limb_t* wf = m.take(pn);  // C(inf) = c4
    limb_t* w1 = m.take(pn);  // C(1)
    limb_t* wm1 = m.take(pn); // |C(-1)|, the one value that can be negative
    limb_t* w2 = m.take(pn);  // C(2)
    limb_t* t = m.take(pn);   // the first combination, which needs two of the
                              // values and somewhere to put the third all at once
    // A square needs one evaluation operand, not two: every point of the product is
    // the square of one point of the operand, and the child that multiplies it is
    // handed the same array twice, which is what tells it to square.
    const bool square = is_square(a, an, b, bn);
    limb_t* ea = m.take(te);
    limb_t* eb = square ? ea : m.take(te);

    // The value of one operand at one of the three finite points, told by two
    // bits: the low one subtracts the middle third, the high one doubles it and
    // quadruples the last.  Whether the result came out negative is what comes
    // back, the array holding its magnitude either way - and only a point with the
    // low bit set can be, the others being sums of non negative thirds.
    const auto eval = [&](limb_t* out, const limb_t* p0, size_t n0, const limb_t* p1,
                          size_t n1, const limb_t* p2, size_t n2,
                          unsigned at) -> bool {
        const unsigned sh = (at & 2u) != 0 ? 1u : 0u;
        std::fill_n(out, te, limb_t(0));
        add_shifted(out, te, p0, n0, 0);
        add_shifted(out, te, p2, n2, 2 * sh);
        if ((at & 1u) == 0) {
            add_shifted(out, te, p1, n1, sh);
            return false;
        }
        if (sub_shifted(out, te, p1, n1, sh) != 0) {
            negate_limbs(out, te);
            return true;
        }
        return false;
    };

    // One point of the product polynomial: the two operands' values at it are
    // built in the two evaluation slots, which the child leaves alone, and the
    // child itself is given the real widths, so that a third which happens to be
    // small does not pay for the padding around it.
    const auto point = [&](limb_t* slot, const limb_t* x, size_t xn, const limb_t* y,
                           size_t yn) {
        if (xn == 0 || yn == 0) {
            std::fill_n(slot, pn, limb_t(0));
            return;
        }
        assert(xn + yn <= pn); // the child writes that many limbs, the slot holds
                               // the rest of them as zeroes
        mul_rec(slot, x, xn, y, yn, m.below());
        std::fill_n(slot + xn + yn, pn - (xn + yn), limb_t(0));
    };

    point(w0, a0, a0n, b0, b0n);
    point(wf, a2, a2n, b2, b2n);

    eval(ea, a0, a0n, a1, a1n, a2, a2n, 0);
    if (!square)
        eval(eb, b0, b0n, b1, b1n, b2, b2n, 0);
    point(w1, ea, mag_size(ea, te), eb, mag_size(eb, te));

    // A product is negative when exactly one factor is.  A square is its factor
    // twice, so the value of the product at minus one is A(-1) squared and is a
    // magnitude however that A(-1) came out - which is all the flag below does, and
    // for a square it does nothing.  The magnitude eval leaves behind is the one the
    // interpolation wants either way.
    const bool aneg = eval(ea, a0, a0n, a1, a1n, a2, a2n, 1) != 0;
    const bool bneg = square ? aneg : eval(eb, b0, b0n, b1, b1n, b2, b2n, 1) != 0;
    const bool s = aneg != bneg;
    point(wm1, ea, mag_size(ea, te), eb, mag_size(eb, te));

    eval(ea, a0, a0n, a1, a1n, a2, a2n, 2);
    if (!square)
        eval(eb, b0, b0n, b1, b1n, b2, b2n, 2);
    point(w2, ea, mag_size(ea, te), eb, mag_size(eb, te));

    // The interpolation, all of it in place and all of it limb local, so one of
    // the two inputs is allowed to be the result.  The three values above the two
    // trivial ones combine into three magnitudes:
    //
    //   (C(2) - C(-1)) / 3 = c1 + c2 + 3 c3 + 5 c4
    //   (C(1) - C(-1)) / 2 = c1 + c3
    //   C(1) - c0           = c1 + c2 + c3 + c4
    //
    // The sum of the two values at one and minus one, which is the other thing
    // those two values are good for, would have given c0 + c2 + c4 and so a c2 of
    // its own, but it would have left 5 c4 to take off the first line - and five
    // of something costs a multiply, where two of it cost a shift.  Subtracting
    // the middle line from the first and then the third is what leaves 2 c3 + 4
    // c4, and halving that once rather than twice is what makes the two shifts
    // below enough: c3 + 2 c4, from which one shifted c4 falls off.
    //
    // None of the subtractions borrows, |A(-1)| never reaching either A(1) or A(2)
    // and every coefficient being non negative.  A negative C(-1) turns every
    // difference into a sum and every sum into a difference, which is all the flag
    // does; the division by three is exact and the difference by two even.
    if (s)
        add_limbs(w2, w2, wm1, pn);
    else
        sub_limbs(w2, w2, wm1, pn);
    divexact_by3(w2, pn); // c1 + c2 + 3 c3 + 5 c4

    if (s)
        add_limbs(t, w1, wm1, pn);
    else
        sub_limbs(t, w1, wm1, pn);
    rshift(t, t, pn, 1); // c1 + c3, and wm1 is free from here

    sub_limbs(w1, w1, w0, pn); // c1 + c2 + c3 + c4
    sub_limbs(w2, w2, w1, pn); // 2 c3 + 4 c4
    rshift(w2, w2, pn, 1);      // c3 + 2 c4
    // The borrow out of that subtraction is the proof that the c4 being taken off
    // was there to take off; it says nothing the release build acts upon.
    [[maybe_unused]] const limb_t back = sub_shifted(w2, pn, wf, mag_size(wf, pn), 1); // c3
    assert(back == 0);

    sub_limbs(w1, w1, t, pn); // c2 + c4
    sub_limbs(w1, w1, wf, pn); // c2

    sub_limbs(t, t, w2, pn); // c1 = (c1 + c3) - c3

    const size_t rn = an + bn;
    std::fill_n(r, rn, limb_t(0));
    add_at(r, rn, 0, w0, mag_size(w0, pn));
    add_at(r, rn, h, t, mag_size(t, pn));
    add_at(r, rn, 2 * h, w1, mag_size(w1, pn));
    add_at(r, rn, 3 * h, w2, mag_size(w2, pn));
    add_at(r, rn, 4 * h, wf, mag_size(wf, pn));
}

// ---------------------------------------------------------------------------
// Division.
// ---------------------------------------------------------------------------

// (hi : lo) / d as a single limb, with the remainder stored through rem when
// one is wanted. The caller has to keep hi below d, which is precisely what
// normalising the divisor buys for every quotient digit of the kernel below.
inline limb_t div128(limb_t hi, limb_t lo, limb_t d, limb_t* rem) noexcept {
    assert(hi < d && d != 0);
#if defined(ALT_HAVE_U128)
#if defined(__GNUC__) || defined(__clang__)
    // Telling the compiler what the precondition already states is what makes it
    // emit a single division instruction instead of a library call for a wide
    // quotient it cannot prove fits in a limb.
    if (hi >= d)
        __builtin_unreachable();
#endif
    const u128_t cur = (u128_t(hi) << 64) | u128_t(lo);
    const limb_t q = limb_t(cur / u128_t(d));
    if (rem != nullptr)
        *rem = limb_t(cur - u128_t(q) * u128_t(d)); // no second division
    return q;
#elif defined(ALT_HAVE_MSVC_INTRINSICS)
    limb_t r = 0;
    const limb_t q = _udiv128(hi, lo, d, &r);
    if (rem != nullptr)
        *rem = r;
    return q;
#else
    // Plain restoring division, for a target with neither a wide integer type
    // nor a wide division intrinsic: let one bit of the dividend in at a time
    // and take the divisor away as soon as the remainder reaches it. Because the
    // remainder stays below d, one shifted in bit can push it past a limb by at
    // most one place, so a single carry bit is all the extra room needed.
    limb_t q = 0, r = 0;
    for (unsigned bit = 2 * limb_bits; bit-- > 0;) {
        const limb_t source = bit >= limb_bits ? hi : lo;
        const limb_t top = r >> (limb_bits - 1);
        r = (r << 1) | ((source >> (bit % limb_bits)) & limb_t(1));
        limb_t taken = 0;
        if (top != 0 || r >= d) {
            r -= d;
            taken = 1;
        }
        q = (q << 1) | taken;
    }
    if (rem != nullptr)
        *rem = r;
    return q;
#endif
}

// q[0,un) = u[0,un) / d, the remainder is returned. d != 0, q may be null, u
// itself is left alone.
inline limb_t divrem_limb(limb_t* q, const limb_t* u, size_t un, limb_t d) {
    assert(d != 0);
    const size_t n = mag_size(u, un);
    if (n == 0) {
        if (q != nullptr)
            std::fill_n(q, un, limb_t(0));
        return 0;
    }
    limb_t r = 0;
    for (size_t i = n; i-- > 0;) {
        // The remainder of the step above is below d, so each step is a legal
        // 128/64 division and the quotient limb cannot overflow.
        const limb_t qq = div128(r, u[i], d, &r);
        if (q != nullptr)
            q[i] = qq;
    }
    if (q != nullptr && n < un)
        std::fill_n(q + n, un - n, limb_t(0));
    return r;
}

// Remainder of u[0,un) divided by a single limb.
inline limb_t mod_limb(const limb_t* u, size_t un, limb_t d) {
    return divrem_limb(nullptr, u, un, d);
}

// Limbs the kernel below asks for: the normalised copies of both operands plus
// the remainder it fills before moving it into place.  The seed of the reciprocal
// further down is a division of this shape, which is why the figure is its own
// function rather than arithmetic inside the kernel.
inline size_t knuth_scratch_size(size_t un, size_t dn) { return un + 2 * dn + 2; }

// Knuth algorithm D working on whole limbs: the estimate of a quotient digit
// costs one 128/64 division instead of the two 64/32 ones a half limb digit
// needs, and the multiply subtract that follows walks limbs rather than packed
// 32 bit digits, so a quotient of n limbs costs about half of what it used to.
// dn >= 2 (the second most significant limb of the divisor takes part in the
// estimate) and u >= d are required; the divisor is normalised here, so it only
// has to be free of leading zero limbs, and the remainder comes back
// denormalised. q[0,un-dn+1) and rem[0,dn) are written, u is not touched.
inline void knuth_d64(limb_t* q, limb_t* rem, const limb_t* u, size_t un,
                      const limb_t* d, size_t dn, mark m) {
    assert(dn >= 2 && un >= dn && d[dn - 1] != 0);
    const unsigned s = clz64(d[dn - 1]);
    limb_t* V = m.take(dn);
    limb_t* U = m.take(un + 1);
    if (s != 0)
        lshift(d, dn, V, s);
    else
        std::copy_n(d, dn, V);
    std::copy_n(u, un, U);
    U[un] = 0;
    if (s != 0)
        U[un] = lshift(U, un, U, s);

    const limb_t v1 = V[dn - 1];
    const limb_t v0 = V[dn - 2];
    for (size_t j = un - dn + 1; j-- > 0;) {
        // D1 and D2: the estimate, kept wide, since the top limb of the window
        // can be as large as the top limb of the divisor.
        limb_t qh, rhi, rl;
        if (U[j + dn] >= v1) {
            qh = limb_max;
            // (base - 1) * v1 is (v1 - 1 : 0 - v1), all the subtraction below
            // needs to know.
            const limb_t plo = limb_t(0) - v1;
            rl = U[j + dn - 1] - plo;
            rhi = U[j + dn] - (v1 - 1) - limb_t(U[j + dn - 1] < plo);
        } else {
            qh = div128(U[j + dn], U[j + dn - 1], v1, &rl);
            rhi = 0;
        }
        // The estimate never looked at v0, so it is too large as soon as
        // qh * v0 exceeds the window it did not see. Two rounds at most take it
        // back (Knuth 4.3.1, exercise 21), and a remainder that grew past a
        // whole limb settles the question by itself.
        while (rhi == 0) {
            const wide64 p = mul_wide(qh, v0);
            if (p.hi < rl || (p.hi == rl && p.lo <= U[j + dn - 2]))
                break;
            qh -= 1;
            const limb_t sum = rl + v1;
            rhi += limb_t(sum < rl);
            rl = sum;
        }
        // D3: take qh * V away from the window. What is borrowed out is a limb
        // plus at most one, since the product of two limbs leaves exactly that
        // much room.
        limb_t blo = 0, bhi = 0;
        for (size_t i = 0; i < dn; ++i) {
            const wide64 p = mul_wide(qh, V[i]);
            const limb_t x = U[j + i];
            const limb_t s1 = x - blo;
            const limb_t s2 = s1 - p.lo;
            U[j + i] = s2;
            const limb_t b1 = p.hi + limb_t(x < blo);
            const limb_t b2 = b1 + limb_t(s1 < p.lo);
            bhi = limb_t(b2 < b1);
            blo = b2;
        }
        limb_t top = U[j + dn];
        const bool too_much = bhi != 0 || top < blo;
        top -= blo;
        top -= bhi;
        if (too_much) {
            // D4 and D5: the estimate was one too large, the divisor goes back
            // and the quotient digit comes down with it.
            qh -= 1;
            limb_t carry = 0;
            for (size_t i = 0; i < dn; ++i) {
                const limb_t a = U[j + i];
                const limb_t a1 = a + V[i];
                const limb_t c1 = limb_t(a1 < a);
                const limb_t a2 = a1 + carry;
                const limb_t c2 = limb_t(a2 < a1);
                U[j + i] = a2;
                carry = c1 + c2;
            }
            top += carry;
        }
        U[j + dn] = top;
        q[j] = qh;
    }

    // D6: undo the normalisation of the remainder, which by now sits in the
    // low dn limbs with nothing above it.
    if (s != 0)
        rshift(rem, U, dn, s);
    else
        std::copy_n(U, dn, rem);
}

// ---------------------------------------------------------------------------
// Division through a reciprocal.
//
// Knuth's algorithm D costs one multiply subtract of the whole divisor per
// quotient limb, so a quotient of n limbs over a divisor of n limbs is a
// quadratic thing no matter how fast a single product is.  Everything from here
// on buys its quotient limbs in blocks instead: with the reciprocal of the
// divisor in hand, a block of limbs of the quotient is one product of the top of
// the partial remainder by the top of that reciprocal, and n limbs of quotient
// then cost a handful of whole multiplications - which is to say the same
// n^1.585 or n^1.465 the products themselves cost.
//
// The reciprocal of a divisor v of n limbs is written as base^n + pi with n limbs
// of pi, which is what the top limb of the reciprocal always being one asks for:
// v is normalised to at least half of base^n, so the reciprocal is between
// base^n and twice that.  With that, a block of in quotient limbs is
//
//     Q = floor(Rhi * pi_top / base^in) + Rhi,   Rhi = the top in limbs of R
//
// where pi_top is the top in limbs of pi and R the partial remainder, and it is
// worth a proof rather than a reference: writing rho = base^n + pi for the
// reciprocal, the ideal block is R * base^in / v = R * rho * base^in / base^{2n}
// up to the factor v * rho / base^{2n}, which is one to within a unit of v.  The
// two terms above are that same product cut in two: pi = rho - base^n =
// base^n * (base^n - v) / v, so
//
//     Rhi * pi_top / base^in  ~  R * (rho - base^n) / base^n = R * base^in / v * (1 - v / base^n)
//     Rhi                     ~  R * base^in / v * (v / base^n)
//
// and the two halves add up to the ideal block, each truncation costing a little
// under one limb of it.  What the estimate cannot see it makes up for afterwards:
// the block is multiplied back against the divisor, taken away, and what is left
// is compared - a remainder that came out below zero or at or above the divisor
// says the estimate was wrong by a limb and puts that limb right.  The comparisons
// are what make the answer correct; the accuracy of the reciprocal only decides
// how often they have anything to do, which is why the ladder below is content
// with being a couple of units off.
// ---------------------------------------------------------------------------

// The three sizes the path above needs to be worth its overhead.  A block of the
// kernel does two products - the partial remainder against the reciprocal, and the
// block back against the divisor - where a step of the Knuth kernel does one, so the
// reciprocal can only pay where a product has stopped being quadratic: that is a
// property of the divisor's width.  And the reciprocal itself has to be earned back,
// which is a property of how many limbs of quotient there are to sell.  Both are
// build time knobs, and all three numbers come from build/divkern.cpp, which times
// the two kernels against each other on the same operands:
//
//   limbs of   recip/knuth at the longest quotient measured     the reciprocal alone,
//   divisor      (32 limbs of quotient per limb of divisor)      same divisor, ns
//     128                       1.15                              36 000
//     256                       0.99                             122 000
//     512                       0.75                             420 000
//    1024                       0.54                           1 340 000
//    2048                       0.36                           4 070 000
//
// The reciprocal grows by about three and a third per doubling of the divisor - two
// schoolbook products a round, the deficit needing the whole divisor - and below five
// hundred and twelve limbs of divisor (thirty two thousand bits) no quotient makes the
// path worth taking, whatever the other two bounds say.  Above it the quotient has to
// be a couple of thousand limbs long before the ladder is paid for; measured at the
// shapes the three bounds below first admit: 1.05 at 512 by 2048, 0.75 at 512 by 8192,
// 0.82 at 1024 by 4096, 0.55 at 2048 by 8192, 0.36 at 2048 by 65536.  The one shape
// that comes out even is the corner itself, which is what a threshold sitting on a
// break-even point does; nothing measured beyond it loses.
#ifndef AXINT_DIVREC_MIN_DN
#define AXINT_DIVREC_MIN_DN 512
#endif
#ifndef AXINT_DIVREC_MIN_QN
#define AXINT_DIVREC_MIN_QN 2048
#endif
#ifndef AXINT_DIVREC_MIN_RATIO
#define AXINT_DIVREC_MIN_RATIO 4
#endif

inline constexpr size_t divrec_min_dn = AXINT_DIVREC_MIN_DN;
inline constexpr size_t divrec_min_qn = AXINT_DIVREC_MIN_QN;
inline constexpr size_t divrec_min_ratio = AXINT_DIVREC_MIN_RATIO;

// Whether u[0,un) divided by dn limbs goes through the reciprocal.  Defining
// AXINT_NO_DIVREC before this header keeps every division on the Knuth kernel,
// which is how the two are measured against each other.
constexpr bool use_divrec(size_t un, size_t dn) noexcept {
#if defined(AXINT_NO_DIVREC)
    static_cast<void>(un);
    static_cast<void>(dn);
    return false;
#else
    return dn >= divrec_min_dn && un >= dn && un - dn + 1 >= divrec_min_qn &&
           un - dn + 1 >= divrec_min_ratio * dn;
#endif
}

// Limbs of scratch the ladder below asks for: the two estimates it alternates
// between, the product the deficit is read out of, the deficit itself, and the
// product that turns the deficit into the new low limbs.
// The seed is a division of three limbs by two, which costs knuth_scratch_size(3, 2)
// on top of the ladder's own five buffers, so the figure below has room for that
// whether or not it is also enough for the ladder alone.
inline size_t recip_scratch_size(size_t n) { return 9 * n + 40; }

// pi[0,n) = floor((base^(2n) - 1) / v) - base^n for a divisor v of n limbs whose
// top bit is set.  The value comes out a couple of units below the exact one at
// worst, the ladder trading exactness for the products it does not do; the kernel
// above takes that difference back with its comparisons.
inline void recip_of(limb_t* pi, const limb_t* v, size_t n, mark m) {
    assert(n >= 2 && (v[n - 1] >> (limb_bits - 1)) != 0);

    limb_t* a = m.take(n + 2);
    limb_t* b = m.take(n + 2);
    limb_t* pp = m.take(2 * n + 2);
    limb_t* dd = m.take(2 * n + 4);
    limb_t* num = m.take(3 * n + 4);

    // Half of base^n exactly is the one divisor whose reciprocal reaches 2 *
    // base^n rather than stopping under it, and it is worth naming rather than
    // letting the ladder overshoot a limb it has nowhere to put.
    bool half = v[n - 1] == (limb_t(1) << (limb_bits - 1));
    for (size_t i = 0; half && i + 1 < n; ++i)
        half = v[i] == 0;
    if (half) {
        std::fill_n(pi, n, limb_max);
        return;
    }

    // The seed is two limbs of the reciprocal, taken from the exact reciprocal of the
    // divisor's top two limbs.  Those two stand for a divisor of their own, whose
    // reciprocal is three limbs wide, so the division is base^4 - 1 by two limbs and
    // of the three limbs it leaves the lowest is noise: the estimate the ladder wants
    // is the pair above it.  Two limbs of divisor buy two limbs of reciprocal and not
    // a third, and keeping a digit that is not yet right is what the doubling would
    // have to square away again.
    limb_t seed[4] = {limb_max, limb_max, limb_max, limb_max}; // base^4 - 1
    limb_t two[2];                            // the top two limbs, as a divisor
    const size_t seed_room = knuth_scratch_size(4, 2);
    limb_t* srem = m.take(2);
    limb_t* sq = m.take(3);
    limb_t* sknuth = m.take(seed_room);
    two[0] = v[n - 2];
    two[1] = v[n - 1];
    knuth_d64(sq, srem, seed, 4, two, 2, mark{sknuth, seed_room});
    a[0] = sq[1];
    a[1] = sq[2];
    size_t xm = 2; // limbs of the reciprocal held so far

    // One round of the ladder.  With xm limbs held, x stands for the top xm digits
    // of the reciprocal, which is to say it approximates base^(n + xm - 1) / v, and
    // the divisor times it falls short of base^(n + xm - 1) by the deficit D - a
    // value under v while x is the floor of that quotient.  The next precision wants
    // base^(n + nm - 1) / v, which is base^sh times the same quotient, so
    //
    //     x' = x * base^sh + base^sh * D / v
    //
    // the old digits moving up by sh and the new term, which is under base^sh,
    // landing in the limbs they vacated.  The division by v is replaced by a
    // multiplication with x itself, v * x being base^(n + xm - 1) to within D, which
    // makes the new term floor(D * x / base^(n + 2 xm - nm - 1)) - one or two limbs
    // wide, read off that product at the limb the division leaves below it.
    //
    // What keeps a round from being exact is the multiplication standing in for the
    // division by v.  Writing d for how far x falls short of base^(n + xm - 1) / v,
    // the deficit is d * v wide and the new term comes out short by that term's own
    // share of it, which works out at
    //
    //     d' <= d * d * base^(sh + 1 - xm) + 1
    //
    // the square being the Newton part and the one the term's own truncation.  So a
    // round may only reach sh = xm - 2 limbs beyond what it holds, for then the factor
    // is 1 / base and a shortfall of a couple of units squares into a fraction of one:
    // the digits then stay right to a unit or two all the way up, which is what the
    // kernel above asks for - a block estimate reads a reciprocal that is off by e as
    // a quotient off by e, so units there are fix-ups, and limbs of them are not.
    // Reaching a whole xm of new limbs each round, as the doubling suggests, leaves a
    // tenth of a limb of error behind at every step and never gets below one.
    //
    // The seed is the one estimate that can be over rather than under, the divisor it
    // inverts being two limbs rather than n, which is why the sign of the difference
    // below is carried along rather than assumed - from that first round on the error
    // is a square, and a square is under.
    while (xm < n + 1) {
        const size_t reach = xm > 2 ? xm - 2 : 1;            // limbs this round adds
        const size_t nm = xm + reach <= n + 1 ? xm + reach : n + 1; // limbs reached
        const size_t dpos = n + xm - 1; // the limb base^(n + xm - 1) puts its one in
        const size_t sh = nm - xm;

        // The whole divisor, rather than the top xm + 1 limbs of it.  The deficit is
        // read at the top of this product, and a limb of v left out anywhere below
        // that top is a whole limb of error in the deficit - the one quantity the
        // doubling exists to square away, so truncating here would cost the ladder its
        // convergence rather than a little time.  Doing it anyway needs a product that
        // wraps the missing part around into the top, which is a measurement of its
        // own and is noted as such in the roadmap.
        mul_limbs(pp, v, n, a, xm); // n + xm limbs, the top one at limb dpos
        bool over;
        size_t dlen;
        if (pp[dpos] == 0) {
            over = false; // the estimate is under the exact reciprocal, as a rule
            limb_t br = 0;
            for (size_t i = 0; i <= dpos; ++i) { // base^dpos minus the product
                const limb_t top = i == dpos ? limb_t(1) : limb_t(0);
                const limb_t t = top - br;
                dd[i] = t - pp[i];
                br = limb_t(top < br) + limb_t(t - pp[i] > t);
            }
            [[maybe_unused]] const limb_t done = br;
            assert(done == 0); // the one was there to take away
            dlen = mag_size(dd, dpos + 1);
        } else {
            over = true; // the same difference, the other way round
            std::copy_n(pp, dpos + 1, dd);
            [[maybe_unused]] const limb_t gone = sub_limb(dd, dd, dpos + 1);
            assert(gone == 0); // the product does reach the limb the one sits in
            dlen = mag_size(dd, dpos + 1);
        }

        std::fill_n(b, nm + 1, limb_t(0));
        std::copy_n(a, xm, b + sh); // the old digits, moved up to their new place
        if (dlen != 0) {
            mul_limbs(num, a, xm, dd, dlen);
            const size_t read = n + 2 * xm - nm - 1; // the division by v, as a position
            const size_t keep = xm + dlen > read ? xm + dlen - read : 0;
            if (!over) {
                add_at(b, nm + 1, 0, num + read, keep);
            } else {
                // And back down for an estimate that read over.  What is taken off
                // is smaller than the digits it stands on, the deficit of an
                // estimate that far over being smaller than the reciprocal is large.
                [[maybe_unused]] const limb_t br = sub_wide(b, b, nm + 1, num + read, keep);
                assert(br == 0);
            }
        }
        xm = nm;
        std::swap(a, b);
    }

    // a now holds the reciprocal itself, base^n plus the n limbs wanted here.
    if (a[n] != 1)
        std::fill_n(pi, n, limb_max); // only reachable for the half base^n divisor
    else
        std::copy_n(a, n, pi);
}

// q[0,un-dn+2) = u[0,un) / d and rem[0,dn) the remainder, in blocks of quotient
// limbs through the reciprocal of d.  The same contract as the kernel above: dn >=
// 2, u >= d, d free of leading zero limbs, the divisor normalised here and the
// remainder coming back denormalised, u not touched.  False means the reciprocal
// turned out worse than the ladder is meant to make it and nothing was decided -
// the quotient slots are then whatever the rounds wrote, so the caller is expected to
// do the division over again the long way, which it can, the scratch it handed in
// being the only thing this takes.
inline bool divrec_multi(limb_t* q, limb_t* rem, const limb_t* u, size_t un,
                         const limb_t* d, size_t dn, mark m) {
    assert(dn >= 2 && un >= dn && d[dn - 1] != 0);
    const unsigned s = clz64(d[dn - 1]);

    limb_t* V = m.take(dn);
    limb_t* U = m.take(un + 1);
    limb_t* R = m.take(dn);
    limb_t* pi = m.take(dn);
    limb_t* Qb = m.take(dn + 1);
    limb_t* T = m.take(2 * dn + 2);
    limb_t* Pp = m.take(2 * dn + 2);

    if (s != 0)
        lshift(d, dn, V, s);
    else
        std::copy_n(d, dn, V);
    std::copy_n(u, un, U);
    U[un] = s != 0 ? lshift(U, un, U, s) : limb_t(0);

    recip_of(pi, V, dn, m);

    // The normalised dividend is one limb longer than the one handed in, that limb
    // holding the bits the shift pushed out.  Counting it as a limb of its own buys
    // a quotient digit which is very often zero and never needs deciding twice: the
    // top digit of a division is a comparison, the divisor being normalised.
    const size_t un2 = un + 1;
    std::copy_n(U + (un2 - dn), dn, R);
    limb_t top = 0;
    if (cmp_wide(R, dn, V, dn) >= 0) {
        sub_limbs(R, R, V, dn);
        top = 1;
    }
    if (q != nullptr)
        q[un2 - dn] = top;

    // Each round holds the invariant that what has been subtracted from the
    // dividend leaves R the partial remainder, less than the divisor, with the
    // limbs below it still untouched in U.  Bringing in in of them and multiplying
    // R up is the same long division the kernel above does a limb at a time.
    for (size_t off = un2 - dn; off != 0;) {
        const size_t in = off < dn ? off : dn;
        const limb_t* Rhi = R + (dn - in);

        mul_limbs(T, Rhi, in, pi + (dn - in), in); // the low half of this is thrown away
        Qb[in] = add_limbs(Qb, T + in, Rhi, in);   // the implicit top limb of the reciprocal

        mul_limbs(Pp, Qb, in + 1, V, dn);

        std::copy_n(U + (off - in), in, T); // the limbs coming down
        std::copy_n(R, dn, T + in);         // the partial remainder, moved up
        T[in + dn] = 0;
        limb_t over = sub_wide(T, T, in + dn + 1, Pp, in + dn + 1);

        // The estimate saw neither the limbs below Rhi nor the low limbs of the
        // reciprocal, and the reciprocal is itself a little short, so what came out
        // is usually the true remainder and occasionally one or two divisors short
        // or too much.  Both are put right here, and how often they are is the only
        // thing the accuracy of the reciprocal decides.
        size_t fixes = 0;
        for (;;) {
            if (over != 0) {
                // The block was too big: the divisor goes back under the window and
                // the quotient comes down by the limb that was put over it.
                const limb_t c1 = add_limbs(T, T, V, dn);
                const limb_t c2 = c1 != 0 ? add_limb(T + dn, T + dn, in + 1, c1) : limb_t(0);
                assert(c2 == 0); // one divisor is more than the difference was short of
                static_cast<void>(c2);
                sub_limb(Qb, Qb, in + 1);
            } else {
                const bool big = mag_size(T + dn, in + 1) != 0 || cmp_wide(T, dn, V, dn) >= 0;
                if (!big)
                    break;
                const limb_t b1 = sub_limbs(T, T, V, dn);
                if (b1 != 0)
                    sub_limb(T + dn, T + dn, in + 1);
                add_limb(Qb, Qb, in + 1, limb_t(1));
            }
            // A reciprocal good to a unit or two of the exact one can only be this far
            // wrong about a block; past that the estimate is not the ladder's doing and
            // the rounds are made of, since every pass here moves by one limb.
            if (++fixes > 4)
                return false;
        }

        if (q != nullptr) {
            std::copy_n(Qb, in, q + (off - in));
            if (Qb[in] != 0) // the block ran past its own width, the overhang belongs
                add_limb(q + off, q + off, un - dn + 2 - off, Qb[in]); // to the digit above
        }
        assert(mag_size(T + dn, in + 1) == 0); // R is the remainder, and nothing above it
        std::copy_n(T, dn, R);
        off -= in;
    }

    // The divisor was normalised, so the remainder comes back shifted the other way.
    if (s != 0)
        rshift(rem, R, dn, s);
    else
        std::copy_n(R, dn, rem);
    return true;
}

// Limbs the reciprocal kernel asks for out of the mark it is handed: the normalised
// copies of both operands, the partial remainder, the reciprocal, one block of
// quotient and two product slots, and the ladder's own block - which is what makes it
// dearer than the Knuth kernel of a small division, and why the dispatch below looks
// at the room on offer as well as at the sizes.
inline size_t divrec_kernel_room(size_t un, size_t dn) {
    return un + 9 * dn + 8 + recip_scratch_size(dn);
}

// What a division of this shape needs as a whole block, the remainder slot included -
// the convention the Knuth figure is written in, since both are read by the callers
// that size one block for the dispatch below, which takes that slot for itself before
// it hands anything to a kernel.  Reading the two figures as if they meant the same
// thing costs nothing loud: the dispatch, which trusts the room it is offered, then
// finds it never quite enough and quietly keeps every division on the Knuth kernel.
// That is how this kernel came to be measured two and a half times faster than the
// other one while not being reached through the class interface at all.
inline size_t divrec_room(size_t un, size_t dn) {
    return dn + divrec_kernel_room(un, dn);
}

// Limbs the two kernels ask for: the Knuth one wants the normalised copies of both
// operands plus the remainder it fills before moving it into place, the reciprocal
// one the above on top of that.  Callers size one block from this and both paths fit
// into it; a caller that budgets less is not left short, it simply gets the Knuth
// kernel, which is what it counted on.
inline size_t divrem_scratch_size(size_t un, size_t dn) {
    const size_t plain = knuth_scratch_size(un, dn);
    if (!use_divrec(un, dn))
        return plain; // the kernel that is going to run is the one that needs less
    const size_t through_reciprocal = divrec_room(un, dn);
    return plain > through_reciprocal ? plain : through_reciprocal;
}

// u[0,un) is replaced by the remainder, q (un - dn + 2 limbs, may be null)
// receives the quotient. Requires d non zero and free of leading zero limbs;
// work has to offer divrem_scratch_size(un, dn) limbs.
inline void divrem_multi(limb_t* q, limb_t* u, size_t un, const limb_t* d, size_t dn,
                         mark work) {
    assert(dn >= 1 && un >= dn && d[dn - 1] != 0);
    const size_t qn = un - dn + 2;
    const size_t un_norm = mag_size(u, un);
    if (cmp_wide(u, un_norm, d, dn) < 0) {
        if (q != nullptr)
            std::fill_n(q, qn, limb_t(0));
        std::fill_n(u + un_norm, un - un_norm, limb_t(0));
        return;
    }
    if (dn == 1) { // the kernel below needs two limbs in the divisor
        const limb_t r = divrem_limb(q, u, un, d[0]);
        if (q != nullptr)
            q[un] = 0; // one limb divisors leave the top quotient slot alone
        u[0] = r;
        if (un > 1)
            std::fill_n(u + 1, un - 1, limb_t(0));
        return;
    }
    limb_t* rem = work.take(dn);
    // The region past the remainder slot, kept aside: the reciprocal kernel only
    // carves scratch out of it, so a division it gives up on can be done over by the
    // kernel that needs less room, from the same block.
    const mark plain = work;
    // Room is part of the decision, not an assumption: the loops that keep their own
    // working block (a Lehmer step, a half-gcd round) counted the Knuth kernel's
    // appetite, and a division that small would not have wanted the reciprocal anyway.
    const bool wanted =
        use_divrec(un_norm, dn) && work.left >= divrec_kernel_room(un_norm, dn);
    if (!wanted || !divrec_multi(q, rem, u, un_norm, d, dn, work))
        knuth_d64(q, rem, u, un_norm, d, dn, plain);
    if (q != nullptr) // the quotient is short by however many limbs u was padded
        std::fill_n(q + (un_norm - dn + 1), un - un_norm + 1, limb_t(0));
    std::copy_n(rem, dn, u);
    if (dn < un)
        std::fill_n(u + dn, un - dn, limb_t(0));
}

// The same with a block of its own, for the callers that are not inside a loop.
inline void divrem_multi(limb_t* q, limb_t* u, size_t un, const limb_t* d, size_t dn) {
    const scratch block(divrem_scratch_size(un, dn));
    divrem_multi(q, u, un, d, dn, block.top());
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

// Division with both results written into buffers that already exist, so that a
// loop can keep them and their capacity from one round to the next. Either
// result may be left out by passing a null pointer; a quotient nobody asks for
// still has to be written somewhere, and the working block below gives it a home
// instead of a buffer of its own. Leading zero limbs in the operands are fine, d
// must not be zero.
inline void divrem_into(const limb_t* u, size_t un, const limb_t* d, size_t dn,
                        std::vector<limb_t>* q, std::vector<limb_t>* rem) {
    un = mag_size(u, un);
    dn = mag_size(d, dn);
    assert(dn != 0);
    if (rem != nullptr)
        rem->clear();
    if (cmp_wide(u, un, d, dn) < 0) { // the dividend already is the remainder
        if (q != nullptr)
            q->clear();
        if (rem != nullptr)
            rem->assign(u, u + un);
        return;
    }
    const size_t qn = un - dn + 1; // the quotient is this long at most
    if (dn == 1) {                 // no copy of the dividend, one limb divisor
        limb_t r = 0;
        if (q != nullptr) {
            q->resize(un);
            r = divrem_limb(q->data(), u, un, d[0]);
            q->resize(mag_size(q->data(), un));
        } else {
            r = mod_limb(u, un, d[0]);
        }
        if (rem != nullptr && r != 0)
            rem->push_back(r);
        return;
    }
    // One block carries the working copy of the dividend, the quotient digits
    // when the caller does not want them, and the room the kernel needs.
    const size_t spare = q != nullptr ? 0 : qn + 1;
    const scratch block(un + spare + divrem_scratch_size(un, dn));
    mark m = block.top();
    limb_t* uu = m.take(un);
    std::copy_n(u, un, uu);
    limb_t* qd;
    if (q != nullptr) {
        q->resize(qn + 1);
        qd = q->data();
    } else {
        qd = m.take(qn + 1);
    }
    divrem_multi(qd, uu, un, d, dn, m.below());
    if (q != nullptr)
        q->resize(mag_size(q->data(), qn + 1));
    if (rem != nullptr) {
        rem->resize(dn);
        std::copy_n(uu, dn, rem->data());
        rem->resize(mag_size(rem->data(), dn));
    }
}

// The quotient as a fresh vector, the remainder into rem when one is wanted.
inline std::vector<limb_t> divrem_vec(const limb_t* u, size_t un, const limb_t* d,
                                      size_t dn, std::vector<limb_t>* rem) {
    std::vector<limb_t> q;
    divrem_into(u, un, d, dn, &q, rem);
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
    // Both results are written into objects that already exist and keep their
    // capacity, which is what lets a loop divide again without allocating. A
    // result that aliases one of the operands is still handled correctly.
    static void divrem(const xuint& a, const xuint& b, xuint& q, xuint& r) {
        check_divisor(b);
        if (&q == &a || &q == &b || &r == &a || &r == &b || &q == &r) {
            xuint qq, rr;
            detail::divrem_into(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), &qq.l_,
                                &rr.l_);
            q.l_.swap(qq.l_);
            r.l_.swap(rr.l_);
            return;
        }
        detail::divrem_into(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), &q.l_, &r.l_);
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
        // Both operands are normalised, so the top limb of the longer one is a
        // one somewhere and survives being combined with nothing: the result is
        // normalised too and needs no pass to be made so.
        r.l_.resize(a.l_.size() > b.l_.size() ? a.l_.size() : b.l_.size());
        detail::bit_wide<detail::bit_op::bit_or>(r.l_.data(), a.l_.data(), a.l_.size(),
                                                 b.l_.data(), b.l_.size());
        assert(r.l_.empty() || r.l_.back() != 0);
        return r;
    }

    friend xuint operator^(const xuint& a, const xuint& b) {
        xuint r;
        // The top limb of an xor of equal length operands can cancel, so unlike
        // the or above this one has to be trimmed back.
        r.l_.resize(a.l_.size() > b.l_.size() ? a.l_.size() : b.l_.size());
        detail::bit_wide<detail::bit_op::bit_xor>(r.l_.data(), a.l_.data(), a.l_.size(),
                                                  b.l_.data(), b.l_.size());
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
    friend class xint; // the signed companion is the only caller of bitwise_into

    static void check_divisor(const xuint& b) {
        if (b.is_zero())
            throw std::domain_error("alt::xuint division by zero");
    }

    static xuint quotient_of(const xuint& a, const xuint& b) {
        check_divisor(b);
        xuint q;
        detail::divrem_into(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), &q.l_, nullptr);
        return q;
    }

    // Only the remainder is asked for, so no quotient vector is needed at all.
    static xuint remainder_of(const xuint& a, const xuint& b) {
        check_divisor(b);
        xuint r;
        detail::divrem_into(a.l_.data(), a.l_.size(), b.l_.data(), b.l_.size(), nullptr, &r.l_);
        return r;
    }

    // The bitwise operation of two values held as a sign and a magnitude, taken
    // as the two's complement of an infinitely wide register is taken.  The
    // magnitude of the answer is written into out - which has to be a third
    // object, not one of the operands - and the sign that magnitude is to be
    // read with is returned.  One allocation, one pass; the cases and the
    // identity they rest on are worked out in the definition below and in
    // detail::bitwise_pass.
    static bool bitwise_into(xuint& out, const xuint& a, bool aneg, const xuint& b, bool bneg,
                             detail::bit_op op) noexcept;

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
    if (b.l_.empty() || l_.empty()) {
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
    // The product is an + bn limbs and the kernel writes every one of them, so
    // growing here only has to make room; the kernel notices that the result
    // overlaps its own first operand and copies it aside itself.
    const std::size_t an = l_.size(), bn = b.l_.size();
    l_.resize(an + bn);
    detail::mul_limbs(l_.data(), l_.data(), an, b.l_.data(), bn);
    normalize();
    return *this;
}

inline xuint& xuint::operator/=(const xuint& b) {
    check_divisor(b);
    if (b.l_.size() == 1) { // a single limb divisor lets the division run in
        const detail::limb_t d = b.low_limb(); // place, so nothing is allocated
        if (d == 1)
            return *this;
        detail::divrem_limb(l_.data(), l_.data(), l_.size(), d);
        normalize();
        return *this;
    }
    xuint q;
    detail::divrem_into(l_.data(), l_.size(), b.l_.data(), b.l_.size(), &q.l_, nullptr);
    l_ = q.l_; // copied rather than swapped, so this object keeps its capacity
    return *this;
}

inline xuint& xuint::operator%=(const xuint& b) {
    check_divisor(b);
    if (b.l_.size() == 1) { // a single limb divisor needs no working copy at all
        const detail::limb_t r = detail::mod_limb(l_.data(), l_.size(), b.low_limb());
        l_.clear();
        if (r != 0)
            l_.push_back(r);
        return *this;
    }
    xuint r;
    detail::divrem_into(l_.data(), l_.size(), b.l_.data(), b.l_.size(), nullptr, &r.l_);
    l_ = r.l_;
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
    // Growing here is the resize below and nothing else: the limbs it adds are
    // zeros, and a limb combined with zero by an or is the other limb.  What was
    // already here keeps its top limb, so the magnitude stays normalised.
    const std::size_t an = l_.size();
    l_.resize(an > b.l_.size() ? an : b.l_.size());
    detail::bit_wide_in<detail::bit_op::bit_or>(l_.data(), an, b.l_.data(), b.l_.size());
    return *this;
}

inline xuint& xuint::operator^=(const xuint& b) {
    if (b.l_.empty())
        return *this;
    const std::size_t an = l_.size();
    l_.resize(an > b.l_.size() ? an : b.l_.size());
    detail::bit_wide_in<detail::bit_op::bit_xor>(l_.data(), an, b.l_.data(), b.l_.size());
    normalize(); // the top limb of an xor can cancel
    return *this;
}

inline xuint& xuint::operator<<=(std::size_t count) {
    if (count == 0 || l_.empty())
        return *this;
    // More than four billion bits of shift is a bug rather than a wish.  The
    // bound is compared in a type that is at least 64 bits wide everywhere,
    // because on a host with a 32 bit size_t the shift producing it would
    // itself be undefined - and no count that large can even be spelled there.
    if (static_cast<unsigned long long>(count) > (1ull << 32))
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

// The definition rather than a method: both operands materialised in a window
// wide enough to hold its sign bit, the operation applied limb wise, the window
// read back as a sign and a magnitude.  Nothing needs it to compute anything -
// it is what the assert build compares the pass below against, and the reason
// the two's complement helpers above are kept.
inline bool bitwise_reference(std::vector<limb_t>& r, const limb_t* a, size_t an, bool aneg,
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

// What one pass of a signed bitwise operation needs: the two magnitudes, the
// two things that may be done to each before combining, the length to write,
// and whether a one is added to the answer.
struct bitwise_args {
    const limb_t* x;
    size_t xn;
    const limb_t* y;
    size_t yn;
    size_t limbs;
    bool x_dec;
    bool y_dec;
    bool x_not;
    bool y_not;
    bool plus_one;
};

// One pass over the magnitudes that produces the magnitude of the two's
// complement result.  It rests on ~x == -x - 1, which says a negative operand
// is ~(A - 1) in its magnitude A, so both the less one and the complement are
// taken inside the loop - the borrow chasing the one, the carry leading the
// plus one - and no window is materialised at all.  Within a limb the order is
// the one the identity asks for: subtract, then complement.
//
// A limb past the end of an operand reads as zero, which is what the complement
// of that operand then extends by, so no limb needs writing for the sign
// extension.  r may not alias x or y, and `limbs` has to be the length the case
// can produce plus the one limb a carried plus one could spill into - which
// case is which is spelled out where the arguments are picked, in
// xuint::bitwise_into.
//
// The chains die early and the pass is written to know it.  A borrow dies on the
// first non zero limb of whatever it is decrementing, a carry on the first limb
// that is not all ones, so over random operands all three are gone after the
// first limb - and a loop that carries them is one in which every limb waits for
// the limb before it, which is also a loop nothing can be done two limbs at a
// time in.  So the settling limbs go through the step one at a time and the rest
// go through the combination alone.  An operand of all ones or all zeroes keeps
// a chain alive to its end and then costs what the same loop with the chains
// costs, which is all it can cost: nothing is done twice.
template <bit_op Op>
inline void bitwise_pass(limb_t* r, const bitwise_args& c) noexcept {
    const limb_t not_x = c.x_not ? ~limb_t(0) : limb_t(0);
    const limb_t not_y = c.y_not ? ~limb_t(0) : limb_t(0);
    limb_t bx = c.x_dec ? limb_t(1) : limb_t(0); // the less one, borrowed as it goes
    limb_t by = c.y_dec ? limb_t(1) : limb_t(0);
    limb_t carry = c.plus_one ? limb_t(1) : limb_t(0);
    const auto step = [&](limb_t xv, limb_t yv) noexcept {
        const limb_t xs = xv - bx;
        bx = limb_t(xv < bx);
        const limb_t ys = yv - by;
        by = limb_t(yv < by);
        const limb_t m = bit_combine<Op>(xs ^ not_x, ys ^ not_y);
        const limb_t s = m + carry;
        carry = limb_t(s < m);
        return s;
    };
    size_t i = 0;
    while (i < c.limbs && (bx | by | carry) != 0) {
        const limb_t xv = i < c.xn ? c.x[i] : limb_t(0);
        const limb_t yv = i < c.yn ? c.y[i] : limb_t(0);
        r[i] = step(xv, yv);
        ++i;
    }
    // Nothing to carry any more, so a limb is the operation over two limbs and
    // two masks, and no limb needs the one before it.  The four runs each get
    // their end worked out first and their pointers hoisted, because testing two
    // bounds per limb is work no optimiser can hoist out of a loop whatever the
    // machine, and this one is where the length of the operation is.
    const limb_t* const xp = c.x;
    const limb_t* const yp = c.y;
    const size_t shared = c.xn < c.yn ? c.xn : c.yn;
    size_t n = shared < c.limbs ? shared : c.limbs;
    for (; i < n; ++i)
        r[i] = bit_combine<Op>(xp[i] ^ not_x, yp[i] ^ not_y);
    n = c.xn < c.limbs ? c.xn : c.limbs;
    for (; i < n; ++i)
        r[i] = bit_combine<Op>(xp[i] ^ not_x, not_y);
    n = c.yn < c.limbs ? c.yn : c.limbs;
    for (; i < n; ++i)
        r[i] = bit_combine<Op>(not_x, yp[i] ^ not_y);
    for (; i < c.limbs; ++i)
        r[i] = bit_combine<Op>(not_x, not_y);
    assert(carry == 0); // the window was meant to be wide enough
    static_cast<void>(carry);
}

// The pass for whichever of the three operations is asked for, chosen once here
// rather than once per limb.  Note that the operation handed over is not always
// the one the caller wanted: the and of two complements is the or of what they
// complement, so that case comes here as bit_or.
inline void bitwise_run(bit_op op, limb_t* r, const bitwise_args& c) noexcept {
    switch (op) {
        case bit_op::bit_and: bitwise_pass<bit_op::bit_and>(r, c); break;
        case bit_op::bit_or:  bitwise_pass<bit_op::bit_or>(r, c); break;
        default:              bitwise_pass<bit_op::bit_xor>(r, c); break;
    }
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

// Which pass a pair of signs needs.  Write A and B for the two magnitudes and
// A' for A - 1, so that a negative value is ~A', because ~x == -x - 1.  Applied
// to the four sign pairs, with De Morgan where both operands are complemented,
// and read left to right as and, or, xor, that gives
//
//   neither negative   A & B        A | B        A ^ B
//   both negative      (A'|B')+1    (A'&B')+1    A' ^ B'
//   one negative       B & ~A'      (A' & ~B)+1  (A' ^ B)+1
//
// The first row, the xor of the second and the and of the third are the results
// that come out non negative; the other five are negative.  A pass thus needs
// at most a decrement and a complement per operand and a one added at the end,
// all three of which it does carry, and hands the operation asked for unchanged
// except where both operands are complemented, where the and and the or swap.
//
// The length written is the longest the case can produce, plus the spare limb a
// carried one can spill into.  Two cases spill, the two that add a one to
// something no operand bounds: the and of two complements (in eight bits, with
// A = 0xFC and B = 5, 0xFB | 0x04 is 0xFF and the one makes 0x100) and the xor
// of a mixed pair (a = -1 and B = 0xFF, so 0x00 ^ 0xFF plus one is 0x100).  The
// or of a mixed pair adds a one as well, but its magnitude is bounded by A' + 1
// which is A, so the negative operand's own length already holds it, and the and
// of a mixed pair is bounded by the positive operand.
inline bool xuint::bitwise_into(xuint& out, const xuint& a, bool aneg, const xuint& b, bool bneg,
                                detail::bit_op op) noexcept {
    assert(&out != &a && &out != &b); // the pass reads its operands while writing
    using detail::bit_op;
    const std::size_t an = a.l_.size(), bn = b.l_.size();
    if (!aneg && !bneg) { // two magnitudes, so the unsigned operators are exact
        out = op == bit_op::bit_and ? (a & b) : op == bit_op::bit_or ? (a | b) : (a ^ b);
        return false;
    }
    detail::bitwise_args c{};
    c.x = a.l_.data();
    c.xn = an;
    c.y = b.l_.data();
    c.yn = bn;
    bit_op combined = op;
    bool negative = true;
    if (aneg && bneg) {
        c.x_dec = c.y_dec = true;
        c.limbs = an > bn ? an : bn;
        if (op == bit_op::bit_xor)
            negative = false; // A' ^ B' needs nothing added
        else {
            c.plus_one = true;
            ++c.limbs; // the one above can spill
            combined = op == bit_op::bit_and ? bit_op::bit_or : bit_op::bit_and;
        }
    } else {
        // Exactly one of the two is negative, so x becomes its magnitude and y
        // the other one's; the and and the xor do not care which was which.
        const bool flip = !aneg;
        const xuint& n = flip ? b : a;
        const xuint& p = flip ? a : b;
        c.x = n.l_.data();
        c.xn = flip ? bn : an;
        c.y = p.l_.data();
        c.yn = flip ? an : bn;
        c.x_dec = true;
        if (op == bit_op::bit_and) { // p & ~x'
            c.x_not = true;
            c.limbs = c.yn;
            negative = false;
        } else if (op == bit_op::bit_or) { // (x' & ~y) + 1
            c.y_not = true;
            c.plus_one = true;
            c.limbs = c.xn;
            combined = bit_op::bit_and;
        } else { // (x' ^ y) + 1
            c.plus_one = true;
            c.limbs = (c.xn > c.yn ? c.xn : c.yn) + 1;
        }
    }
    out.l_.resize(c.limbs);
    detail::bitwise_run(combined, out.l_.data(), c);
    out.normalize();
#ifndef NDEBUG
    // The same answer the slow way: both operands materialised wide enough for
    // their sign bit, combined, read back.  A reference, not a second opinion.
    std::vector<detail::limb_t> want;
    const bool want_negative =
        detail::bitwise_reference(want, a.l_.data(), an, aneg, b.l_.data(), bn, bneg, op);
    assert(want_negative == (negative && !out.l_.empty()));
    assert(want == out.l_);
#endif
    return negative && !out.l_.empty();
}

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
        xint r;
        r.neg_ = xuint::bitwise_into(r.m_, a.m_, a.neg_, b.m_, b.neg_, op);
        return r;
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

// Both work on the magnitude in place, which keeps its capacity, and take the
// divisor check from the unsigned division itself.
inline xint& xint::operator/=(const xint& b) {
    m_ /= b.m_;
    neg_ = neg_ != b.neg_;
    if (m_.is_zero())
        neg_ = false;
    return *this;
}

inline xint& xint::operator%=(const xint& b) {
    m_ %= b.m_; // the remainder keeps the sign of the dividend, which is this
    if (m_.is_zero())
        neg_ = false;
    return *this;
}

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
        // The digits themselves are handed to the unsigned class, which reads a
        // sign of its own, so a second one has to be refused right here: left in
        // place it would be taken for the sign of the magnitude and "++12" would
        // parse, where the digit loop just below - and the same routine of the
        // floating point header - call it an invalid digit.
        if (!s.empty() && (s.front() == '+' || s.front() == '-'))
            throw std::invalid_argument("alt::xint::from_string: invalid digit");
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

// The square on its own, because a * a is otherwise the only way to ask for one and
// a caller who means a square should be able to say so.  The two spellings reach the
// same kernels: a product whose two operands are the same array is a square, and so
// is this.  What is not a square is a * b where b happens to hold the same value in
// an object of its own - see is_square where the multiplication is decided.
inline xuint sqr(const xuint& a) { return a * a; }
inline xint sqr(const xint& a) { return a * a; }

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
    // One ladder, two buffers. Both the squaring and the multiply reduce into the
    // operand they were formed in, so the division below can keep its quotient and
    // remainder buffers for the whole run instead of building a new pair per bit.
    xuint q, r;
    const std::size_t bits = e.bit_size();
    for (std::size_t i = 0; i < bits; ++i) {
        if (e.test_bit(i)) {
            result *= acc;
            xuint::divrem(result, m, q, r);
            result = r;
        }
        if (i + 1 != bits) { // the last squaring would never be used
            acc *= acc;
            xuint::divrem(acc, m, q, r);
            acc = r;
        }
    }
    // A negative base with an odd exponent mirrors the residue.
    if (b.is_negative() && bits != 0 && e.test_bit(0) && !result.is_zero())
        result = m - result;
    return result;
}

// ---------------------------------------------------------------------------
// Greatest common divisor and friends.
// ---------------------------------------------------------------------------

namespace detail {

// ---------------------------------------------------------------------------
// Lehmer's Euclid.
//
// Plain Euclid spends one division of an n limb number by an n limb number on
// every round and needs O(n) rounds, which makes it O(n^3).  Lehmer's idea is
// to read the quotients off the top limbs of the pair, accumulate them into a
// 2x2 transform, and apply that transform to the whole numbers once, so that a
// run of Euclid rounds costs a handful of passes over the operands instead of a
// division per round.
//
// A quotient of the top limbs is not always a quotient of the numbers below
// them, so the round has to know when to stop - see lehmer_form below.  What the
// loop around it needs is a pair that stays non negative and no longer than the
// operands, and that is what the test on the window remainder does guarantee.
// Whether the last quotient of a run is one the plain algorithm would have taken
// is a different matter, and nothing here depends on it: the loop closes every
// run with a full division, and the coefficients of the equation are derived from
// the gcd and one inverse instead of being read off the run (see gcdext).
//
// A transform is kept as four magnitudes plus the parity of the number of steps
// they stand for,
//   even steps:  (u,v) -> ( p*u - q*v ,  s*v - r*u )
//   odd steps:   (u,v) -> ( p*v - q*u ,  s*u - r*v )
// because one step with quotient k rewrites them the same way in both parities:
// p' = s, q' = r, r' = q + k*s, s' = p + k*r.  The entries therefore never
// leave the natural numbers, and the determinant keeps the absolute value one.
// Each of the two rows has one entry of each sign, the one of the second row
// that is subtracted being r in either parity - the quantity the round below
// compares against the window.
// ---------------------------------------------------------------------------

struct form4 {
    limb_t p = 1, q = 0, r = 0, s = 1;
};

// The entries stay below this, so one of them times an n limb operand is n + 1
// limbs and a round never needs a buffer longer than its operands by more than
// two limbs.  It also keeps one quotient times one entry inside a single limb,
// which is what lets the transform be carried in four limbs at all.
inline constexpr limb_t form_limit = limb_t(1) << 62;

// Euclid over the top two limbs of the pair, accumulating the quotients into f
// for as long as they are worth taking.  u and v are the top two limbs of a pair
// of equal length, u >= v; they are consumed, only the entries mean anything to
// the caller.  q needs three limbs, work the room one two limb division asks
// for.  Returning does not mean the window is exhausted: whatever was
// accumulated so far is a product of steps of the pair below either way.
//
// With B for the base of the window, the second row of the transform, (-r, s),
// is worth
//
//     s*u_full - r*v_full = B * (s*u - r*v) + s*(u_full mod B) - r*(v_full mod B)
//                         >= B * (w - r)
//
// on the pair below, u and v being the window and w the window remainder of the
// step.  So while w does not fall short of r the row cannot turn negative, and
// one step later the same argument covers the other row - the pair therefore
// never leaves the values the buffers of the loop were sized for.  Asking w for
// both limbs is the classical way to make that true, and it puts w beyond any r
// the entries can reach.  What it does not promise is that the quotient is one
// the plain algorithm would take too: a run can end one step ahead.  Only the end
// of a run is affected and only the coefficients, which is why gcdext below does
// not take them from a run.
inline void lehmer_form(form4& f, unsigned& parity, limb_t* u, limb_t* v, limb_t* q,
                        mark work) {
    for (;;) {
        const size_t vn = mag_size(v, 2);
        if (vn == 0)
            return;
        const size_t un = mag_size(u, 2);
        if (cmp_wide(u, un, v, vn) < 0)
            return; // a step here would only exchange the two, gaining nothing
        limb_t w[2] = {u[0], u[1]};
        divrem_multi(q, w, 2, v, vn, work);
        // The kernel writes exactly as many quotient limbs as the two limb
        // dividend can produce, so only that many may be looked at.
        if (mag_size(q, 4 - vn) != 1)
            return; // a quotient too wide to become an entry
        const limb_t k = q[0];
        if (f.s != 0 && k > (form_limit - f.q) / f.s)
            return;
        if (f.r != 0 && k > (form_limit - f.p) / f.r)
            return;
        // The entry the step would put under the smaller component.  A remainder
        // that still fills the window stands above it, one that lost a limb no
        // longer does, however small the entry still is.
        const limb_t next_r = f.q + k * f.s;
        if (mag_size(w, 2) < 2)
            return; // the row would outrun the remainder it stands for
        const limb_t p = f.p, r = f.r, s = f.s;
        f.p = s;
        f.q = r;
        f.r = next_r;
        f.s = p + k * r;
        parity ^= 1;
        u[0] = v[0];
        u[1] = v[1];
        v[0] = w[0];
        v[1] = w[1];
    }
}

// One row of the coefficient matrix - or the pair itself, which is carried
// along in exactly the same way.  Of the four buffers, the ones at base and
// base + 1 hold the two values the loop works on and the other two wait for the
// results of the next round: a division step moves the base on by one, a Lehmer
// round by two, so no value is ever copied to make room for another.
struct row4 {
    limb_t* b[4];
    size_t n[4];
    bool s[4];
    unsigned base = 0;

    limb_t* p(size_t i) const noexcept { return b[(base + i) & 3]; }
    size_t& len(size_t i) noexcept { return n[(base + i) & 3]; }
    bool& neg(size_t i) noexcept { return s[(base + i) & 3]; }
    void advance(size_t k) noexcept { base = unsigned((base + k) & 3); }
    // Exchanges the two live values, which is what reordering the pair does to
    // every row that describes it.
    void exchange() noexcept {
        const size_t i = base, j = (base + 1) & 3;
        std::swap(b[i], b[j]);
        std::swap(n[i], n[j]);
        std::swap(s[i], s[j]);
    }
};

// out = a (+/-) b for signed magnitudes; out must not overlap either operand.
inline size_t add_signed(limb_t* out, bool& sgn, const limb_t* a, size_t an, bool aneg,
                         const limb_t* b, size_t bn, bool bneg) {
    if (an == 0) {
        std::copy_n(b, bn, out);
        sgn = bneg;
        return bn;
    }
    if (bn == 0) {
        std::copy_n(a, an, out);
        sgn = aneg;
        return an;
    }
    if (aneg == bneg) {
        sgn = aneg;
        return add_wide(out, a, an, b, bn);
    }
    if (cmp_wide(a, an, b, bn) >= 0) {
        sgn = aneg;
        sub_wide(out, a, an, b, bn);
        return mag_size(out, an);
    }
    sgn = bneg;
    sub_wide(out, b, bn, a, an);
    return mag_size(out, bn);
}

// out = am*a (+/-) bm*b for two signed magnitudes and single limb multipliers,
// the first product being formed in tmp, which needs an + 1 limbs.  out may be
// neither operand buffer, but it does overlap the second product written into it.
inline size_t row_form(limb_t* out, bool& sgn, const limb_t* a, size_t an, bool aneg,
                       limb_t am, const limb_t* b, size_t bn, bool bneg, limb_t bm,
                       limb_t* tmp) {
    if (an == 0 || am == 0) { // at most the second term is left
        if (bn == 0 || bm == 0) {
            sgn = false;
            return 0;
        }
        const limb_t c = mul_limb(out, b, bn, bm);
        out[bn] = c;
        sgn = bneg;
        return c != 0 ? bn + 1 : mag_size(out, bn + 1);
    }
    if (bn == 0 || bm == 0) { // and the same the other way round
        const limb_t c = mul_limb(out, a, an, am);
        out[an] = c;
        sgn = aneg;
        return c != 0 ? an + 1 : mag_size(out, an + 1);
    }
    const limb_t ca = mul_limb(tmp, a, an, am);
    tmp[an] = ca;
    const size_t tn = ca != 0 ? an + 1 : mag_size(tmp, an + 1);
    const limb_t cb = mul_limb(out, b, bn, bm);
    out[bn] = cb;
    const size_t on = cb != 0 ? bn + 1 : mag_size(out, bn + 1);
    if (aneg == bneg) {
        sgn = aneg;
        return add_wide(out, tmp, tn, out, on);
    }
    if (cmp_wide(tmp, tn, out, on) >= 0) {
        sgn = aneg;
        sub_wide(out, tmp, tn, out, on);
        return mag_size(out, tn);
    }
    sgn = bneg;
    sub_wide(out, out, on, tmp, tn);
    return mag_size(out, on);
}

// row1 <- row0 - q * row1, with both rows signed, written into the free buffer
// the row advances into.  tmp needs the lengths of q and of row1 added together.
inline void row_submul(row4& R, const limb_t* q, size_t qn, limb_t* tmp) {
    const limb_t* a = R.p(0);
    const size_t an = R.len(0);
    const bool aneg = R.neg(0);
    const size_t bn = R.len(1);
    const bool bneg = R.neg(1);
    if (qn == 0 || bn == 0) { // nothing to take away
        std::copy_n(a, an, R.p(2));
        R.len(2) = an;
        R.neg(2) = aneg;
        return;
    }
    mul_limbs(tmp, q, qn, R.p(1), bn);
    const size_t tn = mag_size(tmp, qn + bn);
    R.len(2) = add_signed(R.p(2), R.neg(2), a, an, aneg, tmp, tn, !bneg);
}

// What the loop hands back: the gcd and, for the flavour that asked for it, the
// coefficient of the second operand in the equation  x * a + y * b == gcd.
struct euclid_out {
    xuint g;
    xint y;
};

// Euclid over two magnitudes: a Lehmer round whenever both have the same length
// of at least two limbs, a plain division step otherwise - which is also how a
// pair of different lengths gets its leading limbs back in line.  Everything
// lives in one scratch block taken before the loop and is exchanged by moving
// pointers around, so a round allocates nothing.
//
// Tracking means carrying one more row along: the coefficients of b in the two
// components, following every step the pair takes.  They are exact combinations
// of the operands whichever quotients the rounds were built from, and they never
// grow past the operand they belong over, which is what makes the coefficient
// cost no more than the gcd alone.  Only one row is carried, because the caller
// takes the answer into a residue class, where the representative a run happens
// to end on stops mattering.
template <bool TrackCo>
inline euclid_out euclid_core(const xuint& a, const xuint& b) {
    euclid_out res;
    const size_t an = a.limb_size(), bn = b.limb_size();
    if (an == 0 || bn == 0) { // a zero pair member answers for both
        const bool has_a = an != 0;
        res.g = has_a ? a : b;
        if constexpr (TrackCo)
            res.y = has_a || bn == 0 ? xint(0) : xint(1);
        return res;
    }
    const size_t big = an > bn ? an : bn;
    // A component of the pair never gets longer than the operands and a row never
    // longer than the operand it belongs over.  On top of that, one slot has to
    // hold what a single entry (respectively a single quotient) can multiply a
    // value out to before the terms are brought together again.  The asserts at
    // the end of the loop watch the bounds.
    const size_t pslot = big + 2;
    const size_t rslot = 2 * big + 4;
    const size_t rows = TrackCo ? size_t(1) : size_t(0);
    scratch block(4 * pslot + rows * 4 * rslot + rslot + 4 * big + 19);
    mark m = block.top();
    row4 P;
    for (size_t i = 0; i < 4; ++i)
        P.b[i] = m.take(pslot);
    [[maybe_unused]] row4 Y;
    if constexpr (TrackCo)
        for (size_t i = 0; i < 4; ++i)
            Y.b[i] = m.take(rslot);
    limb_t* tmp = m.take(rslot);
    limb_t* q = m.take(big + 3);

    std::copy_n(a.limb_data(), an, P.b[0]);
    std::copy_n(b.limb_data(), bn, P.b[1]);
    P.n[0] = an;
    P.n[1] = bn;
    P.s[0] = false;
    P.s[1] = false;
    if constexpr (TrackCo) { // the first component is a, the second one is b
        Y.b[1][0] = limb_t(1);
        Y.n[0] = 0;
        Y.n[1] = 1;
        Y.s[0] = false;
        Y.s[1] = false;
    }

    for (;;) {
        if (cmp_wide(P.p(0), P.len(0), P.p(1), P.len(1)) < 0) {
            P.exchange(); // the dividend comes first, and its row along with it
            if constexpr (TrackCo)
                Y.exchange();
        }
        if (P.len(1) == 0)
            break; // the second component divides the first
        form4 f;
        unsigned parity = 0;
        if (P.len(0) == P.len(1) && P.len(1) >= 2) {
            limb_t u[2] = {P.p(0)[P.len(0) - 2], P.p(0)[P.len(0) - 1]};
            limb_t w[2] = {P.p(1)[P.len(1) - 2], P.p(1)[P.len(1) - 1]};
            lehmer_form(f, parity, u, w, q, m.below());
        }
        if (f.p != 1 || f.q != 0 || f.r != 0 || f.s != 1 || parity != 0) {
            // Apply the round.  The two source components are latched first,
            // because the results land in the other two buffers and the second
            // row would otherwise be built from an already overwritten length.
            const size_t i0 = parity, i1 = parity ^ 1;
            const limb_t* pa = P.p(i0);
            const limb_t* pb = P.p(i1);
            const size_t la = P.len(i0), lb = P.len(i1);
            bool over0 = false, over1 = false;
            P.len(2) = row_form(P.p(2), over0, pa, la, false, f.p, pb, lb, true, f.q, tmp);
            P.len(3) = row_form(P.p(3), over1, pb, lb, false, f.s, pa, la, true, f.r, tmp);
            // A component that came out negative was turned into its opposite, and
            // so has the row describing it.  With the test lehmer_form keeps that
            // should never happen; turning the value around only keeps the answer
            // right in case it ever does.
            assert(!over0 && !over1);
            if constexpr (TrackCo) {
                const limb_t* ya = Y.p(i0);
                const limb_t* yb = Y.p(i1);
                const size_t yl = Y.len(i0), yln = Y.len(i1);
                const bool ys = Y.neg(i0), ysn = Y.neg(i1);
                Y.len(2) = row_form(Y.p(2), Y.neg(2), ya, yl, ys, f.p, yb, yln, !ysn, f.q, tmp);
                Y.len(3) = row_form(Y.p(3), Y.neg(3), yb, yln, ysn, f.s, ya, yl, !ys, f.r, tmp);
                if (over0)
                    Y.neg(2) = !Y.neg(2);
                if (over1)
                    Y.neg(3) = !Y.neg(3);
            }
            P.advance(2);
            if constexpr (TrackCo)
                Y.advance(2);
        } else {
            // One division step.  The dividend is copied, because the remainder
            // has to land in the buffer the pair advances into rather than in the
            // one it was read from.
            std::copy_n(P.p(0), P.len(0), P.p(2));
            divrem_multi(q, P.p(2), P.len(0), P.p(1), P.len(1), m.below());
            const size_t qn = mag_size(q, P.len(0) - P.len(1) + 2);
            if constexpr (TrackCo)
                row_submul(Y, q, qn, tmp);
            P.len(2) = mag_size(P.p(2), P.len(1)); // at most as long as the divisor
            P.advance(1);
            if constexpr (TrackCo)
                Y.advance(1);
        }
        // The bounds the block above was sized for: neither the pair nor a row can
        // slip out of it, whatever the window quotients turned out to be worth.
        assert(P.len(0) <= big && P.len(1) <= big);
        if constexpr (TrackCo)
            assert(Y.len(0) <= big && Y.len(1) <= big);
    }
    res.g = xuint::from_limbs(P.p(0), P.len(0));
    if constexpr (TrackCo)
        res.y = xint::from_limbs(Y.p(0), Y.len(0), Y.neg(0));
    return res;
}

// Is the argument a negative number, whichever of the three flavours it is?
template <integral_like T>
inline bool is_negative(const T& v) noexcept {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, xuint>)
        return false;
    else if constexpr (std::is_same_v<std::remove_cvref_t<T>, xint>)
        return v.is_negative();
    else if constexpr (std::is_signed_v<T>)
        return v < T(0);
    else
        return false;
}

} // namespace detail

template <detail::integral_like A, detail::integral_like B>
inline xuint gcd(A a, B b) {
    xuint u = detail::magnitude_of(a), v = detail::magnitude_of(b);
    std::size_t un = u.limb_size(), vn = v.limb_size();
    if (un == 0)
        return v;
    if (vn == 0)
        return u;
    if (un < vn) { // the one limb path below wants the shorter number in v
        std::swap(u, v);
        std::swap(un, vn);
    }
    if (vn == 1) { // the shape almost every program is really in
        std::uint64_t y = v.limb_at(0);
        std::uint64_t x = un == 1 ? u.limb_at(0) : detail::mod_limb(u.limb_data(), un, y);
        while (x != 0) {
            const std::uint64_t r = y % x;
            y = x;
            x = r;
        }
        return xuint(y);
    }
    return detail::euclid_core<false>(u, v).g;
}

template <detail::integral_like A, detail::integral_like B>
inline xuint lcm(const A& a, const B& b) {
    xuint u = detail::magnitude_of(a);
    const xuint v = detail::magnitude_of(b);
    if (u.is_zero() || v.is_zero())
        return xuint();
    const xuint g = gcd(u, v);
    // Taking the divisor out of the longer operand keeps the multiplication
    // narrow; the product itself is the same either way.
    if (u.limb_size() >= v.limb_size()) {
        u /= g;
        return u * v;
    }
    return (v / g) * u;
}

// The gcd together with Bezout coefficients:  x * a + y * b == gcd(a, b), the gcd
// always positive.  The pair is the smallest one there is: |x| is below |b| /
// (2 * gcd) and |y| below |a| / (2 * gcd), with the exceptions the equation itself
// forces - x is zero exactly when the gcd is |b| (which covers a zero a and equal
// magnitudes), y is zero when the gcd is |a| unless the magnitudes are equal, and
// an operand of twice the gcd leaves its coefficient at plus or minus one.  Both
// are derived from the gcd and one modular inverse rather than read off an Euclid
// run: a Lehmer round can end on a quotient the plain algorithm would not take,
// and the coefficients of such a run are then one step further along the
// continued fraction than the smallest pair, off by a whole multiple of the other
// operand.
struct gcdext_result {
    xuint gcd;
    xint x, y;
};

template <detail::integral_like A, detail::integral_like B>
inline gcdext_result gcdext(A a, B b) {
    const xuint ua = detail::magnitude_of(a), ub = detail::magnitude_of(b);
    const bool aneg = detail::is_negative(a), bneg = detail::is_negative(b);
    if (ub.is_zero()) { // nothing to reduce against: the signs are the answer
        gcdext_result res{ua, xint(ua.is_zero() ? 0 : 1), xint()};
        if (aneg)
            res.x = -res.x;
        return res;
    }
    const xuint g = gcd(ua, ub);
    // Over the two magnitudes divided by the gcd, which are coprime, x is the
    // inverse of a and y follows from the equation, exactly divisible.  Taking the
    // inverse into the symmetric half of its residue class makes it the smallest x
    // there is, and a y divided out of the equation then stays below the other
    // operand over twice the gcd too - the twice the gcd case above, where x stops
    // at one, being the half a remainder can leave out.
    const xuint half(ub / g); // the modulus x is reduced against
    xint x = inv_mod(ua / g, half);
    if (const xint mod(half); x + x > mod)
        x -= mod;
    gcdext_result res{g, x, (xint(g) - x * ua) / ub};
    if (aneg)
        res.x = -res.x;
    if (bneg)
        res.y = -res.y;
    return res;
}

// The inverse of a modulo m, in [0, m).  Throws when gcd(a, m) is not one.
template <detail::integral_like A, detail::integral_like M>
inline xint inv_mod(const A& a, const M& modulus) {
    const xuint m(modulus);
    if (m.is_zero())
        throw std::domain_error("alt::inv_mod: zero modulus");
    if (m == xuint(1u))
        return xint();
    // The extended run over (m, |a| mod m) ends with  y * |a| + z * m == gcd, so
    // y is the inverse of a.  A coefficient of an Euclid run never grows past the
    // modulus it was computed against, so nothing has to be reduced inside the
    // loop and the single reduction below is all there is.
    const xuint residue = detail::magnitude_of(a) % m;
    const detail::euclid_out r = detail::euclid_core<true>(m, residue);
    if (r.g != xuint(1u))
        throw std::domain_error("alt::inv_mod: value is not invertible");
    const xint mod(m);
    xint res = r.y % mod; // the coefficient can leave [0, m) in both directions
    if (res.is_negative())
        res += mod;
    if (detail::is_negative(a) && !res.is_zero())
        res = mod - res; // what got inverted is |a|, so a negative a mirrors back
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

namespace detail {

// The square root, two ways.
//
// Newton's ladder is quadratic: a round averages the estimate with v over the
// estimate, which doubles the correct bits it holds.  Its cost is therefore the
// number of rounds times the cost of one full division, and the number of rounds
// grows by one only every time the operand doubles - so above a handful of limbs a
// root costs a dozen divisions each as expensive as the root itself.  Below that it
// is the right shape, because the whole answer is then a few cheap divisions and
// there is nothing to set up.
//
// The split is the other shape.  The root of the top half of an operand is the top
// half of its root, and the bottom half then costs one division rather than a
// ladder: writing v = (s * 2^h + u)^2 + rem for the recursion's s and the missing u,
// what the high part left over carries into the low half, and u is that sum over
// twice s * 2^h, to within one unit.  T(n) = T(n/2) + a division of half the width,
// so the work is a geometric series instead of a ladder of full ones, and its
// exponent follows the multiplication rather than the division.
//
// Where the two change places is a property of the machine's multiplier, so it is a
// build time knob like the multiplication thresholds, and it is measured - see
// build\sqrtthin.cpp built six ways by build\sqrtbasewalkbuild.bat and walked through one
// session in rotating order by build\sqrtbasewalk.bat.  Four bases are timed against a
// sixth build whose base is so high that the recursion never starts, which is the ladder's
// own price and the denominator below; every build also times the same 256 by 256 product,
// and that figure is divided out, because the six builds read it as much as six percent
// apart for reasons that have nothing to do with roots.  The published sqrtrem over that
// ladder, at an operand of the width named:
//
//   limbs of the operand     9    12    16    24    32    48    64   128   256   512
//   base 8                 0.87  1.06  0.87  0.52  0.44  0.41  0.32  0.16  0.13  0.11
//   base 16                1.04  1.09  1.08  0.53  0.55  0.39  0.34  0.19  0.14  0.11
//   base 32                0.99  1.07  1.06  0.97  1.04  0.53  0.44  0.21  0.15  0.13
//   base 64                1.04  1.07  1.04  1.08  1.05  1.09  1.00  0.36  0.18  0.15
//
// A row of about one is that base leaving the operand to the ladder, a base splitting only
// what is wider than itself - base 64 is the ladder at sixty four limbs and base 16 is it
// at sixteen, and the table says so to within its spread.  Read down rather than across it
// says where levels stop being worth it: at sixty four limbs the base 8 root is a third of
// the ladder's while the base 64 one has not started, and at a hundred and twenty eight
// 0.16 against 0.36.  Eight is the base on that evidence, and the one row where it is not
// ahead of itself is twelve limbs - where the three builds that never split read 666, 667
// and 666 nanoseconds, the ladder's own quickest 627 and the split's 657, so six percent
// of a six hundred nanosecond root and inside the spread the session itself has.
//
// The lift in sqrtrem is what makes so low a base safe.  A level counts the half it hands
// down in limbs while the root that half stands for is counted in bits, so an operand whose
// top limb carries fifty six leading zeros leaves the candidate it computes far over the
// root; the level then walks its bound, hands the call back, and the caller pays for the
// walk and the ladder.  The same probe calling the split directly on such an operand -
// which nothing in the library does any more, the lift being the only road to a level - is
// 2.6 over the ladder at twelve limbs and 1.4 at thirty two.  With the zeros taken out
// first, build\refuserate.cpp counts no call handed back at all over ten thousand operands
// per bound, drawn as a whole number of random limbs from nine limbs to two thousand and
// forty eight, which is the population the library actually roots.
//
// Eight is also the smallest value the levels can be given at all: an operand of more than
// eight limbs leaves the recursion at least five, and the sum a level builds over the root
// it gets back is `low + high / 2 + 3` limbs wide.  Five is where that still fits inside
// the operand.
#ifndef AXINT_SQRT_BASE
#define AXINT_SQRT_BASE 8
#endif

inline constexpr size_t sqrt_base = AXINT_SQRT_BASE;

static_assert(sqrt_base >= 8,
              "a level builds a sum low + high / 2 + 3 limbs wide over the root of the "
              "high half: eight is where the high half is never so small that the sum "
              "stops fitting inside the operand the level was given");

// An overestimate of the root of v[0,un), held in about fifty one bits, written to r
// with its length returned - or zero when the operand is too narrow for the trick and
// the ladder's own power of two seed is cheaper than any floating point.  The operand
// never reaches the floating point as a whole: its top bits move into a double, the
// host's square root is taken there, and the exponent is put back with shifts, so the
// seed of a root of a million bits costs no more than that of a root of a hundred.
//
// The window is fifty one bits because that puts its root under two to the twenty six,
// where lifting it by twenty seven more still lands inside the range in which a
// double holds whole numbers exactly - the twenty seven bits are what turn the host's
// twenty six bit answer into fifty one bits of root, and the fraction it holds is
// worth keeping.
//
// Three things can leave the estimate under the root.  Dropping the low bits of the
// operand does it by less than two to the (drop/2 - 26), the root of d + 1 being under
// the root of d by less than one over twice the root and the root here over two to the
// twenty five; the host's square root rounds to within half a unit of its last place,
// a quarter of a unit of the estimate; and the cast to an integer truncates, one unit
// more.  Sixteen are added for the three of them, four times the worst case, and
// standing over the root is what lets the ladder only ever come down - the
// alternative being to check the estimate by squaring it, which costs the very round
// the seed was introduced to save.
inline size_t sqrt_seed(limb_t* r, const limb_t* v, size_t un) noexcept {
    const size_t bits = mag_bit_size(v, un);
    if (bits < 128)
        return 0; // a limb or two: the rounds are cheaper than the estimate
    constexpr int keep = 51;
    constexpr int frac = 27; // bits of the root held below the point
    // An even number of bits has to be dropped for what follows the root to be a
    // whole exponent, which the parity of the operand's length does not arrange.
    const int dropped = int(bits) - keep;
    const int wide = keep - (dropped & 1);
    const size_t drop = bits - size_t(wide);

    // The top `wide` bits of the operand, however they straddle the limb boundary:
    // the top limb's own bits, and what the limb below owes it.
    const size_t top = (bits - 1) / limb_bits;
    const int in_top = int(bits - top * limb_bits); // bits of it in use, 1..64
    // The two halves of the window have to lie against one another: what the top limb
    // keeps goes immediately above the bits borrowed from the limb below, so it is
    // shifted by the room the window has left for it and not by the room the limb has
    // left.  Aligning it to the top of the limb instead leaves a gap of `limb_bits -
    // wide` bits in the middle of the window, which inflates it by up to two to the
    // (limb_bits - wide) - a root too high by six to seven bits on three quarters of all
    // bit counts, and a hundred and twenty eight times too high in the estimate's own
    // units, since the ladder starts above and comes down, each round of it halving an
    // excess it could have skipped.  The gap also pushes the estimate past two to the
    // fifty three, out of the range where a double holds whole numbers, which is what
    // the sixteen below are meant to cover.
    limb_t window;
    if (in_top >= wide)
        window = v[top] >> (in_top - wide);
    else
        window = (v[top] << (wide - in_top)) |
                 (v[top - 1] >> (limb_bits - (wide - in_top)));

    const limb_t est = static_cast<limb_t>(std::sqrt(static_cast<double>(window)) *
                                           double(limb_t(1) << frac)) +
                       limb_t(16);
    const size_t shift = drop / 2 - frac; // drop is at least 77 bits, so this is one
    const size_t limbs = shift / limb_bits;
    const unsigned sh = unsigned(shift % limb_bits);
    std::fill_n(r, limbs, limb_t(0));
    r[limbs] = est << sh;
    size_t rn = limbs + 1;
    if (sh != 0 && (est >> (limb_bits - sh)) != 0) {
        r[rn] = est >> (limb_bits - sh);
        ++rn;
    }
    return rn;
}

// Limbs of scratch a ladder keeps: the estimate and its successor, the copy of the
// operand the division eats, the quotient, the square the answer is measured against,
// and what the division kernel asks for on top of those.
inline size_t sqrt_ladder_scratch_size(size_t un) {
    const size_t xn = un / 2 + 3;
    return 3 * xn + 3 * un + divrem_scratch_size(un, xn) + 16;
}

// The floor root of v[0,un) into r (un/2 + 3 limbs) and what is left under it into
// rem (un limbs).  The ladder only ever comes down, so it starts above: from the
// floating point estimate when `seeded` allows, and from a power of two otherwise.
// Should a seeded run ever turn out to have started under the root - the estimate is
// built to stand over it, and the bound on the correction below is what turns that
// "built to" into a proof - the ladder is run again from the power of two, which needs
// neither floating point nor luck.
inline void sqrt_ladder(limb_t* r, limb_t* rem, const limb_t* v, size_t un, mark m,
                        bool seeded) {
    const size_t xn = un / 2 + 3;
    limb_t* x = m.take(xn);
    limb_t* y = m.take(un + 4);
    limb_t* u = m.take(un + 1);
    limb_t* q = m.take(un + 2);
    limb_t* p = m.take(2 * xn + 2);

    const auto run = [&](bool use_seed, size_t bound) -> bool {
        size_t rn = use_seed ? sqrt_seed(x, v, un) : 0;
        if (rn == 0) {
            const size_t k = (mag_bit_size(v, un) + 1) / 2; // a power of two over it
            std::fill_n(x, xn, limb_t(0));
            x[k / limb_bits] = limb_t(1) << (k % limb_bits);
            rn = k / limb_bits + 1;
        }
        for (;;) {
            std::copy_n(v, un, u);
            divrem_multi(q, u, un, x, rn, m.below());
            size_t yn = add_wide(y, x, rn, q, mag_size(q, un - rn + 2));
            rshift(y, y, yn, 1);
            yn = mag_size(y, yn);
            if (yn > xn || cmp_wide(y, yn, x, rn) >= 0)
                break; // the round did not improve on x, so it was the last one
            std::copy_n(y, yn, x);
            rn = yn;
        }
        // The ladder stops at the root or one above it: a floor root two units over
        // the answer would still have improved on it.  Coming down costs a product a
        // step; going back up costs none, because the next unit of root costs 2x + 1
        // of remainder - the same quantity that answers whether it fits pays for the
        // step, and what survives the steps is the remainder the answer leaves.
        size_t steps = 0;
        for (;;) {
            mul_limbs(p, x, rn, x, rn);
            if (cmp_wide(p, 2 * rn, v, un) <= 0)
                break;
            if (++steps > bound)
                return false;
            sub_limb(x, x, rn);
            rn = mag_size(x, rn);
        }
        // The product now stands under the operand, which is what makes both the
        // subtraction and its length safe: the limbs of p above the operand's own are
        // zero, a product that reaches past them could not have stood under it.
        sub_wide(rem, v, un, p, rn * 2 < un ? rn * 2 : un);
        for (;;) {
            // The price of the next unit of root is 2x + 1.  The shift hands the bit
            // it pushed out of the top limb back as a value, so it has to be put into
            // the limb above the shifted ones before the length of the double is read.
            const limb_t spill = lshift(x, rn, y, 1);
            y[rn] = spill;
            const size_t tn = mag_size(y, rn + 1); // the length of 2x
            y[tn] = limb_t(0);
            add_limb(y, y, tn, limb_t(1)); // 2x is even, so the one cannot carry out
            if (cmp_wide(rem, un, y, tn) < 0)
                break; // it does not fit, so x is the root and rem is its remainder
            if (++steps > bound)
                return false;
            sub_wide(rem, rem, un, y, tn);
            if (add_limb(x, x, rn, limb_t(1)) != 0) {
                if (rn + 1 > xn)
                    return false; // a root the ladder has no room to hold
                x[rn++] = limb_t(1);
            }
        }
        std::copy_n(x, rn, r); // the answer itself, and the zeros above it
        std::fill_n(r + rn, xn - rn, limb_t(0));
        return true;
    };
    if (seeded && run(true, 4))
        return;
    run(false, ~size_t(0));
}

// Limbs of scratch one level of the split keeps: what the recursion writes, the sum
// the missing part is divided out of, that division's quotient, twice the recursion's
// root as a divisor, and the two products the remainder is rebuilt from.  The child is
// handed the mark by value, so a level's buffers outlive it and only the depths that
// are genuinely live share space.
inline size_t sqrt_scratch_size(size_t un) {
    size_t total = sqrt_ladder_scratch_size(sqrt_base) + sqrt_base + sqrt_base / 2 + 24;
    for (size_t n = un; n > sqrt_base;) {
        const size_t low = (n / 2) & ~size_t(1);
        const size_t high = n - low;
        const size_t sn_max = high / 2 + 3; // limbs the level's root can need, so that
        const size_t wn = low + sn_max;     // the sum built on top of it has a width
        const size_t qn = wn + 2;           // the quotient of that sum never exceeds
        // What one level keeps live at a time, spelled out rather than folded, since
        // every one of these figures is a buffer the level below also sees: the
        // recursion's root and its remainder, the sum, the copy of it a division eats,
        // twice the root as a divisor, the quotient, the two products the correction
        // is made of, the 2u + 1 the last step is tested against, and a margin.
        total += sn_max + high + 2 * wn + sn_max + 1 + qn + 2 * (2 * qn + 6) + qn + 68;
        n = high;
    }
    return total;
}

// How far a level walks a candidate before refusing it.  The estimate stands over the
// root by what its own square takes off the sum, which grows for every two leading zeros
// the operand's top limb carries - the half handed to the recursion is counted in limbs
// while the root it stands for is counted in bits.  That is why the lift in sqrtrem comes
// before this: with the top limb whole to within one bit the excess is under a unit, and
// the walk is a correction rather than a search.  A walk past the bound still hands the
// call to the ladder, which answers without owing anything to a lemma about how far an
// estimate may stand from the root, so the bound only ever bounds the worst case.
// build\refuserate.cpp counts what the levels actually do with ten thousand operands per
// bound, drawn as a whole number of random limbs - so with a random number of leading
// zeros in the top limb, which is the population the library roots - from nine limbs to
// two thousand and forty eight:
//
//   steps a level may take           4     8    16
//   calls handed to ladder           0     0     0
//   longest walk of one root         7     7     7
//
// The same ten thousand at each bound, checked answer against answer with the ladder's:
// nothing to report.  The walk of one root is what its levels spent together, the deepest
// of them never more than four, and the bound is of no use until it is near that - so it
// sits at eight, twice the longest correction measured and half the worst case the
// sixteen it replaces would have walked.  The figure of two hundredths of a percent once
// quoted here for sixteen was measured over operands whose top limb was forced whole,
// where the estimate was near the root for that reason alone; the same operands with a
// top limb that is not whole handed every single call back, which is what the lift is for.
#ifndef AXINT_SQRT_STEPS
#define AXINT_SQRT_STEPS 8
#endif

inline constexpr size_t sqrt_steps_bound = AXINT_SQRT_STEPS;

// r (un/2 + 3 limbs) and rem (un limbs) come back with the floor root of v[0,un) and
// what is left over it.  A level reports false when its own correction needed more
// rounds than the split can promise, which the caller answers with the ladder: the
// ladder tests its correction against the operand itself, so it never needs to know how
// far an estimate is allowed to stand from the root in order to promise an end.
inline bool sqrt_rec(limb_t* r, limb_t* rem, const limb_t* v, size_t un, mark m) {
    if (un <= sqrt_base) {
        sqrt_ladder(r, rem, v, un, m, true);
        return true;
    }
    const size_t low = (un / 2) & ~size_t(1); // a whole and an even number of limbs,
    const size_t high = un - low;             // so that the shift below is one too
    const size_t k = low / 2;                 // B^k is the half the root is missing
    const size_t sn_max = high / 2 + 3;       // limbs the recursion's root can need,
                                              // the same room this level is owed
    const size_t wn = low + sn_max;           // and the width of the sum below
    const size_t qn = wn + 2;                 // never less than the quotient's own

    limb_t* s = m.take(sn_max);
    limb_t* rest = m.take(high);
    if (!sqrt_rec(s, rest, v + low, high, m.below())) {
        return false; // the level below refused, so this one has no root to build on
    }
    const size_t sn = mag_size(s, sn_max);
    const size_t rsn = mag_size(rest, high); // what the recursion left: at most 2s
    assert(rsn <= sn_max);

    limb_t* w = m.take(wn); // v - (s * B^k)^2: what the missing part has to divide
    std::copy_n(v, low, w); // itself into, the remainder of the high part carrying
    std::copy_n(rest, rsn, w + low); // in above the low half
    std::fill_n(w + low + rsn, wn - low - rsn, limb_t(0));

    limb_t* d = m.take(sn_max + 1); // twice what has been found of the root
    d[sn] = lshift(s, sn, d, 1);    // the limb the shift carries out has to be kept:
                                    // the division below reads a divisor of that length
    size_t dn = sn + (d[sn] != 0 ? 1 : 0);
    if (dn == 0)
        dn = 1; // s is never zero here, but this is not where to learn it

    limb_t* q = m.take(qn);
    // The division is handed a copy of the sum, because a division writes its
    // remainder over the dividend: the sum is what the candidate has to fit under
    // and what it leaves behind is needed for the remainder of the root, while what
    // a division leaves where the dividend was is smaller than the divisor - set a
    // candidate against that and it squares over the operand every single time.
    limb_t* div = m.take(wn);
    std::copy_n(w, wn, div);
    divrem_multi(q, div, wn, d, dn, m.below()); // the remainder it leaves there is
                                                // the part of the sum the quotient
                                                // could not account for, unneeded
    // The quotient reaches wn - dn + 2 limbs and the division pads to exactly that,
    // so reading above it reads whatever the last operation left in the block - and a
    // length taken from those limbs is never the length of a quotient, which is how
    // this level refused to work every single time it was asked to.
    const size_t qw = wn - dn + 2;
    limb_t* u = q + k;               // the low half of that quotient is noise: the
    size_t un_q = qw > k ? mag_size(u, qw - k) : 0; // answer sits above it
    if (un_q > k)
        return false; // the estimate ran wider than the room the split has for it

    limb_t* sub = m.take(2 * qn + 6);
    limb_t* prod = m.take(2 * qn + 6);
    const size_t tn = qn + 4;
    limb_t* t = m.take(tn + 2); // the two hold what a sum can carry past its length
    for (size_t steps = 0;;) {
        // What the candidate has taken from w: u * 2s * B^k + u * u.
        std::fill_n(sub, 2 * qn + 4, limb_t(0));
        size_t used = 0;
        if (un_q != 0) {
            mul_limbs(prod, u, un_q, d, dn);
            used = add_wide(sub + k, sub + k, 2 * qn + 4 - k, prod, un_q + dn);
            mul_limbs(prod, u, un_q, u, un_q);
            used = add_wide(sub, sub, 2 * qn + 4, prod, 2 * un_q);
        }
        if (cmp_wide(sub, used, w, wn) > 0) { // it squared over the operand
            if (un_q == 0 || ++steps > sqrt_steps_bound)
                return false;
            sub_limb(u, u, un_q);
            un_q = mag_size(u, qw - k);
            continue;
        }
        sub_wide(w, w, wn, sub, used); // w is what the candidate left under itself
        // And whether one more unit of root still fits under that, the test being
        // against twice the candidate root plus one: 2s above the shift, 2u + 1
        // below.  The limb the shift pushes out of the top comes back as a value, so
        // it goes into the limb above the shifted ones before the length is read -
        // and the double is even, which is what keeps the one from carrying out.
        std::fill_n(t, tn + 2, limb_t(0));
        std::copy_n(d, dn, t + k);
        limb_t* e = prod; // 2u + 1, in the room the products vacated
        e[un_q] = lshift(u, un_q, e, 1);
        const size_t en = mag_size(e, un_q + 1); // the length of 2u
        if (en > tn - k)
            return false; // the candidate no longer fits under the half the sum has
        e[en] = limb_t(0);
        add_limb(e, e, en, limb_t(1));
        add_wide(t + k, t + k, tn - k, e, en);
        if (cmp_wide(w, wn, t, mag_size(t, tn)) >= 0) { // one more unit fits
            if (++steps > sqrt_steps_bound)
                return false;
            if (add_limb(u, u, un_q, limb_t(1)) != 0) {
                u[un_q] = limb_t(1); // the carry the addition could not place, and
                ++un_q;              // the window has a limb to spare for it
            }
            if (un_q > k)
                return false; // the candidate outgrew the half the split has for it
            continue;
        }
        break;
    }

    std::copy_n(u, un_q, r);       // the candidate below the half the recursion found,
    std::fill_n(r + un_q, k - un_q, limb_t(0)); // with the room between them zeroed
    std::copy_n(s, sn, r + k);
    // The room above what is written, which the caller was promised just as much, has
    // to be zeroed too: a level takes the length of the one below it, and an unwritten
    // limb of a block that was never zeroed is read as a limb of the number it stands
    // for - which is how a root came back far too big once the level was reached.
    std::fill_n(r + k + sn, un / 2 + 3 - (k + sn), limb_t(0));
    std::copy_n(w, wn, rem);       // what the root leaves of v, and the zeros above it
    std::fill_n(rem + wn, un - wn, limb_t(0));
    return true;
}

// An overestimate of the n-th root of v, held in forty four bits of a limb: the top of
// the operand through the host's pow, the exponent put back by a shift, and a margin of
// one part in a trillion against the two to the minus forty nine of itself that the
// estimate can be wrong by.  The window leaves the root known to about forty nine bits,
// which is where the forty four come from and why the margin is what sets the price: the
// ladder agrees with the answer in as many leading bits as the margin leaves over, and
// every one of them it throws away is a round of full width division it has to buy back.
// The asymmetry is the square root's - standing too high costs the ladder one round,
// standing too low is the one thing a decreasing ladder cannot recover from - and the
// fallback is the power of two, which needs no floating point at all.
//
// The shift back is fixed at the width the estimate was scaled to, not measured from the
// estimate's own bit count.  A margin can carry the estimate over the top of that width,
// and then the candidate stands one bit above the answer's own span, which a decreasing
// ladder absorbs; a shift shortened by the extra bit puts it at half the answer instead,
// where the first round cannot improve it, the correction bound runs out over whole
// powers of the candidate, and the ladder has to start again from a power of two.
inline xuint root_seed(const xuint& v, unsigned n) {
    const size_t bits = v.bit_size();
    const size_t k = (bits - 1) / n + 1; // bits the root occupies
    const xuint plain = xuint(1u) << k;  // one correct bit, and no assumptions
    constexpr int hold = 44;             // bits the estimate keeps
    // Two bits of slack over the width the estimate can hold at all - below that the
    // answer is no wider than the estimate and the shift below would be a negative one.
    // The bound is measured rather than assumed: build\seedband.cpp runs the same ladder
    // from both starts and prints build\seedband_fold.txt, where forty six bits of answer
    // is the same cost either way and forty eight is 0.41 of it, two rounds where the
    // power of two needs seven.  The bound used to be twice this width, which was written
    // when the estimate held thirty two bits and cost three rounds of full width division
    // above it at every width.
    if (bits < 128 || k <= hold + 2)
        return plain; // an answer too short for an estimate to be worth a host call

    constexpr int wide = 51; // the same window the square root reads: its n-th root
                             // keeps forty nine of its fifty one bits, so forty four of
                             // them are real rather than the thirty two held before
    const limb_t* d = v.limb_data();
    const size_t top = (bits - 1) / limb_bits;
    const int in_top = int(bits - top * limb_bits); // bits of it in use, 1..64
    // The same window the square root reads, assembled the same way - see the note there
    // for why the top limb is shifted by the room the window has left rather than by the
    // room its own limb has left.
    limb_t window;
    if (in_top >= wide)
        window = d[top] >> (in_top - wide);
    else
        window = (d[top] << (wide - in_top)) |
                 (d[top - 1] >> (limb_bits - (wide - in_top)));

    // The root of the operand is the root of the window times two to the (bits -
    // wide)/n, so the held estimate is that, moved to sit `hold` bits under the top
    // of the answer.  The exponent is shared between a whole number of powers of two,
    // put back by scaling the floating point value, and the remainder, which goes under
    // the root sign with the window.  Taking the difference the estimate would otherwise
    // need is a subtraction between two quantities as large as the operand's bit count,
    // and what survives it is a relative error of two to the minus thirty three for an
    // operand of a million bits - dearer by a thousand fold than the margin the estimate
    // is supposed to leave room for.
    const size_t whole = (bits - wide) / n; // the powers of two, out of the root sign
    const size_t rest = (bits - wide) % n;  // and what stays under it
    const double g = std::pow(static_cast<double>(window) * std::ldexp(1.0, int(rest)),
                              1.0 / double(n));
    const double f = std::ldexp(g, int(long long(whole) + hold - long long(k)));
    if (!(f > 0.0) || !(f < double(1ull << (hold + 2)))) // NaN, zero, or nonsense
        return plain;
    std::uint64_t est = static_cast<std::uint64_t>(f) + 1;
    est += est / (1ull << 40) + 8; // too high being harmless, unlike too low
    xuint r = xuint::from_limbs(&est, 1);
    r <<= k - size_t(hold); // the estimate is the answer divided by two to the (k -
                            // hold), so putting it back needs no measurement of its own
    return r;
}

// The ladder of levels, for an odd degree over a square root's.
//
// The ladder the call below runs is Newton's, and a Newton round is as wide as the operand
// whatever it has already settled: it divides v by x^(n-1) whole, and three quarters of the
// round is that division - 5874 of its 8185 nanoseconds at eight thousand bits of a cubic,
// 270909 of 312742 at sixty five thousand (tmp\folds\irootrounds.cpp).  The quotient of it is
// a fraction one over n of the operand, which is neither the shape the reciprocal of a
// division is worth taking nor half of what a split would need, so the ladder pays the whole
// price of that division eleven times over at sixty five thousand bits.
//
// This one keeps an approximation standing on a scale instead of a guess at the whole answer.
// With k the bits the answer occupies and known the bits settled so far,
//
//   W  = v >> (n * (k - known))   the window of the operand seen so far,
//   An = A^n <= W                 so A is a lower bound for the root of the window.
//
// A level brings b more bits of the operand into that window, and the gap between the two
// windows costs no power of a candidate, the previous one having been kept:
//
//   Wn - (A << b)^n = Wn - (An << (n * b)).
//
// The step over that gap wants a quotient of b bits and a divisor of n * A^(n-1), which is
// (n-1) times the width of the approximation rather than of the operand,
//
//   d = ((Wn - (An << (n*b))) >> (b*(n-1))) / (n * A^(n-1)),
//
// n * (A<<b)^(n-1) being that divisor shifted by b*(n-1), and the bits of the gap below the
// shift deciding nothing at the precision a level asks for.  The divisor therefore shrinks
// with the level while the levels double what they settle, so the ladder costs about its last
// level instead of about its operand times its rounds.  The two step counts are close - eight
// rounds and seven levels at eight thousand bits of a cubic, eleven and ten at sixty five
// thousand, both growing like the logarithm of the answer (tmp\folds\levels.exe rounds) - so
// it is the width of a step that buys the difference, and on the call below it was measured
// against a build of this header without it - tmp\folds\rootab.cpp, seven walks, folded by
// tmp\folds\irootfold.py into tmp\folds\rootab_fold2.txt - at 0.41 of the former time for
// eight thousand bits of a cubic, 0.26 for sixteen thousand and 0.14 for sixty five thousand,
// and 0.40, 0.27, 0.16 at those same widths for a fifth.
//
// A level is taken short of doubling the bits settled, by the width of n-1 plus three, so
// that the second order term of the step - (n-1) d^2 / (2 A 2^b), the price of the tangent
// standing under a convex curve - stays under one unit.  Over degrees 3 to 65 and answers of
// 300 to 21846 bits the quotient of a level landed on the window root every single time, so
// neither of the two corrections below ran at all (tmp\folds\levels.exe stats), which is why
// both are written to cost nothing when they do not.
//
// A zero answer means the levels declined the operand and the ladder below stands.
inline xuint iroot_levels(const xuint& v, unsigned n) {
    const std::size_t nn = n;
    const std::size_t bits = v.bit_size();
    const std::size_t k = (bits - 1) / nn + 1; // bits the answer occupies
    // The gate, and it is the cubic's own.  Below a thousand bits of answer the levels are
    // the slower path, their extra shifts and their extra power per level not yet paid for by
    // the division they save; the crossover was measured over degrees 3, 5, 13 and 23 and sits
    // lowest for the cubic, a loss by a quarter at seven hundred bits of answer against a win
    // by an eighth at eight hundred, where every other degree had crossed already - a sixth of
    // the way back at six hundred bits and degree thirteen.  A thousand bits of answer keeps
    // the whole band where the two paths agree on the side of the one that is known to win
    // (tmp\folds\levels_gate.txt).  The other condition is range: the widest shift a level
    // performs is n*b bits, no more than bits + n, and a shift of more than four billion bits
    // is a bug in this library rather than a wish, so an operand that vast is left to the
    // ladder instead of thrown out of the call.
    if (k < 1024 || bits > (1ull << 32) - nn)
        return xuint();

    const xuint one(1u);
    const xuint deg(n);
    constexpr std::size_t hold = 44; // bits root_seed carries, and so the width of the first
                                     // window, which is all the estimate can be trusted over
    xuint A = root_seed(v, n) >> (k - hold);
    std::size_t known = hold;
    const xuint W = v >> (nn * (k - known));
    // The estimate stands over the root of that window by as many of its own units as the
    // margin of root_seed carries, some thirty.  Walking down one unit at a time would spend
    // thirty powers on it, so the gap is asked of Newton instead: one division at the width of
    // the window, and a step taken from above never crosses the root.  What that leaves under
    // the root the loop after it walks back onto, one power to a step.  Standing exactly on the
    // window root is what frees every level from slack of its own.
    xuint An;
    for (unsigned guard = 0;; ++guard) {
        An = pow(A, static_cast<unsigned long long>(n));
        if (An <= W)
            break;
        xuint excess, unused;
        xuint::divrem(An - W, deg * pow(A, static_cast<unsigned long long>(n - 1u)), excess,
                      unused);
        const xuint drop = excess + one;
        if (guard > 4 || drop >= A)
            return xuint();
        A -= drop;
    }
    for (unsigned guard = 0;; ++guard) {
        const xuint z = pow(A + one, static_cast<unsigned long long>(n));
        if (z > W)
            break;
        if (guard > 3)
            return xuint(); // an estimate this far under is not what a seed is for
        A += one;
        An = z;
    }

    const std::size_t slack = std::bit_width(n - 1u) + 3;
    for (; known < k;) {
        const std::size_t p = A.bit_size();
        if (p <= slack)
            return xuint(); // no room left for a step that stays under the curve
        std::size_t b = p - slack;
        if (b > k - known)
            b = k - known; // the last window is the operand itself, which is what makes the
                           // answer exact without a correction over the whole answer
        const xuint D = deg * pow(A, static_cast<unsigned long long>(n - 1u));
        const xuint Wn = v >> (nn * (k - known - b));
        xuint X = (A << b) + ((Wn - (An << (nn * b))) >> (b * (nn - 1))) / D;
        xuint P = pow(X, static_cast<unsigned long long>(n));
        while (P > Wn) { // the second order term, if it ever speaks up, puts the step over
            if (X.is_zero())
                return xuint();
            X -= one;
            P = pow(X, static_cast<unsigned long long>(n));
        }
        // Whether the truncated quotient left the level a step under the root is asked of the
        // remainder rather than of a second power: a step up needs (X+1)^n <= Wn, so a
        // remainder under n * (A<<b)^(n-1) - the gap (X+1)^n - X^n rounded down, which is this
        // level's own divisor shifted into place - rules it out outright.  The widths answer
        // that before any number is built, and at every level measured they answer it with a
        // no, which is why the shift and the loop below sit inside the test.
        xuint Rem = Wn - P;
        const std::size_t shift = b * (nn - 1);
        if (Rem.bit_size() >= D.bit_size() + shift) {
            for (unsigned guard = 0;; ++guard) {
                if (Rem < (D << shift))
                    break;
                const xuint z = X + one;
                const xuint zp = pow(z, static_cast<unsigned long long>(n));
                if (zp > Wn)
                    break;
                if (guard > 15)
                    return xuint();
                X = z;
                P = zp;
                Rem = Wn - P;
            }
        }
        A = X;
        An = P;
        known += b;
    }
    return A;
}

// The same Newton step as the square root, with the buffers kept between the rounds rather
// than rebuilt inside them, and run from an overestimate so that it only ever comes down.
// The bound is how far short of the root the starting estimate may turn out to stand before
// the run is abandoned: the floating point seed is meant to need a correction step or none,
// and the power of two below is what the ladder started from before there was any floating
// point in a seed.  The degree is the odd one the call above peeled down to.
inline xuint iroot_ladder(const xuint& t, unsigned n) {
    const xuint deg(n), step(n - 1u), one(1u);
    xuint p, q, r, y;
    const auto ladder = [&](const xuint& start, unsigned bound) {
        xuint x = start;
        for (;;) {
            p = pow(x, n - 1);
            xuint::divrem(t, p, q, r);
            y = x;
            y *= step;
            y += q;
            y /= deg;
            if (y >= x)
                break;
            x = y;
        }
        unsigned steps = 0;
        while (!x.is_zero() && pow(x, n) > t) {
            if (++steps > bound)
                return std::pair<xuint, bool>(xuint(), false);
            x -= one;
        }
        while (pow(x + one, n) <= t) {
            if (++steps > bound)
                return std::pair<xuint, bool>(xuint(), false);
            x += one;
        }
        return std::pair<xuint, bool>(x, true);
    };
    const std::pair<xuint, bool> seeded = ladder(root_seed(t, n), 4);
    if (seeded.second)
        return seeded.first;
    return ladder(xuint(1u) << ((t.bit_size() + n - 1) / n), ~0u).first;
}

} // namespace detail

// Floor square root together with the remainder, so that v == r*r + rem.
//
// The operand is lifted first, by an even number of bits, until its top limb is whole to
// within one.  A level of the split counts the half it hands down in limbs while the root
// that half stands for is counted in bits, so an operand whose top limb carries leading
// zeros - any value whose bit count is not a whole number of limbs, which is to say
// nearly every value - leaves its candidate standing over the root by a quantity that
// doubles for every two of those zeros.  The level answers that by walking its whole
// bound and handing the call to the ladder, and the caller pays for both.  The split was
// measured quicker than the ladder on operands drawn as a whole number of random limbs,
// and on those it was; but it refused every single root of a logarithm, whose radicands
// are mantissas of one followed by noughts lifted by a shift, and a logarithm is sixty
// four roots.  build\refusescan counts the calls handed back, one per root and seventeen
// wasted steps with them, and none at all once the operand is lifted.
//
// The lift costs a copy of the operand, and the answer comes back by shifting the root
// down: a left shift by a whole number of squares moves the fraction of a root up without
// carrying into its whole part, so the root of the lifted operand holds the root asked
// for in its upper bits and the extra digits of the fraction below them.  The remainder
// is then taken from the answer - one squaring of the root and one subtraction, less than
// the level the lift spares - rather than from the recursion, whose remainder stands under
// a number two to the sh times bigger.  An operand whose top limb is already whole lifts
// by nothing and takes exactly the path it took before.
inline std::pair<xuint, xuint> sqrtrem(const xuint& v) {
    if (v.is_zero())
        return std::make_pair(xuint(), xuint());
    const std::size_t un = v.limb_size();
    const std::size_t rn = un / 2 + 3;
    const detail::limb_t* data = v.limb_data();
    // The zeros the top limb carries.  An even number of them lifts away and leaves the
    // operand a whole number of limbs wide; an odd one lifts all but one, one being what
    // a shift by an even number of bits is bound to leave, and one bit of gap costs a
    // level under two units of excess - which is the walk a level is sized for.
    const std::size_t zeros =
        un * detail::limb_bits - detail::mag_bit_size(data, un);
    const std::size_t sh = (zeros & 1) != 0 ? zeros - 1 : zeros;
    assert(sh <= zeros); // so the lift below cannot carry out of the operand's own span
    const std::size_t room = (sh != 0 ? un + 2 * rn + 8 : 0) +
                             (un > detail::sqrt_base
                                  ? detail::sqrt_scratch_size(un)
                                  : detail::sqrt_ladder_scratch_size(un)) +
                             detail::sqrt_ladder_scratch_size(un) + rn + un + 8;
    const detail::scratch block(room);
    detail::mark m = block.top();
    detail::limb_t* r = m.take(rn);
    detail::limb_t* rem = m.take(un); // the recursion writes its own remainder here, and
                                      // the lifted path overwrites it with the one asked
                                      // for, which is a different quantity
    if (sh != 0) {
        // The lifted operand, in the same limb span, so the shift has nothing to carry
        // out - which is what the assert over `sh` settles.
        detail::limb_t* u = m.take(un);
        detail::lshift(data, un, u, static_cast<unsigned>(sh));
        if (un <= detail::sqrt_base || !detail::sqrt_rec(r, rem, u, un, m))
            detail::sqrt_ladder(r, rem, u, un, m, true);
        detail::rshift(r, r, rn, static_cast<unsigned>(sh / 2));
        const std::size_t sn = detail::mag_size(r, rn);
        detail::limb_t* p = m.take(2 * rn + 2);
        detail::mul_limbs(p, r, sn, r, sn);
        detail::sub_wide(rem, data, un, p, 2 * sn < un ? 2 * sn : un);
        return std::make_pair(xuint::from_limbs(r, rn), xuint::from_limbs(rem, un));
    }
    if (un <= detail::sqrt_base || !detail::sqrt_rec(r, rem, data, un, m))
        // Too narrow to split, or a level declined.  A level declines over the width an
        // operand counts in, which the lift above now settles, and not over the estimate
        // the seed hands it - so the ladder takes the seed here too, and comes down from
        // it rather than from a power of two.
        detail::sqrt_ladder(r, rem, data, un, m, true);
    return std::make_pair(xuint::from_limbs(r, rn), xuint::from_limbs(rem, un));
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
    // A degree with a factor of two peels off as a square root, the split having made
    // the square root the cheap part of the job: the floor of the 2m-th root is the
    // m-th root of the floor of the square root, both being the largest integer whose
    // 2m-th power still fits.  A degree of eight is therefore three square roots and
    // no ladder at all, and a degree of six one square root and the cubic ladder.
    xuint t = v;
    for (; (n & 1u) == 0; n >>= 1)
        t = sqrt(t);
    if (n == 1)
        return t;

    // The levels of detail::iroot_levels take the wide operands, which is where the
    // division of a Newton round is dear enough for their narrower steps to pay; they
    // answer zero for the rest, and for a degree they will not take.
    if (const xuint level = detail::iroot_levels(t, n); !level.is_zero())
        return level;

    // The Newton ladder, kept in a function of its own so that it holds the frame it held
    // before the levels were written: adding the call above to this body and leaving the
    // ladder in it cost the operands the levels decline five percent, where the same source
    // built twice differs by half one (tmp\folds\rootab_fold.txt measured that, and
    // tmp\folds\rootab_noise.txt is what two builds of one source differ by).
    return detail::iroot_ladder(t, n);
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
    // The squarings reduce into themselves, which keeps one pair of division
    // buffers alive for the whole chain.
    xuint q, r;
    for (unsigned i = 1; i < s; ++i) {
        x *= x;
        xuint::divrem(x, n, q, r);
        x = r;
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
            if (spread.limb_size() > 1)
                a = xuint(h) + xuint(2u); // the hash is a limb and n is wider, so
                                          // the remainder would be the hash itself
            else
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
