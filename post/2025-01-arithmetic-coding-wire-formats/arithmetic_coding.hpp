// arithmetic_coding.hpp
// Pedagogical integer range coder for the post "Arithmetic Coding" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (include/pfc/arithmetic_coding.hpp)
//
// Reference: Witten, Neal, Cleary, "Arithmetic Coding for Data Compression,"
// CACM 30(6), 1987.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace arithmetic_coding {

// 32-bit range coder constants.
constexpr std::uint32_t TOP_VALUE      = 0xFFFFFFFFu;
constexpr std::uint32_t HALF           = 0x80000000u;
constexpr std::uint32_t QUARTER        = 0x40000000u;
constexpr std::uint32_t THREE_QUARTER  = 0xC0000000u;

// ---- BitWriter -- packs individual bits into bytes -------------------------
//
// Bits are packed MSB-first into each byte. flush() pads the final partial
// byte with zeros and appends it.

class BitWriter {
    std::vector<std::uint8_t> bytes_;
    std::uint8_t              current_byte_ = 0;
    int                       bit_count_    = 0;

public:
    void write(bool bit) {
        current_byte_ = static_cast<std::uint8_t>(
            (current_byte_ << 1) | (bit ? 1u : 0u));
        ++bit_count_;
        if (bit_count_ == 8) {
            bytes_.push_back(current_byte_);
            current_byte_ = 0;
            bit_count_    = 0;
        }
    }

    void flush() {
        if (bit_count_ > 0) {
            current_byte_ = static_cast<std::uint8_t>(
                current_byte_ << (8 - bit_count_));
            bytes_.push_back(current_byte_);
            current_byte_ = 0;
            bit_count_    = 0;
        }
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

    [[nodiscard]] std::size_t bit_count() const noexcept {
        return bytes_.size() * 8;
    }
};

// ---- BitReader -- reads individual bits from a byte vector -----------------
//
// Bits are read MSB-first from each byte. Reading past the end returns 0.

class BitReader {
    const std::vector<std::uint8_t>& bytes_;
    std::size_t byte_idx_ = 0;
    int         bit_idx_  = 7;  // next bit to read within current_byte (MSB=7)

public:
    explicit BitReader(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes) {}

    bool read() {
        if (byte_idx_ >= bytes_.size()) return false;
        bool bit = ((bytes_[byte_idx_] >> bit_idx_) & 1u) != 0;
        if (--bit_idx_ < 0) {
            bit_idx_ = 7;
            ++byte_idx_;
        }
        return bit;
    }
};

// ---- ArithmeticEncoder -- integer range coder (encoder) --------------------
//
// Encodes a sequence of symbols by iteratively shrinking the unit interval
// [low, high] / TOP_VALUE according to each symbol's cumulative-frequency
// sub-interval, then emitting agreed-upon high bits.
//
// Reference: Witten, Neal, Cleary (CACM 1987), Section 2.

class ArithmeticEncoder {
    std::uint32_t low_             = 0;
    std::uint32_t high_            = TOP_VALUE;
    std::size_t   underflow_count_ = 0;
    BitWriter&    sink_;

public:
    explicit ArithmeticEncoder(BitWriter& sink) : sink_(sink) {}

    // State accessors (used in tests; not part of the encoding interface).
    [[nodiscard]] std::uint32_t low()             const noexcept { return low_; }
    [[nodiscard]] std::uint32_t high()            const noexcept { return high_; }
    [[nodiscard]] std::size_t   underflow_count() const noexcept { return underflow_count_; }

    void encode_symbol(std::uint32_t low_cum, std::uint32_t high_cum,
                       std::uint32_t total) {
        assert(low_cum < high_cum);
        assert(high_cum <= total);
        assert(total > 0);
        std::uint64_t range = static_cast<std::uint64_t>(high_) - low_ + 1;
        high_ = low_ + static_cast<std::uint32_t>((range * high_cum) / total - 1);
        low_  = low_ + static_cast<std::uint32_t>((range * low_cum)  / total);
        renormalize();
    }

    void finish() {
        // Emit one bit to identify which half of the current interval to use,
        // and flush any pending underflow bits.
        ++underflow_count_;  // ensures at least one opposition bit follows
        if (low_ < QUARTER) {
            emit_bit_and_underflow(false);
        } else {
            emit_bit_and_underflow(true);
        }
    }

private:
    void emit_bit_and_underflow(bool bit) {
        sink_.write(bit);
        while (underflow_count_ > 0) {
            sink_.write(!bit);
            --underflow_count_;
        }
    }

    void renormalize() {
        while (true) {
            if (high_ < HALF) {
                // Both bounds in [0, HALF): high bit is 0.
                emit_bit_and_underflow(false);
            } else if (low_ >= HALF) {
                // Both bounds in [HALF, TOP_VALUE]: high bit is 1.
                emit_bit_and_underflow(true);
                low_  -= HALF;
                high_ -= HALF;
            } else if (low_ >= QUARTER && high_ < THREE_QUARTER) {
                // Underflow: interval straddles HALF; squeeze toward middle.
                ++underflow_count_;
                low_  -= QUARTER;
                high_ -= QUARTER;
            } else {
                break;
            }
            low_  <<= 1;
            high_ = (high_ << 1) | 1u;
        }
    }
};

// ---- ArithmeticDecoder -- integer range coder (decoder) --------------------
//
// Mirrors the encoder. The decoder maintains the same [low, high] interval and
// additionally a 32-bit code register, which holds the current prefix of the
// compressed input. Decoding a symbol finds the sub-interval containing code_,
// then performs the same renormalization as the encoder.

class ArithmeticDecoder {
    std::uint32_t low_  = 0;
    std::uint32_t high_ = TOP_VALUE;
    std::uint32_t code_ = 0;
    BitReader&    src_;

public:
    explicit ArithmeticDecoder(BitReader& src) : src_(src) {
        // Prime the code register with the first 32 bits.
        for (int i = 0; i < 32; ++i) {
            code_ = (code_ << 1) | (src_.read() ? 1u : 0u);
        }
    }

    [[nodiscard]] std::uint32_t low()  const noexcept { return low_; }
    [[nodiscard]] std::uint32_t high() const noexcept { return high_; }
    [[nodiscard]] std::uint32_t code() const noexcept { return code_; }

    // decode_symbol: find the symbol whose cumulative-frequency interval
    // contains the current scaled code value, update the interval, and
    // renormalize (reading new bits from src_).
    //
    // get_freq_cb: (std::uint32_t scaled_value) -> std::size_t symbol
    //   given a value in [0, total), returns the symbol index.
    // cum_range_cb: (std::size_t symbol) -> {low_cum, high_cum}
    //   returns the cumulative-frequency endpoints for the symbol.
    // total: total cumulative frequency.

    template <typename FreqCb, typename RangeCb>
    std::size_t decode_symbol(FreqCb&& get_freq_cb, RangeCb&& cum_range_cb,
                              std::uint32_t total) {
        std::uint64_t range  = static_cast<std::uint64_t>(high_) - low_ + 1;
        std::uint32_t scaled = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(code_ - low_) * total) / range);

        std::size_t sym = get_freq_cb(scaled);
        auto [lo_cum, hi_cum] = cum_range_cb(sym);

        // Update interval exactly as the encoder would.
        high_ = low_ + static_cast<std::uint32_t>((range * hi_cum) / total - 1);
        low_  = low_ + static_cast<std::uint32_t>((range * lo_cum) / total);
        decoder_renormalize();
        return sym;
    }

private:
    void decoder_renormalize() {
        while (true) {
            if (high_ < HALF) {
                // Nothing to subtract; shift in a new bit.
            } else if (low_ >= HALF) {
                code_ -= HALF;
                low_  -= HALF;
                high_ -= HALF;
            } else if (low_ >= QUARTER && high_ < THREE_QUARTER) {
                code_ -= QUARTER;
                low_  -= QUARTER;
                high_ -= QUARTER;
            } else {
                break;
            }
            low_  <<= 1;
            high_ = (high_ << 1) | 1u;
            code_ = (code_ << 1) | (src_.read() ? 1u : 0u);
        }
    }
};

}  // namespace arithmetic_coding
