// mcmillan.hpp
// Pedagogical implementation for the post "McMillan's Converse" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace mcmillan {

// ---- build_prefix_free_code -- the McMillan construction --------------------
//
// Given codeword lengths (l_1, ..., l_n) satisfying Kraft's inequality, this
// function produces a prefix-free code with those exact lengths.
//
// Algorithm (Shannon-Fano-like, walking the trie left-to-right):
//   1. Sort lengths in non-decreasing order.
//   2. Start with the integer counter c = 0.
//   3. For each length l_i in the sorted order:
//      - Emit the binary representation of c, left-padded to l_i bits.
//      - Increment c by 2^{l_max - l_i}, where l_max is the maximum length.
//   4. Return the code (in the original length order).
//
// The increment ensures the next codeword starts at the next available leaf
// in the depth-l_max trie. Kraft's inequality guarantees the increments fit
// within 2^l_max.

inline std::string to_binary(std::uint64_t value, std::size_t width) {
    std::string s(width, '0');
    for (std::size_t i = 0; i < width; ++i) {
        if (value & (std::uint64_t{1} << (width - 1 - i))) {
            s[i] = '1';
        }
    }
    return s;
}

inline std::vector<std::string> build_prefix_free_code(
    const std::vector<std::size_t>& lengths)
{
    if (lengths.empty()) return {};

    std::vector<std::pair<std::size_t, std::size_t>> indexed;
    indexed.reserve(lengths.size());
    for (std::size_t i = 0; i < lengths.size(); ++i) {
        indexed.emplace_back(lengths[i], i);
    }
    std::sort(indexed.begin(), indexed.end());

    std::size_t l_max = indexed.back().first;
    std::vector<std::string> code(lengths.size());
    std::uint64_t counter = 0;
    for (const auto& [l, original_idx] : indexed) {
        std::uint64_t shifted = counter >> (l_max - l);
        code[original_idx] = to_binary(shifted, l);
        counter += std::uint64_t{1} << (l_max - l);
    }
    return code;
}

// ---- is_prefix_free -- verification helper ----------------------------------
//
// Returns true iff no codeword in the input vector is a prefix of any other.
// Useful for verifying that build_prefix_free_code produces valid output.

inline bool is_prefix_free(const std::vector<std::string>& code) {
    for (std::size_t i = 0; i < code.size(); ++i) {
        for (std::size_t j = 0; j < code.size(); ++j) {
            if (i == j) continue;
            const std::string& a = code[i];
            const std::string& b = code[j];
            if (a.size() > b.size()) continue;
            if (b.compare(0, a.size(), a) == 0) return false;
        }
    }
    return true;
}

}  // namespace mcmillan
