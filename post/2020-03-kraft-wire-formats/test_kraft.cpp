#include <gtest/gtest.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "kraft.hpp"

using namespace kraft;

TEST(KraftTest, BinaryTreeInsertCodewordAtRoot) {
    BinaryTree t;
    t.insert("0");
    EXPECT_TRUE(t.contains("0"));
    EXPECT_FALSE(t.contains("1"));
}

TEST(KraftTest, BinaryTreeInsertMultipleCodewords) {
    BinaryTree t;
    t.insert("0");
    t.insert("10");
    t.insert("110");
    t.insert("111");
    EXPECT_TRUE(t.contains("0"));
    EXPECT_TRUE(t.contains("10"));
    EXPECT_TRUE(t.contains("110"));
    EXPECT_TRUE(t.contains("111"));
}

TEST(KraftTest, BinaryTreeIsPrefixFreeReturnsTrueForValidCode) {
    BinaryTree t;
    t.insert("0");
    t.insert("10");
    t.insert("110");
    t.insert("111");
    EXPECT_TRUE(t.is_prefix_free());
}

TEST(KraftTest, BinaryTreeIsPrefixFreeReturnsFalseWhenCodewordIsPrefix) {
    BinaryTree t;
    t.insert("0");
    t.insert("01");
    EXPECT_FALSE(t.is_prefix_free());
}

TEST(KraftTest, BinaryTreeIsPrefixFreeReturnsFalseWhenLongerCodewordContainsAnother) {
    BinaryTree t;
    t.insert("010");
    t.insert("01");
    EXPECT_FALSE(t.is_prefix_free());
}

TEST(KraftTest, KraftSumOfEmptyVectorIsZero) {
    EXPECT_DOUBLE_EQ(kraft_sum({}), 0.0);
}

TEST(KraftTest, KraftSumOfSingletonLengthOne) {
    EXPECT_DOUBLE_EQ(kraft_sum({1}), 0.5);
}

TEST(KraftTest, KraftSumOfBalancedBinaryCode) {
    // Code with all four 2-bit codewords saturates Kraft.
    // Lengths 2, 2, 2, 2 give 4 * 2^-2 = 1.
    EXPECT_DOUBLE_EQ(kraft_sum({2, 2, 2, 2}), 1.0);
}

TEST(KraftTest, KraftSumOfExampleCode) {
    // Example from the post: A=0 (1 bit), B=10 (2 bits), C=110 (3 bits), D=111 (3 bits).
    // Sum: 1/2 + 1/4 + 1/8 + 1/8 = 1.
    EXPECT_DOUBLE_EQ(kraft_sum({1, 2, 3, 3}), 1.0);
}

TEST(KraftTest, KraftSumOfUnaryFirstFewIsLessThanOne) {
    // Unary lengths: 1, 2, 3, 4, 5. Sum: 1/2 + 1/4 + 1/8 + 1/16 + 1/32 = 31/32.
    EXPECT_NEAR(kraft_sum({1, 2, 3, 4, 5}), 31.0/32.0, 1e-12);
}

TEST(KraftTest, KraftSumExceedsOneForOverlongCode) {
    // 3 codewords of length 1 cannot fit prefix-freely (only 2 leaves at depth 1).
    // Sum: 3 * 1/2 = 1.5 > 1.
    EXPECT_GT(kraft_sum({1, 1, 1}), 1.0);
}

namespace {

// Helper: extract the lengths of all codewords in a code map.
void collect_lengths_recursive(const std::map<std::string, std::string>& code,
                               std::vector<std::size_t>& out) {
    for (const auto& [sym, codeword] : code) {
        out.push_back(codeword.size());
    }
}

}  // namespace

TEST(KraftTest, KraftHoldsForExampleCode) {
    // Code: A=0, B=10, C=110, D=111.
    BinaryTree t;
    t.insert("0");
    t.insert("10");
    t.insert("110");
    t.insert("111");
    ASSERT_TRUE(t.is_prefix_free());

    std::map<std::string, std::string> code{
        {"A", "0"}, {"B", "10"}, {"C", "110"}, {"D", "111"}
    };
    std::vector<std::size_t> lengths;
    collect_lengths_recursive(code, lengths);
    EXPECT_DOUBLE_EQ(kraft_sum(lengths), 1.0);
}

TEST(KraftTest, KraftHoldsForUnaryCodeUpToK) {
    // Unary: codeword for n is (n-1) zeros followed by a one. Lengths 1, 2, 3, ...
    constexpr std::size_t K = 10;
    BinaryTree t;
    std::vector<std::size_t> lengths;
    std::string codeword;
    for (std::size_t n = 1; n <= K; ++n) {
        codeword.assign(n - 1, '0');
        codeword += '1';
        t.insert(codeword);
        lengths.push_back(n);
    }
    ASSERT_TRUE(t.is_prefix_free());
    EXPECT_LT(kraft_sum(lengths), 1.0);
    EXPECT_NEAR(kraft_sum(lengths), 1.0 - std::ldexp(1.0, -static_cast<int>(K)), 1e-12);
}

TEST(KraftTest, NonPrefixFreeCodeStillSatisfiesKraftIfLengthsAllow) {
    // The lengths 1, 2 satisfy Kraft (sum = 0.75 <= 1), but the specific
    // assignment "0", "01" is NOT prefix-free. This verifies that Kraft is
    // about the LENGTH VECTOR, not the specific assignment.
    BinaryTree t;
    t.insert("0");
    t.insert("01");
    EXPECT_FALSE(t.is_prefix_free());

    std::vector<std::size_t> lengths{1, 2};
    EXPECT_DOUBLE_EQ(kraft_sum(lengths), 0.75);
}

TEST(KraftTest, TrieEmbeddingComputesSubtreeSizes) {
    // Code with lengths {1, 2, 3, 3}, l_max = 3.
    // Codeword of length 1 occupies 2^(3-1) = 4 leaves.
    // Codeword of length 2 occupies 2^(3-2) = 2 leaves.
    // Codewords of length 3 occupy 2^(3-3) = 1 leaf each.
    // Total: 4 + 2 + 1 + 1 = 8 = 2^3 (saturates).
    auto info = trie_embedding({1, 2, 3, 3});
    EXPECT_EQ(info.l_max, 3u);
    EXPECT_EQ(info.total_leaves, 8u);
    ASSERT_EQ(info.subtree_sizes.size(), 4u);
    EXPECT_EQ(info.subtree_sizes[0], 4u);
    EXPECT_EQ(info.subtree_sizes[1], 2u);
    EXPECT_EQ(info.subtree_sizes[2], 1u);
    EXPECT_EQ(info.subtree_sizes[3], 1u);
    EXPECT_EQ(info.occupied_leaves, 8u);
}

TEST(KraftTest, TrieEmbeddingForUnsaturatedCode) {
    // Lengths {2, 2, 3}, l_max = 3.
    // Subtree sizes: 2, 2, 1 = 5 leaves total.
    // Total: 8. Spare: 3.
    auto info = trie_embedding({2, 2, 3});
    EXPECT_EQ(info.l_max, 3u);
    EXPECT_EQ(info.total_leaves, 8u);
    EXPECT_EQ(info.occupied_leaves, 5u);
}

TEST(KraftTest, TrieEmbeddingForEmptyCode) {
    auto info = trie_embedding({});
    EXPECT_EQ(info.l_max, 0u);
    EXPECT_EQ(info.total_leaves, 1u);  // 2^0 = 1
    EXPECT_EQ(info.occupied_leaves, 0u);
    EXPECT_TRUE(info.subtree_sizes.empty());
}

TEST(KraftTest, IsKraftSatisfyingTrueForValidLengths) {
    EXPECT_TRUE(is_kraft_satisfying({1, 2, 3, 3}));
    EXPECT_TRUE(is_kraft_satisfying({2, 2, 2, 2}));  // saturates
    EXPECT_TRUE(is_kraft_satisfying({}));  // empty: sum is 0
}

TEST(KraftTest, IsKraftSatisfyingFalseForOverlongLengths) {
    EXPECT_FALSE(is_kraft_satisfying({1, 1, 1}));  // sum 1.5
    EXPECT_FALSE(is_kraft_satisfying({1, 2, 2, 2, 2}));  // 0.5 + 4*0.25 = 1.5
}
