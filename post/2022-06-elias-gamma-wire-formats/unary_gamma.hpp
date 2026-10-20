// unary_gamma.hpp
// Pedagogical implementation for the post "Unary and Elias Gamma" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (codecs.hpp: Unary, EliasGamma)

#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace unary_gamma {

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

// ---- Unary -- the simplest universal code -----------------------------------
//
// Encodes positive integer n >= 1 as (n-1) zero bits followed by one '1' bit.
// Examples: 1 -> "1", 2 -> "01", 3 -> "001", 4 -> "0001".
//
// Length of codeword for n: n bits.
// Kraft sum: sum_{n=1}^{inf} 2^{-n} = 1 (saturates).
// Implied prior: p_n = 2^{-n} (geometric distribution with parameter 1/2).
// Optimal source: geometric(1/2), i.e., each value is half as likely as the
//                 previous. Unary achieves entropy exactly on this prior.

struct Unary {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Unary is undefined for n = 0");
        for (value_type i = 1; i < n; ++i) sink.write(false);
        sink.write(true);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        value_type n = 1;
        while (!source.read()) ++n;
        return n;
    }
};

// ---- Gamma -- Elias gamma code (Peter Elias, 1975) -------------------------
//
// Encodes positive integer n >= 1:
//   1. Write floor(log2(n)) zero bits.
//   2. Write a '1' bit.
//   3. Write the binary representation of n minus its leading 1 bit (MSB first),
//      using floor(log2(n)) bits.
//
// Examples: 1->"1", 2->"010", 3->"011", 4->"00100", 5->"00101",
//           6->"00110", 7->"00111", 8->"0001000".
//
// Length: 2*floor(log2(n)) + 1 bits.
// Kraft sum: sum_{k=0}^{inf} 2^k * 2^{-(2k+1)} = sum_{k=0}^{inf} 2^{-(k+1)} = 1.
// Implied prior: p_n = 2^{-(2*floor(log2(n))+1)}, approximately 1/(2n^2).
// Optimal source: power-law with exponent ~2 (e.g., word frequencies).

struct Gamma {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Gamma is undefined for n = 0");
        // k = floor(log2(n)): number of leading zeros and number of trailing bits.
        std::size_t k = std::bit_width(n) - 1;
        // Write k zeros.
        for (std::size_t i = 0; i < k; ++i) sink.write(false);
        // Write the one separator bit.
        sink.write(true);
        // Write the k trailing bits of n (after the implicit leading 1), MSB first.
        for (std::size_t i = k; i > 0; --i) {
            sink.write(((n >> (i - 1)) & 1u) != 0u);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        // Count leading zeros to get k.
        std::size_t k = 0;
        while (!source.read()) ++k;
        // Read k more bits to reconstruct n (starting from the implicit leading 1).
        value_type n = 1;
        for (std::size_t i = 0; i < k; ++i) {
            n = (n << 1) | (source.read() ? value_type{1} : value_type{0});
        }
        return n;
    }
};

// ---- Length-vector generators (pedagogical helpers) ------------------------
//
// These generate the codeword-length vectors for Unary and Gamma for use
// with the priors library (implied_prior, entropy, expected_length, redundancy).

// unary_lengths(K): lengths for symbols 1..K under Unary coding.
// l_n = n for n = 1..K.
inline std::vector<std::size_t> unary_lengths(std::size_t K) {
    std::vector<std::size_t> v(K);
    for (std::size_t i = 0; i < K; ++i) v[i] = i + 1;
    return v;
}

// gamma_lengths(N): lengths for symbols 1..N under Gamma coding.
// l_n = 2*floor(log2(n)) + 1.
inline std::vector<std::size_t> gamma_lengths(std::size_t N) {
    std::vector<std::size_t> v(N);
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t n = i + 1;
        std::size_t k = std::bit_width(n) - 1;  // floor(log2(n))
        v[i] = 2 * k + 1;
    }
    return v;
}

}  // namespace unary_gamma
