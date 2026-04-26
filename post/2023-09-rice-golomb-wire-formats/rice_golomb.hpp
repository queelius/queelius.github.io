// rice_golomb.hpp
// Pedagogical implementation for the post "Rice / Golomb" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (codecs.hpp: Rice<K>, Golomb<M>)

#pragma once

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rice_golomb {

// BitSink and BitSource concepts (minimal local definitions for self-contained
// pedagogical use; production code uses pfc/core.hpp).

template<typename S>
concept BitSink = requires(S& s, bool b) {
    { s.write(b) } -> std::same_as<void>;
};

template<typename S>
concept BitSource = requires(S& s) {
    { s.read() } -> std::same_as<bool>;
};

// ---- Rice<K> -- parametric code for geometric distributions -----------------
//
// Encodes non-negative integer n >= 0 by splitting into quotient q = n >> K
// and remainder r = n & ((1 << K) - 1):
//   1. Write q zeros (the unary-coded quotient, offset by 1: unary(q+1) minus
//      the final '1' is q zeros, then a '1').
//   2. Write a '1' bit (the unary terminator).
//   3. Write the K-bit binary representation of r, MSB first.
//
// Codeword examples for K=2 (r is always 2 bits):
//   n=0: q=0, r=0 -> "1 00"    (3 bits)
//   n=1: q=0, r=1 -> "1 01"    (3 bits)
//   n=4: q=1, r=0 -> "01 00"   (4 bits)
//   n=5: q=1, r=1 -> "01 01"   (4 bits)
//
// Codeword length: (n >> K) + 1 + K bits.
// Kraft sum: sum_{q=0}^{inf} 2^K * 2^{-(q+1+K)} = 1 (saturates).
// Implied prior: geometric with rate parameter p = 2^K / (2^K + 1), tuned by K.
// Optimal source: geometric distribution with mean mu satisfying K ~ log2(mu).
//
// K must satisfy 0 < K < 64. The template parameter K is the number of
// remainder bits (equivalently, the divisor is 2^K).

template<std::size_t K>
struct Rice {
    using value_type = std::uint64_t;
    static_assert(K > 0 && K < 64, "K must be in [1, 63]");

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        std::uint64_t q = n >> K;
        std::uint64_t r = n & ((std::uint64_t{1} << K) - 1);
        // Write q zero bits (quotient in unary-stop-bit form).
        for (std::uint64_t i = 0; i < q; ++i) sink.write(false);
        // Write the stop bit.
        sink.write(true);
        // Write the K-bit remainder, MSB first.
        for (std::size_t i = 0; i < K; ++i) {
            sink.write(((r >> (K - 1 - i)) & 1) != 0);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        // Count zero bits to get q.
        std::uint64_t q = 0;
        while (!source.read()) ++q;
        // Read K remainder bits, MSB first.
        std::uint64_t r = 0;
        for (std::size_t i = 0; i < K; ++i) {
            r = (r << 1) | (source.read() ? std::uint64_t{1} : std::uint64_t{0});
        }
        return (q << K) | r;
    }
};

// ---- truncated_binary -- helper for Golomb's remainder encoding --------------
//
// Encodes r in the range [0, m) using the minimal-length prefix-free code:
//   bits = ceil(log2(m)) = bit_width(m - 1)
//   cutoff = 2^bits - m       (number of values that use bits-1 bits)
//
// If r < cutoff: encode r in (bits - 1) bits.
// If r >= cutoff: encode (r + cutoff) in bits bits.
//
// This packs the m values into a prefix-free binary code of length
// floor(log2(m)) or ceil(log2(m)), splitting the two groups so no codeword
// is a prefix of another.

namespace detail {

// Number of bits needed to represent values in [0, m): ceil(log2(m)).
// For m=1 we need 0 bits (only one value). For m=2 we need 1 bit, etc.
inline std::size_t min_bits(std::size_t m) {
    if (m <= 1) return 0;
    return static_cast<std::size_t>(std::bit_width(m - 1));
}

template<BitSink S>
inline void truncated_binary_encode(std::uint64_t r, std::size_t m, S& sink) {
    assert(r < m);
    if (m == 1) return;  // Zero bits needed for a single value.
    std::size_t bits = min_bits(m);
    std::uint64_t cutoff = (std::uint64_t{1} << bits) - static_cast<std::uint64_t>(m);
    if (r < cutoff) {
        // Encode r in (bits - 1) bits, MSB first.
        for (std::size_t i = bits - 1; i > 0; --i) {
            sink.write(((r >> (i - 1)) & 1) != 0);
        }
    } else {
        // Encode (r + cutoff) in bits bits, MSB first.
        std::uint64_t val = r + cutoff;
        for (std::size_t i = bits; i > 0; --i) {
            sink.write(((val >> (i - 1)) & 1) != 0);
        }
    }
}

template<BitSource S>
inline std::uint64_t truncated_binary_decode(std::size_t m, S& source) {
    if (m == 1) return 0;
    std::size_t bits = min_bits(m);
    std::uint64_t cutoff = (std::uint64_t{1} << bits) - static_cast<std::uint64_t>(m);
    // Read (bits - 1) bits first.
    std::uint64_t val = 0;
    for (std::size_t i = 0; i < bits - 1; ++i) {
        val = (val << 1) | (source.read() ? std::uint64_t{1} : std::uint64_t{0});
    }
    if (val < cutoff) {
        // The short codeword: r = val, consumed (bits - 1) bits.
        return val;
    } else {
        // Need one more bit to distinguish.
        val = (val << 1) | (source.read() ? std::uint64_t{1} : std::uint64_t{0});
        return val - cutoff;
    }
}

}  // namespace detail

// ---- Golomb<M> -- generalized Rice for non-power-of-2 divisors --------------
//
// Encodes non-negative integer n >= 0 by splitting into quotient q = n / m
// and remainder r = n % m:
//   1. Write q in unary (q zeros then a '1' bit).
//   2. Write r in truncated binary (prefix-free code over [0, m)).
//
// When M is a power of 2 (M = 2^K), Golomb<M> produces identical codewords
// to Rice<K>. For non-power-of-2 M, the truncated-binary remainder is
// slightly more efficient than a fixed K-bit field.
//
// Codeword length: floor(n/M) + 1 + (floor(log2(M)) or ceil(log2(M))) bits.
// Implied prior: geometric with rate parameter p tuned to mean ~ M / (1 - p).
// Optimal source: geometric distribution with mean close to M.

template<std::size_t M>
struct Golomb {
    using value_type = std::uint64_t;
    static_assert(M >= 1, "M must be at least 1");

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        std::uint64_t q = n / static_cast<std::uint64_t>(M);
        std::uint64_t r = n % static_cast<std::uint64_t>(M);
        // Write q in unary: q zeros then a '1'.
        for (std::uint64_t i = 0; i < q; ++i) sink.write(false);
        sink.write(true);
        // Write r in truncated binary.
        detail::truncated_binary_encode(r, M, sink);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        // Count zero bits to get q.
        std::uint64_t q = 0;
        while (!source.read()) ++q;
        // Decode r from truncated binary.
        std::uint64_t r = detail::truncated_binary_decode(M, source);
        return q * static_cast<std::uint64_t>(M) + r;
    }
};

// ---- Parameter selection (Gallager and van Voorhis 1975) --------------------
//
// For a geometric distribution with mean mu (i.e., p = 1/mu if the distribution
// is over non-negative integers), the optimal Golomb parameter is approximately:
//
//   m* = -1 / log2((mu - 1) / mu)  =  -1 / log2(1 - 1/mu)
//
// For Rice (which requires m = 2^K), round m* to the nearest power of 2 and
// return K = round(log2(m*)).
//
// Both functions clamp their results to sensible ranges to avoid degenerate
// outputs from extreme or near-zero mean values.

// optimal_golomb_m: returns the approximately optimal Golomb parameter m
// for a geometric source with the given mean (mean > 1).
inline std::size_t optimal_golomb_m(double mean) {
    assert(mean > 1.0 && "Golomb parameter undefined for mean <= 1");
    // Gallager-van Voorhis formula.
    double p = (mean - 1.0) / mean;  // geometric success probability
    // m* = -1 / log2(p)
    double m_star = -1.0 / std::log2(p);
    std::size_t m = static_cast<std::size_t>(std::round(m_star));
    if (m < 1) m = 1;
    return m;
}

// optimal_rice_k: returns the approximately optimal Rice parameter K
// for a geometric source with the given mean (mean >= 1).
// Rice<K> is optimal for a geometric source with mean close to 2^K.
// K = round(log2(mean)), clamped to [1, 62].
inline std::size_t optimal_rice_k(double mean) {
    if (mean <= 2.0) return 1;  // Clamp: K must be >= 1.
    // K ~ log2(mean): 2^K is the nearest power of 2 to the mean.
    double k_real = std::log2(mean);
    std::size_t k = static_cast<std::size_t>(std::max(1.0, std::round(k_real)));
    if (k >= 63) k = 62;  // Clamp to valid Rice template range.
    return k;
}

}  // namespace rice_golomb
