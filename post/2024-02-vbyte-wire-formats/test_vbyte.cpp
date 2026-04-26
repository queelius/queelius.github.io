#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "vbyte.hpp"

using namespace vbyte;

// Minimal in-memory BitSink/BitSource for tests.
struct BitBuffer {
    std::vector<bool> bits;
    std::size_t pos_ = 0;
    void write(bool b) { bits.push_back(b); }
    bool read() { bool b = bits[pos_]; ++pos_; return b; }
};

// Helper: encode then decode n via VByte, check round-trip.
static std::uint64_t vbyte_round_trip(std::uint64_t n) {
    BitBuffer buf;
    VByte::encode(n, buf);
    buf.pos_ = 0;
    return VByte::decode(buf);
}

// Helper: return the bit count emitted for VByte(n).
static std::size_t vbyte_bit_count(std::uint64_t n) {
    BitBuffer buf;
    VByte::encode(n, buf);
    return buf.bits.size();
}

// Round-trip: small values (fit in 1 byte = 8 bits).
TEST(VByteTest, RoundTripSmall) {
    for (std::uint64_t n = 0; n <= 127; ++n) {
        EXPECT_EQ(vbyte_round_trip(n), n) << "n=" << n;
    }
}

// Round-trip: two-byte range [128, 16383].
TEST(VByteTest, RoundTripTwoByte) {
    for (std::uint64_t n : {std::uint64_t{128}, std::uint64_t{255},
                             std::uint64_t{1000}, std::uint64_t{16383}}) {
        EXPECT_EQ(vbyte_round_trip(n), n) << "n=" << n;
    }
}

// Round-trip: three-byte range.
TEST(VByteTest, RoundTripThreeByte) {
    for (std::uint64_t n : {std::uint64_t{16384}, std::uint64_t{100000},
                             std::uint64_t{2097151}}) {
        EXPECT_EQ(vbyte_round_trip(n), n) << "n=" << n;
    }
}

// Round-trip: large values (multi-byte).
TEST(VByteTest, RoundTripLarge) {
    for (std::uint64_t n : {std::uint64_t{1000000}, std::uint64_t{0xFFFFFFFF},
                             std::uint64_t{0xFFFFFFFFFFFFull}}) {
        EXPECT_EQ(vbyte_round_trip(n), n) << "n=" << n;
    }
}

// Values 0-127 encode in exactly 8 bits (1 byte).
TEST(VByteTest, OneByteLengthForSmall) {
    for (std::uint64_t n = 0; n <= 127; ++n) {
        EXPECT_EQ(vbyte_bit_count(n), 8u) << "n=" << n;
    }
}

// Values 128-16383 encode in exactly 16 bits (2 bytes).
TEST(VByteTest, TwoByteLengthForMedium) {
    for (std::uint64_t n : {std::uint64_t{128}, std::uint64_t{1000},
                             std::uint64_t{16383}}) {
        EXPECT_EQ(vbyte_bit_count(n), 16u) << "n=" << n;
    }
}

// Values 16384-2097151 encode in exactly 24 bits (3 bytes).
TEST(VByteTest, ThreeByteLengthForLarge) {
    for (std::uint64_t n : {std::uint64_t{16384}, std::uint64_t{2097151}}) {
        EXPECT_EQ(vbyte_bit_count(n), 24u) << "n=" << n;
    }
}

// Spot-check: VByte(0) = single byte 0x00 -> bits 0,0,0,0,0,0,0,0 (LSB first).
TEST(VByteTest, EncodingZero) {
    BitBuffer buf;
    VByte::encode(std::uint64_t{0}, buf);
    ASSERT_EQ(buf.bits.size(), 8u);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(buf.bits[static_cast<std::size_t>(i)], false) << "bit " << i;
    }
}

// Spot-check: VByte(1) = byte 0x01 -> bits 1,0,0,0,0,0,0,0 (LSB first).
TEST(VByteTest, EncodingOne) {
    BitBuffer buf;
    VByte::encode(std::uint64_t{1}, buf);
    ASSERT_EQ(buf.bits.size(), 8u);
    EXPECT_EQ(buf.bits[0], true);   // bit 0 (LSB) of byte 0x01
    for (int i = 1; i < 8; ++i) {
        EXPECT_EQ(buf.bits[static_cast<std::size_t>(i)], false) << "bit " << i;
    }
}

// Spot-check: VByte(128) = two bytes: 0x80, 0x01.
// First byte: bits 0,0,0,0,0,0,0,1 (value 128 & 0x7F = 0, continuation bit set).
// Second byte: bits 1,0,0,0,0,0,0,0 (value 1, no continuation bit).
TEST(VByteTest, Encoding128) {
    BitBuffer buf;
    VByte::encode(std::uint64_t{128}, buf);
    ASSERT_EQ(buf.bits.size(), 16u);
    // First byte: 0x80 = 10000000 binary. Stored LSB first: 0,0,0,0,0,0,0,1.
    for (int i = 0; i < 7; ++i) {
        EXPECT_EQ(buf.bits[static_cast<std::size_t>(i)], false) << "byte0 bit " << i;
    }
    EXPECT_EQ(buf.bits[7], true);   // continuation bit (bit 7 of byte 0)
    // Second byte: 0x01 = 00000001. Stored LSB first: 1,0,0,0,0,0,0,0.
    EXPECT_EQ(buf.bits[8], true);   // bit 0 of second byte
    for (int i = 9; i < 16; ++i) {
        EXPECT_EQ(buf.bits[static_cast<std::size_t>(i)], false) << "byte1 bit " << i;
    }
}

// n=0 is valid.
TEST(VByteTest, RoundTripZero) {
    EXPECT_EQ(vbyte_round_trip(std::uint64_t{0}), std::uint64_t{0});
}

#include "../2022-01-priors-wire-formats/priors.hpp"
#include "../2022-06-elias-gamma-wire-formats/unary_gamma.hpp"
#include "../2022-11-elias-delta-omega-wire-formats/elias_delta_omega.hpp"

// Helper: compute VByte length in bits for n.
// Formula: 8 * ceil(log2(n+1) / 7), minimum 8.
static std::size_t vbyte_length_formula(std::uint64_t n) {
    if (n < 128) return 8;
    std::uint64_t tmp = n;
    std::size_t bytes = 0;
    while (tmp > 0) {
        ++bytes;
        tmp >>= 7;
    }
    return bytes * 8;
}

// Verify VByte formula matches actual encode bit count.
TEST(VByteTest, LengthFormulaMatchesEncode) {
    for (std::uint64_t n : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{100},
                             std::uint64_t{127}, std::uint64_t{128}, std::uint64_t{1000},
                             std::uint64_t{16383}, std::uint64_t{16384},
                             std::uint64_t{1048575}, std::uint64_t{1048576}}) {
        EXPECT_EQ(vbyte_bit_count(n), vbyte_length_formula(n)) << "n=" << n;
    }
}

// Length comparison table (verified against actual implementations):
// | n       | VByte | Gamma | Delta |
// | 1       | 8     | 1     | 1     |
// | 100     | 8     | 13    | 11    |
// | 1000    | 16    | 19    | 16    |
// | 2^20    | 24    | 41    | 29    |
// | 2^32    | 40    | 65    | 43    |
//
// Delta length formula: Gamma(bit_width(n)) + bit_width(n) - 1.
// For large n, Delta grows as ~2*log2(log2(n)) + log2(n), faster than VByte.
//
// Gamma and Delta lengths are derived from their encoding bit counts.

// Helper: Gamma bit count for n.
static std::size_t gamma_bit_count_for(std::uint64_t n) {
    struct BitCounter {
        std::size_t count = 0;
        void write(bool) { ++count; }
    } counter;
    unary_gamma::Gamma::encode(n, counter);
    return counter.count;
}

// Helper: Delta bit count for n.
static std::size_t delta_bit_count_for(std::uint64_t n) {
    struct BitCounter {
        std::size_t count = 0;
        void write(bool) { ++count; }
    } counter;
    elias_delta_omega::Delta::encode(n, counter);
    return counter.count;
}

TEST(VByteTest, LengthTableN1) {
    EXPECT_EQ(vbyte_bit_count(1u), 8u);
    EXPECT_EQ(gamma_bit_count_for(1u), 1u);
    EXPECT_EQ(delta_bit_count_for(1u), 1u);
}

TEST(VByteTest, LengthTableN100) {
    EXPECT_EQ(vbyte_bit_count(100u), 8u);
    EXPECT_EQ(gamma_bit_count_for(100u), 13u);
    EXPECT_EQ(delta_bit_count_for(100u), 11u);
}

TEST(VByteTest, LengthTableN1000) {
    EXPECT_EQ(vbyte_bit_count(1000u), 16u);
    EXPECT_EQ(gamma_bit_count_for(1000u), 19u);
    EXPECT_EQ(delta_bit_count_for(1000u), 16u);
}

TEST(VByteTest, LengthTableN2pow20) {
    std::uint64_t n = 1u << 20;
    EXPECT_EQ(vbyte_bit_count(n), 24u);
    EXPECT_EQ(gamma_bit_count_for(n), 41u);
    EXPECT_EQ(delta_bit_count_for(n), 29u);
}

TEST(VByteTest, LengthTableN2pow32) {
    std::uint64_t n = std::uint64_t{1} << 32;
    EXPECT_EQ(vbyte_bit_count(n), 40u);
    EXPECT_EQ(gamma_bit_count_for(n), 65u);
    EXPECT_EQ(delta_bit_count_for(n), 43u);
}
