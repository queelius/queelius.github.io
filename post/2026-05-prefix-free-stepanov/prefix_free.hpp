// prefix_free.hpp
// Pedagogical implementation for the post "When Lists Become Bits"
// (Stepanov series, slot 21). For the production version, see PFC:
// https://github.com/queelius/pfc

#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace prefix_free {

// ---- Bit I/O ----------------------------------------------------------------

class BitWriter {
    std::span<std::uint8_t> buf_;
    std::size_t byte_idx_ = 0;
    std::uint8_t byte_ = 0;
    std::uint8_t bit_pos_ = 0;
public:
    explicit BitWriter(std::span<std::uint8_t> buf) noexcept : buf_(buf) {}

    void write(bool bit) noexcept {
        byte_ |= (bit ? std::uint8_t{1} : std::uint8_t{0}) << bit_pos_;
        if (++bit_pos_ == 8) {
            buf_[byte_idx_++] = byte_;
            byte_ = 0;
            bit_pos_ = 0;
        }
    }

    void align() noexcept {
        if (bit_pos_ > 0) {
            buf_[byte_idx_++] = byte_;
            byte_ = 0;
            bit_pos_ = 0;
        }
    }

    [[nodiscard]] std::size_t bytes_written() const noexcept {
        return byte_idx_ + (bit_pos_ > 0 ? 1 : 0);
    }
};

class BitReader {
    std::span<const std::uint8_t> buf_;
    std::size_t byte_idx_ = 0;
    std::uint8_t bit_pos_ = 0;
public:
    explicit BitReader(std::span<const std::uint8_t> buf) noexcept : buf_(buf) {}

    bool read() noexcept {
        bool bit = ((buf_[byte_idx_] >> bit_pos_) & 1) != 0;
        if (++bit_pos_ == 8) {
            ++byte_idx_;
            bit_pos_ = 0;
        }
        return bit;
    }

    // True while at least one BYTE remains in the buffer. Callers query this
    // only at item boundaries, never mid-item, so padding bits from the
    // writer's align() are never exposed as data.
    [[nodiscard]] bool peek() const noexcept {
        return byte_idx_ < buf_.size();
    }
};

// ---- Concepts ---------------------------------------------------------------

template<typename T>
concept BitSink = requires(T& s, bool bit) {
    { s.write(bit) } -> std::same_as<void>;
};

template<typename T>
concept BitSource = requires(T& s) {
    { s.read() } -> std::same_as<bool>;
    { s.peek() } -> std::convertible_to<bool>;
};

// ---- Leaf codec: Unary ------------------------------------------------------
// Encodes positive integers (>= 1).
// Codeword for n: (n - 1) zero bits, then a one bit. Length: n bits.

struct Unary {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Unary is undefined for n = 0");
        for (value_type i = 1; i < n; ++i) sink.write(false);
        sink.write(true);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        value_type n = 1;
        while (!source.read()) ++n;
        return n;
    }
};

// ---- Leaf codec: Elias gamma -- (re-stated from post 1) ---------------------

struct Gamma {
    using value_type = std::uint64_t;

    template<BitSink S>
    static void encode(value_type n, S& sink) {
        assert(n >= 1 && "Gamma is undefined for n = 0");
        std::size_t bits = std::bit_width(n);
        for (std::size_t i = 0; i < bits - 1; ++i) sink.write(false);
        sink.write(true);
        for (std::size_t i = bits - 1; i > 0; --i) {
            sink.write(((n >> (i - 1)) & 1) != 0);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t bits = 1;
        while (!source.read()) ++bits;
        value_type result = 1;
        for (std::size_t i = 1; i < bits; ++i) {
            result = (result << 1) | (source.read() ? value_type{1} : value_type{0});
        }
        return result;
    }
};

// ---- Combinator: Vec -- the free monoid lift (re-stated from post 1) -------
// Wire format: gamma-coded (size + 1) followed by C's encoding of each element.

template<typename C>
struct Vec {
    using value_type = std::vector<typename C::value_type>;

    template<BitSink S>
    static void encode(const value_type& v, S& sink) {
        Gamma::encode(v.size() + 1, sink);
        for (const auto& x : v) {
            C::encode(x, sink);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t n = static_cast<std::size_t>(Gamma::decode(source) - 1);
        value_type result;
        result.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            result.push_back(C::decode(source));
        }
        return result;
    }
};

// ---- Helpers for the ambiguity demonstration --------------------------------
// These are NOT part of the codec abstraction. They support the demonstration
// in section D of the post: showing that a non-prefix-free code admits
// multiple parses for the same bit string.

inline std::string encode_string(const std::map<char, std::string>& code,
                                 std::string_view symbols) {
    std::string out;
    for (char c : symbols) {
        auto it = code.find(c);
        if (it == code.end()) return {};  // unknown symbol
        out += it->second;
    }
    return out;
}

namespace detail {

inline void enumerate_parses_recursive(
    const std::map<char, std::string>& code,
    std::string_view bits,
    std::size_t pos,
    std::vector<char>& current,
    std::vector<std::vector<char>>& results)
{
    if (pos == bits.size()) {
        results.push_back(current);
        return;
    }
    for (const auto& [sym, codeword] : code) {
        if (pos + codeword.size() <= bits.size() &&
            bits.compare(pos, codeword.size(), codeword) == 0) {
            current.push_back(sym);
            enumerate_parses_recursive(code, bits, pos + codeword.size(),
                                       current, results);
            current.pop_back();
        }
    }
}

}  // namespace detail

inline std::vector<std::vector<char>> enumerate_parses(
    const std::map<char, std::string>& code,
    std::string_view bits)
{
    std::vector<std::vector<char>> results;
    std::vector<char> current;
    detail::enumerate_parses_recursive(code, bits, 0, current, results);
    return results;
}

}  // namespace prefix_free
