// synthesis.hpp
// Pedagogical implementation for the post "Synthesis: Codecs as Structure" in the
// "Algebra over Wire Formats" series. For the production library, see PFC:
// https://github.com/queelius/pfc

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace synthesis {

// ---- empirical_distribution -- estimate distribution from a sample ----------
//
// Counts occurrences of each value in the sample, then normalizes by the
// total count to produce a probability distribution.
//
// Returns a map from value to estimated probability. All probabilities are
// positive (zero-count values are not included) and sum to 1.

inline std::map<std::uint64_t, double>
empirical_distribution(const std::vector<std::uint64_t>& sample)
{
    assert(!sample.empty() && "Cannot estimate distribution from empty sample");
    std::map<std::uint64_t, std::size_t> counts;
    for (std::uint64_t v : sample) ++counts[v];
    std::map<std::uint64_t, double> dist;
    double total = static_cast<double>(sample.size());
    for (const auto& [v, c] : counts) {
        dist[v] = static_cast<double>(c) / total;
    }
    return dist;
}

// ---- entropy_of -- Shannon entropy of a distribution map -------------------
//
// H(p) = -sum_v p(v) * log2(p(v))
// Symbols with zero probability are skipped (0 * log 0 = 0 by convention).

inline double entropy_of(const std::map<std::uint64_t, double>& dist)
{
    double h = 0.0;
    for (const auto& [v, p] : dist) {
        if (p > 0.0) h -= p * std::log2(p);
    }
    return h;
}

// ---- length_for -- analytical codeword length for named universal codes -----
//
// Returns the number of bits a named code would use to encode value n >= 1.
// Supports: "Unary", "Gamma", "Delta", "Omega", "Fibonacci", "VByte".
//
// Length formulas:
//   Unary:     n bits.
//   Gamma:     2*floor(log2(n)) + 1.
//   Delta:     floor(log2(n)) + 2*floor(log2(floor(log2(n))+1)) + 1.
//   Omega:     iterated: encode each level's length until we reach 1.
//   Fibonacci: floor(log_phi(n)) + 2  (number of Zeckendorf bits + 1 terminator).
//   VByte:     8 * ceil(max(1, ceil(log2(n+1))) / 7.0).

inline std::size_t length_for(std::string_view code_name, std::uint64_t n)
{
    if (code_name == "Unary") {
        assert(n >= 1 && "Unary is undefined for n=0");
        return static_cast<std::size_t>(n);
    }

    if (code_name == "Gamma") {
        assert(n >= 1 && "Gamma is undefined for n=0");
        // k = floor(log2(n)) computed via bit_width.
        std::size_t k = 0;
        std::uint64_t tmp = n;
        while (tmp > 1) { tmp >>= 1; ++k; }
        return 2 * k + 1;
    }

    if (code_name == "Delta") {
        assert(n >= 1 && "Delta is undefined for n=0");
        // k = floor(log2(n)).
        std::size_t k = 0;
        std::uint64_t tmp = n;
        while (tmp > 1) { tmp >>= 1; ++k; }
        // L = k + 1 = floor(log2(n)) + 1 (the bit-width of n).
        // Delta encodes L in Gamma: gamma_len(L) = 2*floor(log2(L)) + 1.
        std::size_t L = k + 1;
        std::size_t kk = 0;
        std::size_t tmpL = L;
        while (tmpL > 1) { tmpL >>= 1; ++kk; }
        std::size_t gamma_len_of_L = 2 * kk + 1;
        // Total: gamma_len(L) + (L - 1) trailing bits.
        return gamma_len_of_L + (L - 1);
    }

    if (code_name == "Omega") {
        // Elias omega: encode n, then encode floor(log2(n))+1 in omega, etc.
        // Iterative computation: push each level's bit-width, sum.
        assert(n >= 1 && "Omega is undefined for n=0");
        std::size_t total_bits = 0;
        std::uint64_t cur = n;
        while (cur > 1) {
            // This level contributes bit_width(cur) bits.
            std::size_t w = 0;
            std::uint64_t tmp2 = cur;
            while (tmp2 > 1) { tmp2 >>= 1; ++w; }
            total_bits += w + 1; // w trailing bits + 1 length bit
            cur = w;             // next level encodes the width
        }
        // The base case (cur == 1) contributes 1 bit ("1" terminator in Omega).
        total_bits += 1;
        return total_bits;
    }

    if (code_name == "Fibonacci") {
        // Zeckendorf length: floor(log_phi(n * sqrt(5) + 0.5)) + 1 terminator bit.
        // Implemented via direct Fibonacci enumeration (exact, no float).
        assert(n >= 1 && "Fibonacci is undefined for n=0");
        // Generate Fibonacci numbers up to n.
        std::vector<std::uint64_t> fibs = {1, 2};
        while (fibs.back() <= n) {
            fibs.push_back(fibs[fibs.size()-1] + fibs[fibs.size()-2]);
        }
        if (fibs.back() > n) fibs.pop_back();
        // Number of Zeckendorf bits = size of fibs vector, since the greedy
        // decomposition occupies at most one bit per Fibonacci number.
        // Length = number of Fibonacci numbers considered + 1 terminator.
        // (This is an upper bound; it equals the actual codeword length.)
        return fibs.size() + 1;
    }

    if (code_name == "VByte") {
        // Each byte holds 7 bits of payload; final byte has MSB=0.
        // Length in bits = 8 * number of bytes needed.
        // Number of bytes = ceil(bit_width(n+1) / 7), minimum 1.
        if (n == 0) return 8;  // 1 byte for 0.
        std::uint64_t val = n;
        std::size_t bytes = 0;
        do {
            val >>= 7;
            ++bytes;
        } while (val > 0);
        return 8 * bytes;
    }

    assert(false && "Unknown code name");
    return 0;
}

// ---- redundancy_for -- expected codeword length minus entropy ---------------
//
// Computes the redundancy of a named code on a given empirical distribution:
//   R = sum_v dist(v) * length_for(code, v) - entropy_of(dist)
//
// Redundancy is always >= 0 (Shannon's source-coding theorem). A lower value
// means the code's implied prior is a closer match to the actual source.

inline double redundancy_for(std::string_view code_name,
                             const std::map<std::uint64_t, double>& dist)
{
    double expected_len = 0.0;
    for (const auto& [v, p] : dist) {
        expected_len += p * static_cast<double>(length_for(code_name, v));
    }
    return expected_len - entropy_of(dist);
}

// ---- recommend_code -- select the universal code with minimum redundancy ----
//
// Given a sample of positive integers, estimates the empirical distribution,
// computes the redundancy of each candidate universal code, and returns the
// name of the code with the smallest redundancy.
//
// Candidates: Unary, Gamma, Delta, Omega, Fibonacci, VByte.
// Huffman and Arithmetic are not candidates because they require the
// distribution as input rather than a sample, and they are not universal codes
// in the same sense.
//
// The function makes the code-selection process concrete: there is no "best
// code in general," but there is a best code given a sample.

inline std::string recommend_code(const std::vector<std::uint64_t>& sample)
{
    auto dist = empirical_distribution(sample);
    constexpr std::string_view candidates[] = {
        "Unary", "Gamma", "Delta", "Omega", "Fibonacci", "VByte"
    };
    double best_redundancy = std::numeric_limits<double>::infinity();
    std::string best_code;
    for (std::string_view candidate : candidates) {
        double r = redundancy_for(candidate, dist);
        if (r < best_redundancy) {
            best_redundancy = r;
            best_code = std::string(candidate);
        }
    }
    return best_code;
}

}  // namespace synthesis
