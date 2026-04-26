// succinct_bitvector.hpp
// Pedagogical implementation for the post "Succinct Bit Vectors and Rank/Select"
// in the "Algebra over Wire Formats" series.
//
// Production version: PFC include/pfc/succinct.hpp (SuccinctBitVector class with
// BlockRankSupport, O(1) rank, O(log n) select).
// https://github.com/queelius/pfc

#pragma once

#include <algorithm> // std::min
#include <bit>       // std::popcount (C++20)
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace succinct_bv {

// ---- popcount_word ---------------------------------------------------------
//
// Returns the number of set bits in a single 64-bit word.
// Uses std::popcount (C++20), which compiles to a single hardware instruction
// (POPCNT) on x86-64 and equivalent on ARM.

[[nodiscard]] inline std::size_t popcount_word(std::uint64_t w) noexcept {
    return static_cast<std::size_t>(std::popcount(w));
}

// ---- SuccinctBitVector --------------------------------------------------
//
// Stores a bit vector in packed uint64_t words. The auxiliary rank/select
// index is built lazily in build_index() (called by the constructor).
//
// Word layout: bit i lives in bits_[i/64] at position i%64 (LSB-first).
// Unused bits in the last word are kept zero.

class SuccinctBitVector {
public:
    // Construct from std::vector<bool>. Builds the auxiliary index immediately.
    explicit SuccinctBitVector(const std::vector<bool>& bits)
        : n_(bits.size())
        , bits_((bits.size() + 63) / 64, uint64_t{0})
    {
        for (std::size_t i = 0; i < bits.size(); ++i) {
            if (bits[i]) {
                bits_[i / 64] |= (uint64_t{1} << (i % 64));
            }
        }
        build_index();  // Builds superblock_ranks_ and block_ranks_.
    }

    // Logical size in bits.
    [[nodiscard]] std::size_t size() const noexcept { return n_; }

    // Access bit at position i (0-indexed). No bounds checking.
    [[nodiscard]] bool bit(std::size_t i) const noexcept {
        return (bits_[i / 64] >> (i % 64)) & uint64_t{1};
    }

    // rank1(i): count of 1-bits in [0, i). O(1) using superblock + block + popcount.
    //
    // Algorithm:
    //   1. Find superblock:  sb = i / SUPERBLOCK_BITS.
    //   2. Find block:       blk = i / BLOCK_BITS.
    //   3. Find within-word: w_off = i % BLOCK_BITS.
    //   4. Return superblock_ranks_[sb]
    //            + block_ranks_[blk]
    //            + popcount(bits_[blk] & ((1<<w_off)-1)).
    //
    // Three array lookups and one popcount: all constant-time operations.
    // Special case: when w_off == 0 and blk == bits_.size() (i.e., i == n_
    // and n_ is a multiple of BLOCK_BITS), we clamp blk to the last valid
    // block and add its full popcount instead of accessing out-of-range memory.
    [[nodiscard]] std::size_t rank1(std::size_t i) const noexcept {
        if (i == 0) return 0;
        std::size_t sb      = i / SUPERBLOCK_BITS;
        std::size_t blk     = i / BLOCK_BITS;
        std::size_t w_off   = i % BLOCK_BITS;

        // Guard: when i == n_ and n_ is an exact multiple of BLOCK_BITS,
        // blk == bits_.size() which is out of range.  In that case, the
        // answer is the absolute cumulative rank up to the start of superblock
        // sb, plus the superblock-relative cumulative rank stored for blk-1,
        // plus the full popcount of the last word.
        if (blk >= bits_.size()) {
            // Sum: superblock rank + block rank of last block + its popcount.
            if (bits_.empty()) return 0;
            std::size_t last_blk = bits_.size() - 1;
            std::size_t last_sb  = last_blk / BLOCKS_PER_SB;
            std::size_t result   = superblock_ranks_[last_sb];
            result += block_ranks_[last_blk];
            result += popcount_word(bits_[last_blk]);
            return result;
        }

        // Absolute rank up to this superblock's start.
        std::size_t result  = superblock_ranks_[sb];
        // Add block-relative rank (within the superblock, before this block).
        result += block_ranks_[blk];
        // Add count of set bits in bits_[blk] strictly before bit w_off.
        if (w_off > 0) {
            uint64_t mask = (uint64_t{1} << w_off) - uint64_t{1};
            result += popcount_word(bits_[blk] & mask);
        }
        return result;
    }

    // select1(j): position of the j-th set bit (0-indexed). O(log n).
    //
    // Algorithm:
    //   1. Binary search superblock_ranks_ to find the superblock sb where the
    //      j-th 1-bit lies: largest sb s.t. superblock_ranks_[sb] <= j.
    //   2. Walk blocks within sb linearly until the running sum exceeds j.
    //   3. Within the found block, scan set bits to find the exact position.
    //
    // Worst case: O(log(n/4096)) for the binary search + O(64) constant for the
    // block and bit scans = O(log n) total.
    [[nodiscard]] std::size_t select1(std::size_t j) const noexcept {
        // Step 1: binary search over superblock_ranks_.
        // Find the largest sb such that superblock_ranks_[sb] <= j.
        std::size_t lo = 0;
        std::size_t hi = superblock_ranks_.size();  // Exclusive upper bound.
        while (lo + 1 < hi) {
            std::size_t mid = lo + (hi - lo) / 2;
            if (superblock_ranks_[mid] <= j) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        std::size_t sb = lo;

        // Step 2: linear scan over blocks within superblock sb.
        // Find the last block whose cumulative rank (absolute) <= j.
        std::size_t first_blk = sb * BLOCKS_PER_SB;
        std::size_t last_blk  = std::min(first_blk + BLOCKS_PER_SB,
                                          block_ranks_.size());
        std::size_t blk = first_blk;
        for (std::size_t b = first_blk + 1; b < last_blk; ++b) {
            // Absolute rank at start of block b.
            std::size_t abs_rank = superblock_ranks_[sb] + block_ranks_[b];
            if (abs_rank > j) break;
            blk = b;
        }

        // Absolute rank at start of block blk.
        std::size_t base = superblock_ranks_[sb] + block_ranks_[blk];
        // We need the (j - base)-th set bit (0-indexed) within bits_[blk].
        std::size_t target = j - base;

        // Step 3: find the target-th set bit in bits_[blk].
        // Iterate over set bits by repeatedly clearing the lowest set bit.
        uint64_t word = bits_[blk];
        std::size_t bit_pos = blk * BLOCK_BITS;
        for (std::size_t k = 0; k < target; ++k) {
            // Clear lowest set bit; advance bit_pos past it.
            std::size_t low = static_cast<std::size_t>(__builtin_ctzll(word));
            bit_pos = blk * BLOCK_BITS + low + 1;
            word &= word - 1;
        }
        // The answer is the position of the lowest set bit now remaining in word.
        std::size_t final_low = static_cast<std::size_t>(__builtin_ctzll(word));
        return blk * BLOCK_BITS + final_low;
    }

    // Space occupied by the raw bit array (bytes).
    [[nodiscard]] std::size_t bit_bytes() const noexcept {
        return bits_.size() * sizeof(uint64_t);
    }

    // Space occupied by the auxiliary index (superblock + block arrays, bytes).
    [[nodiscard]] std::size_t index_bytes() const noexcept {
        return superblock_ranks_.size() * sizeof(uint64_t)
             + block_ranks_.size()      * sizeof(uint16_t);
    }

    // Expose the naive O(n/64) rank for testing and index-correctness verification.
    [[nodiscard]] std::size_t rank1_naive(std::size_t i) const noexcept {
        // Same logic as Task 5's rank1 but kept separately so rank1 can be
        // switched to the indexed version in Task 7.
        if (i == 0) return 0;
        std::size_t word_idx    = i / BLOCK_BITS;
        std::size_t within_word = i % BLOCK_BITS;
        std::size_t count = 0;
        for (std::size_t w = 0; w < word_idx; ++w) {
            count += popcount_word(bits_[w]);
        }
        if (within_word > 0) {
            uint64_t mask = (uint64_t{1} << within_word) - uint64_t{1};
            count += popcount_word(bits_[word_idx] & mask);
        }
        return count;
    }

    // Test accessor: absolute cumulative rank at superblock sb's start.
    [[nodiscard]] std::size_t superblock_rank_at(std::size_t sb) const noexcept {
        return (sb < superblock_ranks_.size()) ? superblock_ranks_[sb] : 0;
    }

    // Test accessor: block-relative rank for block blk (relative to its superblock start).
    [[nodiscard]] std::size_t block_rank_at(std::size_t blk) const noexcept {
        return (blk < block_ranks_.size()) ? block_ranks_[blk] : 0;
    }

protected:
    std::size_t n_;                    // Logical bit count.
    std::vector<uint64_t> bits_;       // Packed bit array, LSB-first.
    std::vector<uint64_t> superblock_ranks_;  // Cumulative rank at superblock boundaries.
    std::vector<uint16_t> block_ranks_;       // Block-relative rank (within superblock).

    static constexpr std::size_t SUPERBLOCK_BITS = 4096;  // 64 words per superblock.
    static constexpr std::size_t BLOCK_BITS      = 64;    // One uint64_t word per block.
    static constexpr std::size_t BLOCKS_PER_SB   = SUPERBLOCK_BITS / BLOCK_BITS;  // 64.

    // build_index(): construct superblock_ranks_ and block_ranks_ from bits_.
    //
    // Index layout:
    //   superblock: every SUPERBLOCK_BITS bits (4096 bits = 64 words).
    //               One uint64_t per superblock storing the *absolute* cumulative
    //               rank from bit 0 to the start of this superblock.
    //   block: every BLOCK_BITS bits (64 bits = one word).
    //          One uint16_t per block storing the *superblock-relative* cumulative
    //          rank from the start of the enclosing superblock to the start of
    //          this block.
    //
    // Space: superblock_ranks_ has ceil(n/4096) entries of 8 bytes each.
    //        block_ranks_ has ceil(n/64) entries of 2 bytes each.
    //        Total index: ~ n/512 + n/32 bytes = ~ 0.2 * n/8 bytes (roughly 3% of n bits).
    //        Asymptotically o(n) and in practice small.
    void build_index() {
        if (n_ == 0) return;
        std::size_t num_superblocks = (n_ + SUPERBLOCK_BITS - 1) / SUPERBLOCK_BITS;
        std::size_t num_blocks      = (n_ + BLOCK_BITS      - 1) / BLOCK_BITS;

        superblock_ranks_.resize(num_superblocks, 0);
        block_ranks_.resize(num_blocks, 0);

        std::size_t cumulative    = 0;  // Absolute rank from bit 0.
        std::size_t sb_cumulative = 0;  // Rank within the current superblock.

        for (std::size_t blk = 0; blk < num_blocks; ++blk) {
            std::size_t sb = blk / BLOCKS_PER_SB;  // Which superblock.
            // At the start of each superblock: record absolute rank.
            if (blk % BLOCKS_PER_SB == 0) {
                superblock_ranks_[sb] = cumulative;
                sb_cumulative = 0;
            }
            // Record the block-relative rank (before counting this block's bits).
            block_ranks_[blk] = static_cast<uint16_t>(sb_cumulative);

            // Count bits in this block.
            std::size_t count = popcount_word(bits_[blk]);
            cumulative    += count;
            sb_cumulative += count;
        }
    }
};

}  // namespace succinct_bv
