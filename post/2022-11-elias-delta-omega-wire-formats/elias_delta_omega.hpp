// elias_delta_omega.hpp
// Pedagogical implementation for the post "Elias Delta and Omega" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (codecs.hpp: EliasDelta, EliasOmega)
//
// Loose-coupling note: Gamma is re-implemented here rather than included
// from unary_gamma.hpp. Each post's header stands alone.

#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stack>
#include <utility>
#include <vector>

namespace elias_delta_omega {

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

// ---- Gamma -- Elias gamma (re-implemented locally for loose coupling) --------
//
// Encodes positive integer n >= 1:
//   1. Write floor(log2(n)) zero bits.
//   2. Write a '1' bit.
//   3. Write the floor(log2(n)) trailing bits of n (after the implicit leading
//      1 bit), MSB first.
//
// This is identical to the Gamma in post 4's unary_gamma.hpp. Re-implemented
// here so this header stands alone without a sibling-directory dependency.
//
// Length: 2*floor(log2(n)) + 1 bits.
// Implied prior: approximately 1/(2n^2).

struct Gamma {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Gamma is undefined for n = 0");
        std::size_t k = std::bit_width(n) - 1;  // floor(log2(n))
        for (std::size_t i = 0; i < k; ++i) sink.write(false);
        sink.write(true);
        for (std::size_t i = k; i > 0; --i) {
            sink.write(((n >> (i - 1)) & 1u) != 0u);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t k = 0;
        while (!source.read()) ++k;
        value_type n = 1;
        for (std::size_t i = 0; i < k; ++i) {
            n = (n << 1) | (source.read() ? value_type{1} : value_type{0});
        }
        return n;
    }
};

// ---- Delta -- Elias delta code (Peter Elias, 1975) --------------------------
//
// Encodes positive integer n >= 1:
//   Let L = floor(log2(n)) + 1  (the bit-width of n, equivalently bit_width(n)).
//   1. Encode L in Gamma.
//   2. Write the (L-1) trailing bits of n after its leading 1, MSB first.
//
// Gamma encodes L (a small integer) efficiently, replacing the linear-sized
// unary length prefix in Gamma's own encoding of n.
//
// Examples:
//   1 -> L=1, Gamma(1)="1", no trailing bits         -> "1"      (1 bit)
//   2 -> L=2, Gamma(2)="010", trailing bits="0"       -> "0100"   (4 bits)
//   3 -> L=2, Gamma(2)="010", trailing bits="1"       -> "0101"   (4 bits)
//   4 -> L=3, Gamma(3)="011", trailing bits="00"      -> "01100"  (5 bits)
//
// Length: L_delta(n) = L_gamma(bit_width(n)) + (bit_width(n) - 1)
//         = O(log n + log log n) bits.
// Implied prior: approximately 1 / (n * log^2(n)), heavier tail than Gamma.

struct Delta {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1);
        std::size_t bits = std::bit_width(n);  // L = floor(log2(n)) + 1
        Gamma::encode(static_cast<value_type>(bits), sink);
        // Write the (bits - 1) trailing bits of n, MSB first, skipping the
        // implicit leading 1.
        for (std::size_t i = bits - 1; i > 0; --i) {
            sink.write(((n >> (i - 1)) & 1u) != 0u);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t bits = static_cast<std::size_t>(Gamma::decode(source));
        // The leading 1 is implicit; read the remaining (bits - 1) trailing bits.
        value_type result = 1;
        for (std::size_t i = 1; i < bits; ++i) {
            result = (result << 1) | (source.read() ? value_type{1} : value_type{0});
        }
        return result;
    }
};

// ---- Omega -- Elias omega code (Peter Elias, 1975) --------------------------
//
// Omega encodes n >= 1 by recursively encoding the bit-width of each level
// until the value reaches 1. The recursion unwinds by stacking the binary
// representations, then writing them from outermost (largest) to innermost
// (smallest), followed by a terminating 0 bit.
//
// Encoding algorithm (iterative with a stack):
//   1. While n > 1:
//        a. Push (n, bit_width(n)) onto the stack.
//        b. Replace n with (bit_width(n) - 1).   // the "length minus 1" step
//   2. Pop the stack in reverse order (top first), writing each value in its
//      full binary representation.
//   3. Write a terminating 0 bit.
//
// Decoding algorithm (forward):
//   1. Start with current = 1.
//   2. Peek at the next bit:
//        - If 0 (terminator): stop. Return current.
//        - If 1: read (current + 1) bits (including the peeked 1) to get next.
//                Set current = next. Repeat.
//
// Length: O(log* n) asymptotically (iterated logarithm).
// Implied prior: slightly heavier tail than Delta's 1/(n log^2 n).
// Practical note: Omega is the theoretical limit of the recursion. Delta is
// preferred in practice because the Omega overhead dominates for practical n.

struct Omega {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1);
        // Build the stack bottom-up: each frame is the value to write plus its
        // bit-width. The recursion terminates when n reaches 1.
        std::vector<std::pair<value_type, std::size_t>> stack;
        while (n > 1) {
            std::size_t w = static_cast<std::size_t>(std::bit_width(n));
            stack.emplace_back(n, w);
            n = static_cast<value_type>(w - 1);  // encode (bit_width - 1) next
        }
        // Write in reverse (outermost frame first), each value MSB-first.
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto [val, width] = *it;
            for (std::size_t i = width; i > 0; --i) {
                sink.write(((val >> (i - 1)) & 1u) != 0u);
            }
        }
        // Terminating 0 bit.
        sink.write(false);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        value_type current = 1;
        // The next bit tells us whether more data follows.
        // We must peek: read the bit, then decide.
        // Implementation: attempt to read the next bit directly.
        // If it is 0, we stop. If it is 1, it is the MSB of the next field,
        // which has (current + 1) bits total (including this one).
        while (true) {
            bool b = source.read();
            if (!b) {
                // Terminating 0: current holds the decoded value.
                break;
            }
            // b == 1: this is the MSB of a (current + 1)-bit value.
            value_type next = 1;  // implicit leading 1 (the bit we just read)
            for (value_type i = 0; i < current; ++i) {
                next = (next << 1) | (source.read() ? value_type{1} : value_type{0});
            }
            current = next;
        }
        return current;
    }
};

}  // namespace elias_delta_omega
