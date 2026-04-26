#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <span>
#include <vector>
#include "prefix_free.hpp"

using namespace prefix_free;

TEST(PrefixFreeTest, BitIORoundTrip) {
    std::array<std::uint8_t, 2> buf{};
    std::span<std::uint8_t> buf_span(buf);
    BitWriter w(buf_span);
    const std::vector<bool> pattern{true, false, true, true, false, false, true};
    for (bool b : pattern) w.write(b);
    w.align();

    std::span<const std::uint8_t> read_span(buf.data(), w.bytes_written());
    BitReader r(read_span);
    for (bool b : pattern) {
        EXPECT_EQ(r.read(), b);
    }
}

TEST(PrefixFreeTest, ConceptsAreSatisfied) {
    static_assert(BitSink<BitWriter>);
    static_assert(BitSource<BitReader>);
}

namespace {

template<typename Codec, typename T>
T round_trip(const T& v) {
    std::array<std::uint8_t, 1024> buf{};
    std::span<std::uint8_t> buf_span(buf);
    BitWriter w(buf_span);
    Codec::encode(v, w);
    w.align();
    std::span<const std::uint8_t> read_span(buf.data(), w.bytes_written());
    BitReader r(read_span);
    return Codec::decode(r);
}

}  // namespace

TEST(PrefixFreeTest, UnaryRoundTrip) {
    for (std::uint64_t n : {std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{5},
                            std::uint64_t{10}, std::uint64_t{32}}) {
        EXPECT_EQ(round_trip<Unary>(n), n) << "n = " << n;
    }
}

TEST(PrefixFreeTest, GammaRoundTrip) {
    for (std::uint64_t n : {std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{100},
                            std::uint64_t{1024}, std::uint64_t{1} << 20}) {
        EXPECT_EQ(round_trip<Gamma>(n), n) << "n = " << n;
    }
}

TEST(PrefixFreeTest, VecOfGammaRoundTrip) {
    using Codec = Vec<Gamma>;
    Codec::value_type v{1, 2, 3, 5, 8, 13, 21};
    EXPECT_EQ(round_trip<Codec>(v), v);
}

TEST(PrefixFreeTest, VecEmptyRoundTrip) {
    using Codec = Vec<Gamma>;
    Codec::value_type v;
    EXPECT_EQ(round_trip<Codec>(v), v);
}

TEST(PrefixFreeTest, EncodeStringConcatenatesCodewords) {
    std::map<char, std::string> code{
        {'A', "0"},
        {'B', "01"},
        {'C', "10"},
        {'D', "010"},
    };
    EXPECT_EQ(encode_string(code, "DA"), "0100");
    EXPECT_EQ(encode_string(code, "AC"), "010");
    EXPECT_EQ(encode_string(code, "BA"), "010");
    EXPECT_EQ(encode_string(code, "D"), "010");
}

TEST(PrefixFreeTest, EnumerateParsesReturnsAllValidDecodings) {
    std::map<char, std::string> code{
        {'A', "0"},
        {'B', "01"},
        {'C', "10"},
        {'D', "010"},
    };
    auto parses = enumerate_parses(code, "010");
    std::vector<std::string> as_strings;
    for (const auto& p : parses) {
        as_strings.emplace_back(p.begin(), p.end());
    }
    std::sort(as_strings.begin(), as_strings.end());
    ASSERT_EQ(as_strings.size(), 3u);
    EXPECT_EQ(as_strings[0], "AC");
    EXPECT_EQ(as_strings[1], "BA");
    EXPECT_EQ(as_strings[2], "D");
}

TEST(PrefixFreeTest, NonPrefixFreeIsAmbiguous) {
    // A "code" that is NOT prefix-free.
    // "0"  is a prefix of "01" and "010".
    // "01" is a prefix of "010".
    std::map<char, std::string> code{
        {'A', "0"},
        {'B', "01"},
        {'C', "10"},
        {'D', "010"},
    };

    // [D], [A, C], and [B, A] all encode to the same bit string "010".
    EXPECT_EQ(encode_string(code, "D"),  "010");
    EXPECT_EQ(encode_string(code, "AC"), "010");
    EXPECT_EQ(encode_string(code, "BA"), "010");

    // Greedy left-to-right decoding cannot pick a unique answer; the multi-parse
    // decoder finds all three valid parses.
    auto parses = enumerate_parses(code, "010");
    EXPECT_EQ(parses.size(), 3u);
}

TEST(PrefixFreeTest, KraftInequalityHoldsForUnary) {
    // Unary code: codeword for n has length n bits.
    // Sum of 2^-l for n = 1..K is 1 - 2^-K, which is < 1.
    double sum = 0.0;
    constexpr int K = 30;
    for (int n = 1; n <= K; ++n) {
        sum += std::ldexp(1.0, -n);  // 2^-n
    }
    EXPECT_LE(sum, 1.0);
    // Convergence check: sum approaches 1 as K grows.
    EXPECT_GT(sum, 1.0 - std::ldexp(1.0, -(K - 1)));
}

TEST(PrefixFreeTest, KraftInequalityHoldsForGamma) {
    // Gamma code: codeword for n has length 2 * floor(log2(n)) + 1.
    // For each "block" of integers with the same length 2k+1, there are 2^k
    // such integers (from 2^k to 2^(k+1) - 1). Their contribution is
    // 2^k * 2^-(2k+1) = 2^-(k+1).
    // Summing over k = 0, 1, 2, ... gives 1/2 + 1/4 + 1/8 + ... = 1.
    double sum = 0.0;
    constexpr int N = 1 << 14;
    for (int n = 1; n <= N; ++n) {
        int len = 2 * static_cast<int>(std::bit_width(static_cast<unsigned>(n))) - 1;
        sum += std::ldexp(1.0, -len);
    }
    EXPECT_LE(sum, 1.0 + 1e-9);
}
