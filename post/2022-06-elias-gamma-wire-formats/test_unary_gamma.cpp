#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include "unary_gamma.hpp"

using namespace unary_gamma;

// Minimal in-memory BitSink for tests.
struct BitBuffer {
    std::vector<bool> bits;
    void write(bool b) { bits.push_back(b); }
    bool read() {
        bool b = bits[pos_]; ++pos_; return b;
    }
    std::size_t pos_ = 0;
};

// Helper: encode then decode n, check round-trip.
static uint64_t unary_round_trip(uint64_t n) {
    BitBuffer buf;
    Unary::encode(n, buf);
    buf.pos_ = 0;
    return Unary::decode(buf);
}

// Helper: return the bit count emitted for n.
static std::size_t unary_bit_count(uint64_t n) {
    BitBuffer buf;
    Unary::encode(n, buf);
    return buf.bits.size();
}

TEST(UnaryGammaTest, UnaryRoundTrip1) {
    EXPECT_EQ(unary_round_trip(1), 1u);
}

TEST(UnaryGammaTest, UnaryRoundTrip2) {
    EXPECT_EQ(unary_round_trip(2), 2u);
}

TEST(UnaryGammaTest, UnaryRoundTripLarge) {
    for (uint64_t n = 1; n <= 20; ++n) {
        EXPECT_EQ(unary_round_trip(n), n) << "n=" << n;
    }
}

// Codeword for n has exactly n bits.
TEST(UnaryGammaTest, UnaryBitCount) {
    for (uint64_t n = 1; n <= 20; ++n) {
        EXPECT_EQ(unary_bit_count(n), n) << "n=" << n;
    }
}

// Codeword for n=1 is a single '1' bit.
TEST(UnaryGammaTest, UnaryEncoding1IsSingleOne) {
    BitBuffer buf;
    Unary::encode(uint64_t{1}, buf);
    ASSERT_EQ(buf.bits.size(), 1u);
    EXPECT_EQ(buf.bits[0], true);
}

// Codeword for n=3 is "001": two zeros then a one.
TEST(UnaryGammaTest, UnaryEncoding3IsTwoZerosOneOne) {
    BitBuffer buf;
    Unary::encode(uint64_t{3}, buf);
    ASSERT_EQ(buf.bits.size(), 3u);
    EXPECT_EQ(buf.bits[0], false);
    EXPECT_EQ(buf.bits[1], false);
    EXPECT_EQ(buf.bits[2], true);
}

// Helper: encode then decode n via Gamma, check round-trip.
static uint64_t gamma_round_trip(uint64_t n) {
    BitBuffer buf;
    Gamma::encode(n, buf);
    buf.pos_ = 0;
    return Gamma::decode(buf);
}

// Helper: return bit count for Gamma(n).
static std::size_t gamma_bit_count(uint64_t n) {
    BitBuffer buf;
    Gamma::encode(n, buf);
    return buf.bits.size();
}

TEST(UnaryGammaTest, GammaRoundTrip) {
    for (uint64_t n = 1; n <= 256; ++n) {
        EXPECT_EQ(gamma_round_trip(n), n) << "n=" << n;
    }
}

// Length of Gamma(n) = 2*floor(log2(n)) + 1.
TEST(UnaryGammaTest, GammaBitCount) {
    for (uint64_t n = 1; n <= 128; ++n) {
        std::size_t k = 0;
        uint64_t tmp = n;
        while (tmp > 1) { tmp >>= 1; ++k; }
        std::size_t expected = 2 * k + 1;
        EXPECT_EQ(gamma_bit_count(n), expected) << "n=" << n;
    }
}

// Spot-check specific encodings from the spec.
// 1 -> "1", 2 -> "010", 3 -> "011", 4 -> "00100", 8 -> "0001000"
TEST(UnaryGammaTest, GammaEncoding1IsSingleOne) {
    BitBuffer buf;
    Gamma::encode(uint64_t{1}, buf);
    ASSERT_EQ(buf.bits.size(), 1u);
    EXPECT_EQ(buf.bits[0], true);
}

TEST(UnaryGammaTest, GammaEncoding2Is010) {
    BitBuffer buf;
    Gamma::encode(uint64_t{2}, buf);
    ASSERT_EQ(buf.bits.size(), 3u);
    EXPECT_EQ(buf.bits[0], false);
    EXPECT_EQ(buf.bits[1], true);
    EXPECT_EQ(buf.bits[2], false);
}

TEST(UnaryGammaTest, GammaEncoding3Is011) {
    BitBuffer buf;
    Gamma::encode(uint64_t{3}, buf);
    ASSERT_EQ(buf.bits.size(), 3u);
    EXPECT_EQ(buf.bits[0], false);
    EXPECT_EQ(buf.bits[1], true);
    EXPECT_EQ(buf.bits[2], true);
}

TEST(UnaryGammaTest, GammaEncoding4Is00100) {
    BitBuffer buf;
    Gamma::encode(uint64_t{4}, buf);
    ASSERT_EQ(buf.bits.size(), 5u);
    // "00100": leading zeros count = 2, then the 3-bit binary for 4
    EXPECT_EQ(buf.bits[0], false);
    EXPECT_EQ(buf.bits[1], false);
    EXPECT_EQ(buf.bits[2], true);
    EXPECT_EQ(buf.bits[3], false);
    EXPECT_EQ(buf.bits[4], false);
}

// unary_lengths(K) returns {1, 2, 3, ..., K}.
TEST(UnaryGammaTest, UnaryLengthsVector) {
    auto v = unary_lengths(5);
    ASSERT_EQ(v.size(), 5u);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(v[i], i + 1) << "i=" << i;
    }
}

// gamma_lengths(N) returns the Gamma codeword lengths for 1..N.
TEST(UnaryGammaTest, GammaLengthsVector) {
    // Spot-check: gamma(1)=1, gamma(2)=3, gamma(3)=3, gamma(4)=5, gamma(8)=7.
    auto v = gamma_lengths(8);
    ASSERT_EQ(v.size(), 8u);
    EXPECT_EQ(v[0], 1u);  // n=1
    EXPECT_EQ(v[1], 3u);  // n=2
    EXPECT_EQ(v[2], 3u);  // n=3
    EXPECT_EQ(v[3], 5u);  // n=4
    EXPECT_EQ(v[4], 5u);  // n=5
    EXPECT_EQ(v[5], 5u);  // n=6
    EXPECT_EQ(v[6], 5u);  // n=7
    EXPECT_EQ(v[7], 7u);  // n=8
}

// gamma_lengths agrees with the bit-count measured by Gamma::encode.
TEST(UnaryGammaTest, GammaLengthsMatchEncode) {
    auto v = gamma_lengths(32);
    for (std::size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(v[i], gamma_bit_count(static_cast<uint64_t>(i + 1))) << "n=" << (i+1);
    }
}

// Integration tests using the priors library from post 3.
#include "../2022-01-priors-wire-formats/priors.hpp"

// Unary is exactly optimal for geometric(1/2): redundancy ~ 0.
// (The code achieves entropy exactly because the prior is dyadic and saturates Kraft.)
TEST(UnaryGammaTest, UnaryAchievesEntropyOnGeometricPrior) {
    const std::size_t K = 30;
    auto lens = unary_lengths(K);
    auto probs = priors::implied_prior(lens);
    double r = priors::redundancy(probs, lens);
    // The implied prior of unary IS geometric(1/2), so redundancy should be
    // essentially zero (only floating-point and truncation error).
    EXPECT_NEAR(r, 0.0, 1e-6);
}

// Gamma has bounded redundancy on a power-law(2) source.
// This is the "approximately optimal" claim from the spec.
TEST(UnaryGammaTest, GammaSmallRedundancyOnPowerLaw2) {
    const std::size_t N = 128;
    auto lens = gamma_lengths(N);
    // Build power-law(2) source: p_n = C/n^2.
    std::vector<double> pl(N);
    double z = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        double n = static_cast<double>(i + 1);
        pl[i] = 1.0 / (n * n);
        z += pl[i];
    }
    for (double& p : pl) p /= z;
    double r = priors::redundancy(pl, lens);
    EXPECT_GE(r, 0.0);
    EXPECT_LT(r, 3.0);  // Universal-code bounded redundancy.
}

// Gamma beats unary on a power-law source.
TEST(UnaryGammaTest, GammaBeatsUnaryOnPowerLaw2) {
    const std::size_t N = 64;
    auto gamma_lens = gamma_lengths(N);
    auto unary_lens = unary_lengths(N);
    std::vector<double> pl(N);
    double z = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        double n = static_cast<double>(i + 1);
        pl[i] = 1.0 / (n * n);
        z += pl[i];
    }
    for (double& p : pl) p /= z;
    double r_gamma = priors::redundancy(pl, gamma_lens);
    double r_unary = priors::redundancy(pl, unary_lens);
    EXPECT_LT(r_gamma, r_unary);
}

// Unary beats gamma on a geometric(1/2) source.
TEST(UnaryGammaTest, UnaryBeatsGammaOnGeometricHalf) {
    const std::size_t K = 30;
    auto unary_lens = unary_lengths(K);
    auto gamma_lens = gamma_lengths(K);
    auto probs = priors::implied_prior(unary_lens);  // geometric(1/2)
    double r_unary = priors::redundancy(probs, unary_lens);
    double r_gamma = priors::redundancy(probs, gamma_lens);
    EXPECT_LE(r_unary, r_gamma);
}
