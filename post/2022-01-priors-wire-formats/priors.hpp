// priors.hpp
// Pedagogical implementation for the post "Universal Codes as Priors" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc

#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace priors {

// ---- implied_prior -- the core correspondence: lengths -> probabilities ------
//
// For a prefix-free code with codeword lengths (l_1, ..., l_n), the implicit
// probability of symbol i is 2^{-l_i}. If the lengths do not saturate Kraft
// (sum < 1), normalize so the probabilities sum to 1.
//
// This is the inverse of Shannon's prescription: given p_i, the optimal length
// is -log2(p_i). Going backward: given a length l_i, the implied probability is
// 2^{-l_i}.

inline std::vector<double> implied_prior(const std::vector<std::size_t>& lengths) {
    std::vector<double> probs;
    probs.reserve(lengths.size());
    double total = 0.0;
    for (std::size_t l : lengths) {
        double p = std::ldexp(1.0, -static_cast<int>(l));
        probs.push_back(p);
        total += p;
    }
    // Normalize if Kraft sum is less than 1.
    if (total < 1.0) {
        for (double& p : probs) p /= total;
    }
    return probs;
}

// ---- entropy -- Shannon entropy of a distribution --------------------------
//
// H(p) = -sum_i p_i * log2(p_i)
// Zero-probability symbols contribute 0 (by convention: 0 * log 0 = 0).

inline double entropy(const std::vector<double>& probs) {
    double h = 0.0;
    for (double p : probs) {
        if (p > 0.0) h -= p * std::log2(p);
    }
    return h;
}

// ---- expected_length -- average codeword length under a distribution --------
//
// L(p, l) = sum_i p_i * l_i
// This is the expected bits-per-symbol when encoding from distribution p
// using a code with lengths l.

inline double expected_length(const std::vector<double>& probs,
                              const std::vector<std::size_t>& lengths) {
    assert(probs.size() == lengths.size());
    double L = 0.0;
    for (std::size_t i = 0; i < probs.size(); ++i) {
        L += probs[i] * static_cast<double>(lengths[i]);
    }
    return L;
}

// ---- redundancy -- excess bits beyond Shannon optimum ----------------------
//
// R(p, l) = L(p, l) - H(p) >= 0  (Shannon's source-coding theorem).
// Equality holds iff l_i = -log2(p_i) for all i and the Kraft sum equals 1.

inline double redundancy(const std::vector<double>& probs,
                         const std::vector<std::size_t>& lengths) {
    return expected_length(probs, lengths) - entropy(probs);
}

}  // namespace priors
