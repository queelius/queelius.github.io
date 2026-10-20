// vbyte.hpp
// Pedagogical implementation for the post "VByte / Varint" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (codecs.hpp: VByte)
//
// Note: real VByte implementations operate on bytes directly, not individual
// bits. This bit-level implementation is for consistency with the rest of the
// series. See the post's section B aside for an explanation.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vbyte {

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

// ---- VByte -- byte-aligned variable-length encoding -------------------------
//
// Encodes non-negative integer n >= 0 as a sequence of 7-bit groups,
// least significant first. Each group is stored in a byte where:
//   - Bits 0-6 hold 7 bits of the integer's value.
//   - Bit 7 (MSB) is a continuation flag: 1 = more bytes follow, 0 = last byte.
//
// So small integers (0-127) take 1 byte (8 bits); integers up to 2^14 - 1
// take 2 bytes (16 bits); up to 2^21 - 1 take 3 bytes (24 bits); etc.
//
// This bit-level implementation stores each byte LSB first to maintain
// consistency with the rest of the series. Real VByte implementations
// operate on bytes directly without the bit-level layer.
//
// Length: 8 * ceil(log2(n+1) / 7) bits, with a minimum of 8 bits.
// Implied prior: step-uniform over byte boundaries. VByte is optimal for
// sources where the byte-length of the encoding is geometrically distributed.
// Practical interpretation: if most values fit in 1 or 2 bytes, VByte is
// within a constant factor of entropy.

struct VByte {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        while (n >= 128) {
            // Group the low 7 bits and set the continuation flag (bit 7).
            std::uint8_t byte = static_cast<std::uint8_t>((n & 0x7F) | 0x80);
            // Write the byte LSB first.
            for (int i = 0; i < 8; ++i) sink.write(((byte >> i) & 1) != 0);
            n >>= 7;
        }
        // Last byte: low 7 bits, no continuation flag.
        std::uint8_t byte = static_cast<std::uint8_t>(n & 0x7F);
        for (int i = 0; i < 8; ++i) sink.write(((byte >> i) & 1) != 0);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        value_type result = 0;
        std::size_t shift = 0;
        while (true) {
            // Read one byte (8 bits, LSB first).
            std::uint8_t byte = 0;
            for (int i = 0; i < 8; ++i) {
                if (source.read()) byte |= static_cast<std::uint8_t>(1 << i);
            }
            // Accumulate the 7 data bits at the current shift position.
            result |= static_cast<value_type>(byte & 0x7F) << shift;
            // If bit 7 is clear, this is the last byte.
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        return result;
    }
};

}  // namespace vbyte
