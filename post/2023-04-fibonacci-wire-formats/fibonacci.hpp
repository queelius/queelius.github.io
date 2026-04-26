// fibonacci.hpp
// Pedagogical implementation for the post "Fibonacci Coding" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (codecs.hpp: Fibonacci)

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fibonacci {

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

// ---- to_zeckendorf -- greedy Zeckendorf decomposition ----------------------
//
// Zeckendorf's theorem: every positive integer n has a unique representation
// as a sum of non-consecutive Fibonacci numbers (F_2=1, F_3=2, F_4=3, F_5=5,
// F_6=8, ...). The greedy algorithm finds this representation by subtracting
// the largest Fibonacci number <= n at each step.
//
// Returns a bit vector where bits[i] == true iff F_{i+2} is in the sum.
// Index 0 corresponds to F_2 = 1, index 1 to F_3 = 2, etc.
//
// Example: to_zeckendorf(4) -> {true, false, true}
//          meaning 4 = F_2 + F_4 = 1 + 3.

inline std::vector<bool> to_zeckendorf(std::uint64_t n) {
    assert(n >= 1 && "Zeckendorf is undefined for n = 0");
    // Build the Fibonacci sequence up to n. Start with F_2=1, F_3=2.
    std::vector<std::uint64_t> fibs{1, 2};
    while (fibs.back() <= n) {
        fibs.push_back(fibs[fibs.size() - 1] + fibs[fibs.size() - 2]);
    }
    // The last entry is > n; remove it so all entries are <= n.
    fibs.pop_back();
    // Greedy decomposition: subtract the largest Fibonacci number <= remaining n.
    std::vector<bool> bits(fibs.size(), false);
    for (std::size_t i = fibs.size(); i-- > 0;) {
        if (n >= fibs[i]) {
            bits[i] = true;
            n -= fibs[i];
        }
    }
    return bits;  // bits[i] = true iff fibs[i] is in the Zeckendorf sum
}

// ---- Fibonacci codec -------------------------------------------------------
//
// Encodes positive integer n >= 1:
//   1. Compute the Zeckendorf representation of n: a bit vector where
//      bits[i] == true iff F_{i+2} is in the sum.
//   2. Write the Zeckendorf bits in order from index 0 (F_2) to the highest
//      set index.
//   3. Append a final '1' as the terminator.
//
// Codeword for n ends in "11" (the highest Zeckendorf bit is always 1, and
// the terminator is 1). No "11" appears elsewhere (Zeckendorf's non-adjacency
// condition).
//
// Examples:
//   1 -> bits={1},     codeword = "1" + "1"    = "11"
//   2 -> bits={0,1},   codeword = "01" + "1"   = "011"
//   3 -> bits={0,0,1}, codeword = "001" + "1"  = "0011"
//   4 -> bits={1,0,1}, codeword = "101" + "1"  = "1011"
//   8 -> bits={0,0,0,0,1}, codeword = "00001" + "1" = "000011"
//
// Decoding: read bits until two consecutive 1s are seen. The second 1 is the
// terminator; all prior bits (including the first 1 of the terminal pair) are
// Zeckendorf bits. Reconstruct n from the Fibonacci sum.
//
// Length: roughly log_phi(n) + 1 bits, where phi = (1+sqrt(5))/2 ~ 1.618.
//         Approximately 1.44 * log2(n) + 1 bits (44% overhead vs entropy).
// Implied prior: p_n ~ phi^{-n} (geometric with golden-ratio base).
// Key property: self-synchronizing via the "11" marker. A single bit flip
//               corrupts at most the codeword it hits and its immediate neighbor.

struct Fibonacci {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Fibonacci is undefined for n = 0");
        auto bits = to_zeckendorf(n);
        // Write Zeckendorf bits in order from F_2 (index 0) outward.
        for (bool b : bits) sink.write(b);
        // Terminating '1' bit.
        sink.write(true);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        // Read bits until we see two consecutive 1s. The second 1 is the
        // terminator. All bits before the terminator (including the first of
        // the terminal pair) are Zeckendorf bits.
        std::vector<bool> bits;
        bool prev = false;
        while (true) {
            bool cur = source.read();
            if (cur && prev) {
                // Two consecutive 1s: the last bit in 'bits' (which is 'prev')
                // and 'cur' (the terminator). Remove the terminator from bits.
                // 'prev' was appended to bits in the previous iteration.
                // The terminator 'cur' is consumed but not appended.
                break;
            }
            bits.push_back(cur);
            prev = cur;
        }
        // Reconstruct n from the Fibonacci sum.
        std::vector<std::uint64_t> fibs{1, 2};
        while (fibs.size() < bits.size()) {
            fibs.push_back(fibs[fibs.size()-1] + fibs[fibs.size()-2]);
        }
        value_type n = 0;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            if (bits[i]) n += fibs[i];
        }
        return n;
    }
};

}  // namespace fibonacci
