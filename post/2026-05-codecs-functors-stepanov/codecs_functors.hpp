// codecs_functors.hpp
// Pedagogical implementation for the post "Bits Follow Types"
// (Stepanov series, slot 20). For the production version, see PFC:
// https://github.com/queelius/pfc

#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace codecs_functors {

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

template<typename C>
concept Codec = requires {
    typename C::value_type;
};
// Note: a fuller Codec concept would require encode and decode functions
// against any BitSink and BitSource. The minimal form above is sufficient
// for the pedagogical examples in this post; the prose discusses the
// fuller requirements.

// ---- Leaf codec: Elias gamma -----------------------------------------------
// Encodes positive integers (>= 1).
// Codeword for n: (bit_width(n) - 1) zero bits, then a one bit,
// then the bits of n minus the leading 1, MSB first.
// Length: 2 * floor(log2(n)) + 1 bits.

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

// ---- Combinator: Opt -- coproduct with unit (1 + T) -------------------------
// Wire format: 1 tag bit (1 = present, 0 = absent), then C's encoding if present.

template<typename C>
struct Opt {
    using value_type = std::optional<typename C::value_type>;

    template<BitSink S>
    static void encode(const value_type& v, S& sink) {
        if (v) {
            sink.write(true);
            C::encode(*v, sink);
        } else {
            sink.write(false);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        if (source.read()) {
            return C::decode(source);
        }
        return std::nullopt;
    }
};

// ---- Combinator: Either -- binary coproduct (A + B) -------------------------
// Wire format: 1 tag bit (0 = left/A, 1 = right/B), then the chosen branch.

template<typename A, typename B>
struct Either {
    using value_type = std::variant<typename A::value_type, typename B::value_type>;

    template<BitSink S>
    static void encode(const value_type& v, S& sink) {
        if (v.index() == 0) {
            sink.write(false);
            A::encode(std::get<0>(v), sink);
        } else {
            sink.write(true);
            B::encode(std::get<1>(v), sink);
        }
    }

    template<BitSource S>
    static value_type decode(S& source) {
        if (source.read()) {
            return value_type{std::in_place_index<1>, B::decode(source)};
        }
        return value_type{std::in_place_index<0>, A::decode(source)};
    }
};

// ---- Combinator: Either3 -- ternary coproduct (A + B + C) -------------------
// Wire format: 2 tag bits encode the branch index as an integer (LSB written
// first): A=0, B=1, C=2. Then the chosen branch. The fourth code (3) is
// unused; a malformed input that produces it is treated as C.

template<typename A, typename B, typename C>
struct Either3 {
    using value_type = std::variant<typename A::value_type,
                                    typename B::value_type,
                                    typename C::value_type>;

    template<BitSink S>
    static void encode(const value_type& v, S& sink) {
        std::size_t idx = v.index();
        sink.write((idx & 0x1) != 0);
        sink.write((idx & 0x2) != 0);
        if (idx == 0) A::encode(std::get<0>(v), sink);
        else if (idx == 1) B::encode(std::get<1>(v), sink);
        else C::encode(std::get<2>(v), sink);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t idx = (source.read() ? 1u : 0u);
        idx |= (source.read() ? 2u : 0u);
        if (idx == 0) return value_type{std::in_place_index<0>, A::decode(source)};
        if (idx == 1) return value_type{std::in_place_index<1>, B::decode(source)};
        return value_type{std::in_place_index<2>, C::decode(source)};
    }
};

// ---- Combinator: Pair -- binary product (A x B) ----------------------------
// Wire format: A's encoding immediately followed by B's encoding. Recovery
// works because A is prefix-free; the boundary is implicit in A's structure.

template<typename A, typename B>
struct Pair {
    using value_type = std::pair<typename A::value_type, typename B::value_type>;

    template<BitSink S>
    static void encode(const value_type& v, S& sink) {
        A::encode(v.first, sink);
        B::encode(v.second, sink);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        auto a = A::decode(source);
        auto b = B::decode(source);
        return value_type{std::move(a), std::move(b)};
    }
};

// ---- Combinator: Vec -- free monoid on T (List<T>) ---------------------------
// Wire format: gamma-coded (size + 1) followed by C's encoding of each element.
// The +1 bias is because Gamma requires positive integers and 0 is a valid size.

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

// ---- Auxiliary leaf codec: Bool ---------------------------------------------
// Wire format: 1 bit (0 = false, 1 = true).

struct Bool {
    using value_type = bool;

    template<BitSink S>
    static void encode(value_type v, S& sink) { sink.write(v); }

    template<BitSource S>
    static value_type decode(S& source) { return source.read(); }
};

// ---- Auxiliary adapter: Signed ----------------------------------------------
// Wraps an unsigned codec via zigzag encoding so signed integers can use it:
//   0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4, ...
// Then biased by +1 because the underlying codec (e.g. Gamma) requires positives.

template<typename C>
struct Signed {
    using value_type = std::int64_t;

    template<BitSink S>
    static void encode(value_type v, S& sink) {
        std::uint64_t z;
        if (v < 0) {
            z = (static_cast<std::uint64_t>(-(v + 1)) << 1) | 1u;
        } else {
            z = static_cast<std::uint64_t>(v) << 1;
        }
        C::encode(z + 1, sink);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::uint64_t z = C::decode(source) - 1;
        if (z & 1u) {
            return -static_cast<std::int64_t>(z >> 1) - 1;
        }
        return static_cast<std::int64_t>(z >> 1);
    }
};

// ---- Auxiliary leaf codec: Byte ---------------------------------------------
// Wire format: 8 bits, LSB first (matches BitWriter's byte packing).

struct Byte {
    using value_type = std::uint8_t;

    template<BitSink S>
    static void encode(value_type v, S& sink) {
        for (int i = 0; i < 8; ++i) sink.write(((v >> i) & 1) != 0);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i) {
            if (source.read()) v |= static_cast<std::uint8_t>(1 << i);
        }
        return v;
    }
};

// ---- Auxiliary codec: String -- conceptually Vec<Byte> over std::string ----
// Wire format: gamma-coded (size + 1) followed by 8 bits per byte.

struct String {
    using value_type = std::string;

    template<BitSink S>
    static void encode(const value_type& s, S& sink) {
        Gamma::encode(s.size() + 1, sink);
        for (char c : s) Byte::encode(static_cast<std::uint8_t>(c), sink);
    }

    template<BitSource S>
    static value_type decode(S& source) {
        std::size_t n = static_cast<std::size_t>(Gamma::decode(source) - 1);
        value_type s;
        s.reserve(n);
        for (std::size_t i = 0; i < n; ++i) s.push_back(static_cast<char>(Byte::decode(source)));
        return s;
    }
};

}  // namespace codecs_functors
