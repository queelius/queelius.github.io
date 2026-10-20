#include <gtest/gtest.h>
#include <cstdint>
#include <numeric>
#include <vector>
#include "fibonacci.hpp"

using namespace fibonacci;

// ---- Zeckendorf tests -------------------------------------------------------

// to_zeckendorf(n) returns a bit vector where bits[i] == true iff F_{i+2} is
// in the Zeckendorf sum. Index 0 corresponds to F_2 = 1, index 1 to F_3 = 2,
// index 2 to F_4 = 3, etc.

// Helper: reconstruct n from its Zeckendorf bits.
static uint64_t from_zeckendorf(const std::vector<bool>& bits) {
    std::vector<uint64_t> fibs{1, 2};
    while (fibs.size() < bits.size()) {
        fibs.push_back(fibs[fibs.size()-1] + fibs[fibs.size()-2]);
    }
    uint64_t n = 0;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) n += fibs[i];
    }
    return n;
}

// Helper: verify no two consecutive bits are true (Zeckendorf uniqueness).
static bool no_consecutive_ones(const std::vector<bool>& bits) {
    for (std::size_t i = 0; i + 1 < bits.size(); ++i) {
        if (bits[i] && bits[i+1]) return false;
    }
    return true;
}

// Spot-check: known Zeckendorf representations.
// 1 = F_2                     -> bits = {1}
// 2 = F_3                     -> bits = {0, 1}
// 3 = F_4                     -> bits = {0, 0, 1}
// 4 = F_4 + F_2 = 3+1         -> bits = {1, 0, 1}
// 10 = F_6 + F_3 = 8+2        -> bits = {0, 1, 0, 0, 1}
// 11 = F_6 + F_4 = 8+3        -> bits = {0, 0, 1, 0, 1}
TEST(FibonacciTest, ZeckendorfSpotCheck1) {
    auto b = to_zeckendorf(1u);
    ASSERT_GE(b.size(), 1u);
    EXPECT_EQ(b[0], true);
    EXPECT_EQ(from_zeckendorf(b), 1u);
}

TEST(FibonacciTest, ZeckendorfSpotCheck4) {
    auto b = to_zeckendorf(4u);
    // 4 = 3 + 1 = F_4 + F_2: bits[0]=1 (F_2), bits[1]=0 (F_3), bits[2]=1 (F_4).
    ASSERT_GE(b.size(), 3u);
    EXPECT_EQ(b[0], true);
    EXPECT_EQ(b[1], false);
    EXPECT_EQ(b[2], true);
    EXPECT_EQ(from_zeckendorf(b), 4u);
}

TEST(FibonacciTest, ZeckendorfSpotCheck10) {
    auto b = to_zeckendorf(10u);
    EXPECT_EQ(from_zeckendorf(b), 10u);
    EXPECT_TRUE(no_consecutive_ones(b));
}

// Round-trip: for all n in 1..200, from_zeckendorf(to_zeckendorf(n)) == n.
TEST(FibonacciTest, ZeckendorfRoundTrip) {
    for (uint64_t n = 1; n <= 200; ++n) {
        auto b = to_zeckendorf(n);
        EXPECT_EQ(from_zeckendorf(b), n) << "n=" << n;
    }
}

// Non-consecutive: Zeckendorf bits never have two adjacent 1s.
TEST(FibonacciTest, ZeckendorfNoConsecutiveOnes) {
    for (uint64_t n = 1; n <= 200; ++n) {
        EXPECT_TRUE(no_consecutive_ones(to_zeckendorf(n))) << "n=" << n;
    }
}

// Uniqueness: to_zeckendorf should always return the same result for the same n.
TEST(FibonacciTest, ZeckendorfDeterministic) {
    for (uint64_t n = 1; n <= 50; ++n) {
        EXPECT_EQ(to_zeckendorf(n), to_zeckendorf(n)) << "n=" << n;
    }
}

// ---- Fibonacci codec tests --------------------------------------------------

// Minimal in-memory BitSink/BitSource for tests.
struct BitBuffer {
    std::vector<bool> bits;
    void write(bool b) { bits.push_back(b); }
    bool read() {
        bool b = bits[pos_];
        ++pos_;
        return b;
    }
    std::size_t pos_ = 0;
};

static uint64_t fib_round_trip(uint64_t n) {
    BitBuffer buf;
    Fibonacci::encode(n, buf);
    buf.pos_ = 0;
    return Fibonacci::decode(buf);
}

static std::size_t fib_bit_count(uint64_t n) {
    BitBuffer buf;
    Fibonacci::encode(n, buf);
    return buf.bits.size();
}

TEST(FibonacciTest, FibonacciRoundTrip) {
    for (uint64_t n = 1; n <= 200; ++n) {
        EXPECT_EQ(fib_round_trip(n), n) << "n=" << n;
    }
}

TEST(FibonacciTest, FibonacciRoundTripLarge) {
    for (uint64_t n : {uint64_t{1000}, uint64_t{10000}, uint64_t{100000}}) {
        EXPECT_EQ(fib_round_trip(n), n) << "n=" << n;
    }
}

// Spot-check known codewords from the spec:
// 1 -> "11" (F_2 bit + terminator)
// 2 -> "011" (F_3 bit + terminator: bits={0,1}, append 1)
// 3 -> "0011" (F_4: bits={0,0,1}, append 1)
// 4 -> "1011" (F_2+F_4: bits={1,0,1}, append 1)
// 8 -> "000011" (F_6: bits={0,0,0,0,1}, append 1)

TEST(FibonacciTest, FibonacciEncoding1Is11) {
    BitBuffer buf;
    Fibonacci::encode(uint64_t{1}, buf);
    ASSERT_EQ(buf.bits.size(), 2u);
    EXPECT_EQ(buf.bits[0], true);   // F_2 bit
    EXPECT_EQ(buf.bits[1], true);   // terminator
}

TEST(FibonacciTest, FibonacciEncoding2Is011) {
    BitBuffer buf;
    Fibonacci::encode(uint64_t{2}, buf);
    ASSERT_EQ(buf.bits.size(), 3u);
    EXPECT_EQ(buf.bits[0], false);  // F_2 bit = 0
    EXPECT_EQ(buf.bits[1], true);   // F_3 bit = 1
    EXPECT_EQ(buf.bits[2], true);   // terminator
}

TEST(FibonacciTest, FibonacciEncoding4Is1011) {
    BitBuffer buf;
    Fibonacci::encode(uint64_t{4}, buf);
    ASSERT_EQ(buf.bits.size(), 4u);
    EXPECT_EQ(buf.bits[0], true);   // F_2=1 bit
    EXPECT_EQ(buf.bits[1], false);  // F_3=2 bit
    EXPECT_EQ(buf.bits[2], true);   // F_4=3 bit
    EXPECT_EQ(buf.bits[3], true);   // terminator
}

// Every codeword ends in "11" (Zeckendorf bits followed by terminator '1').
// The last Zeckendorf bit is always 1 (it is the highest Fibonacci in the sum).
TEST(FibonacciTest, AllCodewordsEndIn11) {
    for (uint64_t n = 1; n <= 100; ++n) {
        BitBuffer buf;
        Fibonacci::encode(n, buf);
        std::size_t len = buf.bits.size();
        ASSERT_GE(len, 2u) << "n=" << n;
        // The last two bits must both be 1.
        EXPECT_EQ(buf.bits[len - 1], true) << "terminator missing for n=" << n;
        EXPECT_EQ(buf.bits[len - 2], true) << "last Zeckendorf bit not 1 for n=" << n;
    }
}

// No codeword contains "11" except at the very end.
TEST(FibonacciTest, NoInternalConsecutiveOnes) {
    for (uint64_t n = 1; n <= 100; ++n) {
        BitBuffer buf;
        Fibonacci::encode(n, buf);
        std::size_t len = buf.bits.size();
        // Check all pairs except the final pair (which is the "11" terminator).
        for (std::size_t i = 0; i + 2 < len; ++i) {
            EXPECT_FALSE(buf.bits[i] && buf.bits[i+1])
                << "Internal '11' at position " << i << " for n=" << n;
        }
    }
}

// ---- Self-synchronization test (spec section E) ----------------------------

// Helper: encode a sequence of integers into one flat bit stream.
static std::vector<bool> encode_sequence(const std::vector<uint64_t>& seq) {
    struct VecSink {
        std::vector<bool>& bits;
        void write(bool b) { bits.push_back(b); }
    };
    std::vector<bool> result;
    VecSink sink{result};
    for (uint64_t n : seq) Fibonacci::encode(n, sink);
    return result;
}

// Helper: flip bit at index idx in a copy of the bit stream.
static std::vector<bool> flip_bit(std::vector<bool> bits, std::size_t idx) {
    bits[idx] = !bits[idx];
    return bits;
}

// Helper: try to decode at most one integer from a bit stream with an EOF guard.
// Returns false if we run out of bits before finding the "11" terminator.
static bool try_decode_one(const std::vector<bool>& bits, std::size_t& pos,
                            uint64_t& out) {
    std::vector<bool> zeck;
    bool prev = false;
    while (pos < bits.size()) {
        bool cur = bits[pos++];
        if (cur && prev) { out = 0; /* compute below */ goto reconstruct; }
        zeck.push_back(cur);
        prev = cur;
    }
    return false;  // ran out of bits
reconstruct:
    std::vector<uint64_t> fibs{1, 2};
    while (fibs.size() < zeck.size())
        fibs.push_back(fibs[fibs.size()-1] + fibs[fibs.size()-2]);
    out = 0;
    for (std::size_t i = 0; i < zeck.size(); ++i)
        if (zeck[i]) out += fibs[i];
    return true;
}

// Spec section E: a single bit flip corrupts at most two codewords.
// We encode {3, 5, 7, 9, 11, 13}, flip the first bit in the stream
// (within the codeword for 3), and verify that the first codeword may be
// wrong but subsequent ones are recovered.
// The key property: after a corrupted codeword, the next "11" resynchronizes.
TEST(FibonacciTest, BitFlipStaysLocal) {
    const std::vector<uint64_t> original = {3, 5, 7, 9, 11, 13};
    auto encoded = encode_sequence(original);
    // Flip the very first bit (within the codeword for 3 = "0011 1").
    // This corrupts at most the first codeword; from the first "11" we find
    // afterwards, the rest of the stream is recovered.
    auto corrupted = flip_bit(encoded, 0);
    std::size_t pos = 0;
    std::vector<uint64_t> decoded;
    uint64_t v;
    while (decoded.size() < original.size() && try_decode_one(corrupted, pos, v)) {
        decoded.push_back(v);
    }
    int matches = 0;
    for (std::size_t i = 0; i < std::min(decoded.size(), original.size()); ++i) {
        if (decoded[i] == original[i]) ++matches;
    }
    // The first codeword may be corrupted; the remaining 5 should be intact.
    EXPECT_GE(matches, 4)
        << "Expected at least 4/6 values intact after flipping bit 0";
}

// ---- Integration tests using the priors library ----------------------------

#include "../2022-01-priors-wire-formats/priors.hpp"

// Helper: build fibonacci length vector for symbols 1..N.
static std::vector<std::size_t> fib_lengths(std::size_t N) {
    std::vector<std::size_t> v(N);
    for (std::size_t i = 0; i < N; ++i) {
        v[i] = fib_bit_count(static_cast<uint64_t>(i + 1));
    }
    return v;
}

// Fibonacci has bounded redundancy on its own implied prior (by definition).
TEST(FibonacciTest, FibonacciSmallRedundancyOnImpliedPrior) {
    const std::size_t N = 50;
    auto lens = fib_lengths(N);
    auto probs = priors::implied_prior(lens);
    double r = priors::redundancy(probs, lens);
    // The code is approximately optimal for its own prior.
    EXPECT_GE(r, 0.0);
    EXPECT_LT(r, 2.0);
}

// Fibonacci length grows as roughly log_phi(n) + 1 ~ 1.44 * log2(n) + 1.
// This is always at least ceil(log2(n)) + 1 bits (can't do better without
// the Zeckendorf representation). Verify Fibonacci length is bounded above
// by approximately 1.5 * gamma_length for n in [4..100].
TEST(FibonacciTest, FibonacciLengthWithinBounds) {
    for (uint64_t n = 4; n <= 100; ++n) {
        // gamma length = 2*floor(log2(n)) + 1.
        std::size_t k = 0;
        uint64_t tmp = n;
        while (tmp > 1) { tmp >>= 1; ++k; }
        std::size_t gl = 2 * k + 1;
        std::size_t fl = fib_bit_count(n);
        // Fibonacci length is at most 1.5 * gamma length for n in this range.
        // (Both grow as O(log n) with Fibonacci at ~1.44*log2(n) and gamma at ~2*log2(n).)
        EXPECT_LE(fl, gl + gl / 2 + 2) << "n=" << n;
        // Fibonacci length is at least floor(log2(n)) + 1 (minimum for n bits of info).
        EXPECT_GE(fl, k + 1) << "n=" << n;
    }
}
