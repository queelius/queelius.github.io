#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "synthesis.hpp"

using namespace synthesis;

// ---- empirical_distribution tests ------------------------------------------

// Single-value sample: distribution is a single point mass.
TEST(SynthesisTest, EmpiricalDistributionSingleValue) {
    std::vector<std::uint64_t> sample = {5, 5, 5};
    auto dist = empirical_distribution(sample);
    ASSERT_EQ(dist.size(), 1u);
    EXPECT_NEAR(dist.at(5), 1.0, 1e-12);
}

// Two-value sample, equal counts: both have probability 0.5.
TEST(SynthesisTest, EmpiricalDistributionTwoEqualValues) {
    std::vector<std::uint64_t> sample = {1, 2, 1, 2};
    auto dist = empirical_distribution(sample);
    ASSERT_EQ(dist.size(), 2u);
    EXPECT_NEAR(dist.at(1), 0.5, 1e-12);
    EXPECT_NEAR(dist.at(2), 0.5, 1e-12);
}

// Probabilities must sum to 1.
TEST(SynthesisTest, EmpiricalDistributionSumsToOne) {
    std::vector<std::uint64_t> sample = {1, 2, 3, 1, 2, 1};
    auto dist = empirical_distribution(sample);
    double total = 0.0;
    for (const auto& [v, p] : dist) total += p;
    EXPECT_NEAR(total, 1.0, 1e-12);
}

// Known frequencies: {1: 3 times, 2: 1 time} -> {1: 0.75, 2: 0.25}.
TEST(SynthesisTest, EmpiricalDistributionKnownFrequencies) {
    std::vector<std::uint64_t> sample = {1, 1, 1, 2};
    auto dist = empirical_distribution(sample);
    ASSERT_EQ(dist.size(), 2u);
    EXPECT_NEAR(dist.at(1), 0.75, 1e-12);
    EXPECT_NEAR(dist.at(2), 0.25, 1e-12);
}

// ---- entropy_of tests -------------------------------------------------------

// Entropy of a uniform distribution over K symbols = log2(K).
TEST(SynthesisTest, EntropyOfUniform) {
    std::map<std::uint64_t, double> dist = {{1, 0.25}, {2, 0.25}, {3, 0.25}, {4, 0.25}};
    EXPECT_NEAR(entropy_of(dist), 2.0, 1e-12);
}

// Entropy of a degenerate distribution (one symbol certain) = 0.
TEST(SynthesisTest, EntropyOfDegenerate) {
    std::map<std::uint64_t, double> dist = {{7, 1.0}};
    EXPECT_NEAR(entropy_of(dist), 0.0, 1e-12);
}

// Entropy of {0.5, 0.5} = 1.
TEST(SynthesisTest, EntropyOfBinaryHalf) {
    std::map<std::uint64_t, double> dist = {{1, 0.5}, {2, 0.5}};
    EXPECT_NEAR(entropy_of(dist), 1.0, 1e-12);
}

// ---- length_for tests -------------------------------------------------------

// Unary: length of n is n bits (for positive integers).
TEST(SynthesisTest, LengthForUnary) {
    for (std::uint64_t n = 1; n <= 10; ++n) {
        EXPECT_EQ(length_for("Unary", n), n) << "n=" << n;
    }
}

// Gamma: length = 2*floor(log2(n)) + 1.
TEST(SynthesisTest, LengthForGamma) {
    // n=1 -> 1, n=2 -> 3, n=3 -> 3, n=4 -> 5, n=8 -> 7, n=16 -> 9
    EXPECT_EQ(length_for("Gamma", 1), 1u);
    EXPECT_EQ(length_for("Gamma", 2), 3u);
    EXPECT_EQ(length_for("Gamma", 3), 3u);
    EXPECT_EQ(length_for("Gamma", 4), 5u);
    EXPECT_EQ(length_for("Gamma", 8), 7u);
    EXPECT_EQ(length_for("Gamma", 16), 9u);
}

// Delta: total = gamma_len(floor(log2(n))+1) + floor(log2(n)).
// Standard Elias delta formula: L=floor(log2(n))+1, encode L in gamma, then k trailing bits.
// Known values: n=1->1, n=2->4, n=3->4, n=4->5, n=16->9, n=256->15.
TEST(SynthesisTest, LengthForDelta) {
    EXPECT_EQ(length_for("Delta", 1), 1u);
    EXPECT_EQ(length_for("Delta", 2), 4u);
    EXPECT_EQ(length_for("Delta", 3), 4u);
    EXPECT_EQ(length_for("Delta", 4), 5u);
    EXPECT_EQ(length_for("Delta", 16), 9u);
    EXPECT_EQ(length_for("Delta", 256), 15u);
}

// VByte: length = 8 * ceil(ceil(log2(n+1)) / 7), or 8 for n in [0,127].
TEST(SynthesisTest, LengthForVByte) {
    EXPECT_EQ(length_for("VByte", 0),    8u);   // 1 byte
    EXPECT_EQ(length_for("VByte", 1),    8u);   // 1 byte
    EXPECT_EQ(length_for("VByte", 127),  8u);   // 1 byte
    EXPECT_EQ(length_for("VByte", 128),  16u);  // 2 bytes
    EXPECT_EQ(length_for("VByte", 16383), 16u); // 2 bytes
    EXPECT_EQ(length_for("VByte", 16384), 24u); // 3 bytes
}

// ---- redundancy_for tests ---------------------------------------------------

// Redundancy is non-negative (Shannon's theorem).
TEST(SynthesisTest, RedundancyNonNegative) {
    // Build a geometric-ish distribution that favors small values.
    std::vector<std::uint64_t> geo_sample;
    for (std::uint64_t i = 1; i <= 20; ++i) {
        // Add i copies of value 21-i so smaller values are more frequent.
        for (std::uint64_t j = 0; j < (21 - i); ++j) geo_sample.push_back(i);
    }
    auto dist = empirical_distribution(geo_sample);
    for (std::string_view code : {"Unary", "Gamma", "Delta", "Omega", "Fibonacci", "VByte"}) {
        double r = redundancy_for(code, dist);
        EXPECT_GE(r, -1e-9) << "code=" << code;
    }
}

// On a strongly geometric source (small values dominate), Unary has low redundancy.
TEST(SynthesisTest, RedundancyUnaryLowForGeometricSource) {
    // Build a geometric source: p_n = (1/2)^n for n=1..10, normalized.
    std::vector<std::uint64_t> geo_sample;
    // Add 2^(11-n) copies of each value n to approximate geometric(1/2).
    for (std::uint64_t n = 1; n <= 10; ++n) {
        std::uint64_t count = static_cast<std::uint64_t>(1) << (11 - n);
        for (std::uint64_t j = 0; j < count; ++j) geo_sample.push_back(n);
    }
    auto dist = empirical_distribution(geo_sample);
    double r_unary = redundancy_for("Unary", dist);
    double r_gamma = redundancy_for("Gamma", dist);
    // Unary should beat Gamma on this strongly geometric source.
    EXPECT_LT(r_unary, r_gamma);
}

// On a large-range source (values spread over many bytes), VByte has lower
// redundancy than Unary.
TEST(SynthesisTest, RedundancyVByteLowForLargeValues) {
    // Build a sample with many values in [1000, 2000].
    std::vector<std::uint64_t> large_sample;
    for (std::uint64_t v = 1000; v <= 2000; ++v) large_sample.push_back(v);
    auto dist = empirical_distribution(large_sample);
    double r_vbyte = redundancy_for("VByte", dist);
    double r_unary = redundancy_for("Unary", dist);
    // Unary would be catastrophically long for large values.
    EXPECT_LT(r_vbyte, r_unary);
}

// ---- recommend_code tests ---------------------------------------------------

// On a strongly geometric source, recommend_code should pick Unary or a
// code with low redundancy for geometric data.
TEST(SynthesisTest, RecommendCodeGeometricPicksUnaryOrFibonacci) {
    // Heavy geometric source: value 1 is extremely common.
    std::vector<std::uint64_t> geo_sample(200, 1);
    for (std::uint64_t n = 2; n <= 5; ++n) {
        for (std::uint64_t j = 0; j < (6 - n) * 10; ++j) geo_sample.push_back(n);
    }
    std::string code = recommend_code(geo_sample);
    // Unary or Fibonacci both work well for strongly geometric sources.
    EXPECT_TRUE(code == "Unary" || code == "Fibonacci" || code == "Gamma")
        << "Unexpected recommendation for geometric source: " << code;
}

// On a large-value source, recommend_code should not pick Unary.
TEST(SynthesisTest, RecommendCodeLargeValuesNotUnary) {
    std::vector<std::uint64_t> large_sample;
    for (std::uint64_t v = 500; v <= 1000; ++v) large_sample.push_back(v);
    std::string code = recommend_code(large_sample);
    EXPECT_NE(code, "Unary") << "Unary should not be recommended for large values";
}

// recommend_code returns one of the six named codes.
TEST(SynthesisTest, RecommendCodeReturnsKnownCode) {
    std::vector<std::uint64_t> sample = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::string code = recommend_code(sample);
    const std::vector<std::string> known = {
        "Unary", "Gamma", "Delta", "Omega", "Fibonacci", "VByte"};
    bool found = std::find(known.begin(), known.end(), code) != known.end();
    EXPECT_TRUE(found) << "Unknown code returned: " << code;
}
