// roaring_bitmap.hpp
// Pedagogical implementation for the post "RoaringBitmap" in the
// "Algebra over Wire Formats" series.
//
// Production version: PFC include/pfc/succinct.hpp (RoaringBitmap class with
// three container types: array, bitmap, run).
// https://github.com/queelius/pfc

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace roaring {

// ---- ArrayContainer ---------------------------------------------------------
//
// Stores at most 4096 distinct uint16_t values as a sorted array.
// Contains check: binary search, O(log cardinality).
// Add: sorted insertion, O(cardinality) worst case due to shifting.
//
// Space: 2 * cardinality bytes (2 bytes per uint16_t element).
// At 4096 elements: 8 KB. A BitmapContainer costs 8 KB for 65536 bits.
// The threshold ARRAY_MAX = 4096 is chosen so that array and bitmap cost
// the same number of bytes at the crossover point.

static constexpr std::size_t ARRAY_MAX = 4096;

class ArrayContainer {
    std::vector<uint16_t> elems_;   // Sorted, unique.

public:
    ArrayContainer() = default;

    // Add value v to the container. No-op if already present.
    void add(uint16_t v) {
        auto it = std::lower_bound(elems_.begin(), elems_.end(), v);
        if (it != elems_.end() && *it == v) return;  // Already present.
        elems_.insert(it, v);
    }

    // Returns true if v is in the container.
    [[nodiscard]] bool contains(uint16_t v) const noexcept {
        auto it = std::lower_bound(elems_.begin(), elems_.end(), v);
        return it != elems_.end() && *it == v;
    }

    // Number of distinct values.
    [[nodiscard]] std::size_t cardinality() const noexcept {
        return elems_.size();
    }

    // Raw access for conversion helpers.
    [[nodiscard]] const std::vector<uint16_t>& elements() const noexcept {
        return elems_;
    }
};

// ---- BitmapContainer --------------------------------------------------------
//
// Dense bit vector covering the full 16-bit value space (65536 values).
// Stored as 1024 uint64_t words (65536 / 64 = 1024), occupying exactly 8 KB.
//
// Contains check: O(1) word lookup + bit test.
// Add: O(1) word lookup + bit set.
// Cardinality: O(1024) = O(1) amortized, scanning all words with popcount.
//
// Used when a chunk has more than ARRAY_MAX = 4096 elements.

class BitmapContainer {
    static constexpr std::size_t NUM_WORDS = 65536 / 64;  // 1024 words = 8 KB.
    std::vector<uint64_t> words_;

public:
    BitmapContainer() : words_(NUM_WORDS, uint64_t{0}) {}

    void add(uint16_t v) noexcept {
        words_[v / 64] |= (uint64_t{1} << (v % 64));
    }

    [[nodiscard]] bool contains(uint16_t v) const noexcept {
        return (words_[v / 64] >> (v % 64)) & uint64_t{1};
    }

    [[nodiscard]] std::size_t cardinality() const noexcept {
        std::size_t count = 0;
        for (auto w : words_) count += static_cast<std::size_t>(__builtin_popcountll(w));
        return count;
    }

    [[nodiscard]] const std::vector<uint64_t>& raw_words() const noexcept {
        return words_;
    }
};

// ---- RunContainer -----------------------------------------------------------
//
// Stores consecutive-integer runs as (start, length-1) pairs, sorted by start.
// Run (s, l) covers integers s through s+l inclusive (l+1 values).
//
// Optimal when a chunk has many long runs: space = 4 * num_runs bytes.
// Example: integers 0-999 stored as one run (0, 999) = 4 bytes vs 2000 bytes
// as an array or 8192 bytes as a bitmap.
//
// add: inserts a value, extending or merging runs as needed. Amortized O(runs).
// contains: linear scan; replace with binary search for production use.
// cardinality: sums (length+1) for each run.

class RunContainer {
    // Run: integers in [start, start + length] (inclusive on both ends).
    std::vector<std::pair<uint16_t, uint16_t>> runs_;  // (start, length-1), sorted.

public:
    RunContainer() = default;

    void add(uint16_t v) {
        // Find the first run whose start >= v (lower_bound with < comparator).
        auto it = std::lower_bound(runs_.begin(), runs_.end(),
                                   std::pair<uint16_t, uint16_t>{v, 0u},
                                   [](const auto& a, const auto& b) {
                                       return a.first < b.first;
                                   });

        // Check if v falls within the run at 'it' itself (it->first == v means covered).
        if (it != runs_.end() && it->first == v) return;  // v is the start of run 'it'.

        // Check if v is already in the run immediately before it.
        if (it != runs_.begin()) {
            auto prev = std::prev(it);
            uint16_t end = prev->first + prev->second;  // Last value in prev run.
            if (v <= end) return;                        // Already covered.
            if (v == static_cast<uint16_t>(end + 1)) {  // Extends prev run.
                ++prev->second;
                // Check if prev now touches 'it'.
                if (it != runs_.end() && static_cast<uint16_t>(prev->first + prev->second + 1) == it->first) {
                    prev->second += it->second + 1;
                    runs_.erase(it);
                }
                return;
            }
        }

        // Check if v == it->first - 1 (prepend to 'it' run).
        if (it != runs_.end() && it->first > 0 && v == static_cast<uint16_t>(it->first - 1)) {
            --it->first;
            ++it->second;
            return;
        }

        // v is isolated: insert a new run of length 1 (length-1 = 0).
        runs_.insert(it, {v, uint16_t{0}});
    }

    [[nodiscard]] bool contains(uint16_t v) const noexcept {
        for (const auto& [start, len] : runs_) {
            if (v >= start && v <= static_cast<uint16_t>(start + len)) return true;
            if (start > v) break;
        }
        return false;
    }

    [[nodiscard]] std::size_t cardinality() const noexcept {
        std::size_t total = 0;
        for (const auto& [start, len] : runs_) total += static_cast<std::size_t>(len) + 1;
        return total;
    }

    // Number of runs (for testing).
    [[nodiscard]] std::size_t num_runs() const noexcept { return runs_.size(); }

    [[nodiscard]] const std::vector<std::pair<uint16_t, uint16_t>>& run_list() const noexcept {
        return runs_;
    }
};

// ---- Container conversion helpers -------------------------------------------
//
// These are called by the RoaringBitmap dispatcher when a chunk crosses a
// density threshold. The conversion cost is O(n) in the chunk size and is
// amortized over many subsequent O(1) operations.

// array_to_bitmap: O(cardinality) -- set each array element as a bitmap bit.
[[nodiscard]] inline BitmapContainer array_to_bitmap(const ArrayContainer& a) {
    BitmapContainer b;
    for (uint16_t v : a.elements()) b.add(v);
    return b;
}

// bitmap_to_array: O(65536/64) -- scan bitmap words, collect set bits.
[[nodiscard]] inline ArrayContainer bitmap_to_array(const BitmapContainer& b) {
    ArrayContainer a;
    const auto& words = b.raw_words();
    for (std::size_t w = 0; w < words.size(); ++w) {
        uint64_t word = words[w];
        while (word) {
            // Find lowest set bit.
            std::size_t bit = static_cast<std::size_t>(__builtin_ctzll(word));
            a.add(static_cast<uint16_t>(w * 64 + bit));
            word &= word - 1;  // Clear lowest set bit.
        }
    }
    return a;
}

// array_to_run: O(cardinality) -- scan sorted array, group consecutive values.
// The array must be sorted (invariant of ArrayContainer).
[[nodiscard]] inline RunContainer array_to_run(const ArrayContainer& a) {
    RunContainer r;
    for (uint16_t v : a.elements()) r.add(v);
    return r;
}

// bitmap_to_run: convert a BitmapContainer into a RunContainer via array.
[[nodiscard]] inline RunContainer bitmap_to_run(const BitmapContainer& b) {
    return array_to_run(bitmap_to_array(b));
}

// ---- RoaringBitmap ----------------------------------------------------------
//
// A compressed integer set over uint32_t values. The 32-bit space is divided
// into 64K-integer chunks (high 16 bits select the chunk; low 16 bits index
// within). Each chunk uses the optimal container type based on cardinality:
//
//   cardinality <= ARRAY_MAX (4096): ArrayContainer  (sparse)
//   cardinality >  ARRAY_MAX:        BitmapContainer (dense)
//   after optimize():                RunContainer    (clustered)
//
// The variant dispatch ensures each chunk's operations use the right algorithm.
// std::visit lets callers write single generic lambdas instead of manual
// switch-on-type dispatch.

using ContainerVariant = std::variant<ArrayContainer, BitmapContainer, RunContainer>;

class RoaringBitmap {
    std::map<uint16_t, ContainerVariant> chunks_;  // chunk-id -> container.

    // Extract the high 16 bits (chunk-id) and low 16 bits of a 32-bit value.
    static uint16_t chunk_id(uint32_t v) noexcept { return static_cast<uint16_t>(v >> 16); }
    static uint16_t low_bits(uint32_t v) noexcept { return static_cast<uint16_t>(v & 0xFFFF); }

public:
    RoaringBitmap() = default;

    // Add value v. Creates the chunk if it does not exist (starts as ArrayContainer).
    // Automatically converts Array -> Bitmap if the chunk exceeds ARRAY_MAX.
    void add(uint32_t v) {
        uint16_t cid = chunk_id(v);
        uint16_t low = low_bits(v);

        auto it = chunks_.find(cid);
        if (it == chunks_.end()) {
            chunks_.emplace(cid, ArrayContainer{});
            it = chunks_.find(cid);
        }

        std::visit([&](auto& container) {
            using T = std::decay_t<decltype(container)>;
            if constexpr (std::is_same_v<T, ArrayContainer>) {
                container.add(low);
                // Threshold check: convert to bitmap when array exceeds ARRAY_MAX.
                if (container.cardinality() > ARRAY_MAX) {
                    it->second = array_to_bitmap(container);
                }
            } else if constexpr (std::is_same_v<T, BitmapContainer>) {
                container.add(low);
            } else {
                // RunContainer: convert to bitmap, add, then rebuild runs.
                BitmapContainer b;
                for (const auto& [start, len] : container.run_list()) {
                    for (std::size_t i = 0; i <= len; ++i)
                        b.add(static_cast<uint16_t>(start + i));
                }
                b.add(low);
                it->second = bitmap_to_run(b);
            }
        }, it->second);
    }

    // Returns true if v is in the bitmap.
    [[nodiscard]] bool contains(uint32_t v) const noexcept {
        auto it = chunks_.find(chunk_id(v));
        if (it == chunks_.end()) return false;
        return std::visit([low = low_bits(v)](const auto& c) {
            return c.contains(low);
        }, it->second);
    }

    // Total number of distinct values.
    [[nodiscard]] std::size_t cardinality() const noexcept {
        std::size_t total = 0;
        for (const auto& [cid, variant] : chunks_) {
            total += std::visit([](const auto& c) { return c.cardinality(); }, variant);
        }
        return total;
    }

    // optimize(): convert each chunk to RunContainer if that reduces space.
    // Called explicitly after bulk-loading; not called automatically.
    void optimize() {
        for (auto& [cid, variant] : chunks_) {
            std::visit([&variant](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, ArrayContainer>) {
                    variant = array_to_run(c);
                } else if constexpr (std::is_same_v<T, BitmapContainer>) {
                    variant = bitmap_to_run(c);
                }
                // RunContainer: already optimized; leave as is.
            }, variant);
        }
    }

    // union_with: all values in either *this or other.
    [[nodiscard]] RoaringBitmap union_with(const RoaringBitmap& other) const {
        RoaringBitmap result = *this;  // Start with a copy of *this.
        for (const auto& [cid, variant] : other.chunks_) {
            std::visit([&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, ArrayContainer>) {
                    for (uint16_t v : c.elements()) {
                        result.add(static_cast<uint32_t>(cid) << 16 | v);
                    }
                } else if constexpr (std::is_same_v<T, BitmapContainer>) {
                    const auto& words = c.raw_words();
                    for (std::size_t w = 0; w < words.size(); ++w) {
                        uint64_t word = words[w];
                        while (word) {
                            std::size_t bit = static_cast<std::size_t>(__builtin_ctzll(word));
                            result.add(static_cast<uint32_t>(cid) << 16 |
                                       static_cast<uint32_t>(w * 64 + bit));
                            word &= word - 1;
                        }
                    }
                } else {
                    for (const auto& [start, len] : c.run_list()) {
                        for (std::size_t i = 0; i <= len; ++i) {
                            result.add(static_cast<uint32_t>(cid) << 16 |
                                       static_cast<uint32_t>(start + i));
                        }
                    }
                }
            }, variant);
        }
        return result;
    }

    // intersection_with: only values present in both *this and other.
    [[nodiscard]] RoaringBitmap intersection_with(const RoaringBitmap& other) const {
        RoaringBitmap result;
        for (const auto& [cid, variant] : chunks_) {
            auto it = other.chunks_.find(cid);
            if (it == other.chunks_.end()) continue;
            // Both chunks exist: add values present in both containers.
            std::visit([&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, ArrayContainer>) {
                    for (uint16_t v : c.elements()) {
                        if (std::visit([v](const auto& o) { return o.contains(v); }, it->second)) {
                            result.add(static_cast<uint32_t>(cid) << 16 | v);
                        }
                    }
                } else if constexpr (std::is_same_v<T, BitmapContainer>) {
                    const auto& words = c.raw_words();
                    for (std::size_t w = 0; w < words.size(); ++w) {
                        uint64_t word = words[w];
                        while (word) {
                            std::size_t bit = static_cast<std::size_t>(__builtin_ctzll(word));
                            uint16_t v = static_cast<uint16_t>(w * 64 + bit);
                            if (std::visit([v](const auto& o) { return o.contains(v); }, it->second)) {
                                result.add(static_cast<uint32_t>(cid) << 16 | v);
                            }
                            word &= word - 1;
                        }
                    }
                } else {
                    for (const auto& [start, len] : c.run_list()) {
                        for (std::size_t i = 0; i <= len; ++i) {
                            uint16_t v = static_cast<uint16_t>(start + i);
                            if (std::visit([v](const auto& o) { return o.contains(v); }, it->second)) {
                                result.add(static_cast<uint32_t>(cid) << 16 | v);
                            }
                        }
                    }
                }
            }, variant);
        }
        return result;
    }

    // difference: values in *this but not in other.
    [[nodiscard]] RoaringBitmap difference(const RoaringBitmap& other) const {
        RoaringBitmap result;
        for (const auto& [cid, variant] : chunks_) {
            auto it = other.chunks_.find(cid);
            std::visit([&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                auto add_if_absent = [&](uint16_t v) {
                    if (it == other.chunks_.end() ||
                        !std::visit([v](const auto& o) { return o.contains(v); }, it->second)) {
                        result.add(static_cast<uint32_t>(cid) << 16 | v);
                    }
                };
                if constexpr (std::is_same_v<T, ArrayContainer>) {
                    for (uint16_t v : c.elements()) add_if_absent(v);
                } else if constexpr (std::is_same_v<T, BitmapContainer>) {
                    const auto& words = c.raw_words();
                    for (std::size_t w = 0; w < words.size(); ++w) {
                        uint64_t word = words[w];
                        while (word) {
                            std::size_t bit = static_cast<std::size_t>(__builtin_ctzll(word));
                            add_if_absent(static_cast<uint16_t>(w * 64 + bit));
                            word &= word - 1;
                        }
                    }
                } else {
                    for (const auto& [start, len] : c.run_list()) {
                        for (std::size_t i = 0; i <= len; ++i) {
                            add_if_absent(static_cast<uint16_t>(start + i));
                        }
                    }
                }
            }, variant);
        }
        return result;
    }
};

}  // namespace roaring
