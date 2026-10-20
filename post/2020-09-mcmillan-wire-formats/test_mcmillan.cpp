#include <gtest/gtest.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include "mcmillan.hpp"

using namespace mcmillan;

TEST(McMillanTest, BuildPrefixFreeCodeForBalancedLengths) {
    auto code = build_prefix_free_code({2, 2, 2, 2});
    ASSERT_EQ(code.size(), 4u);
    for (const auto& cw : code) {
        EXPECT_EQ(cw.size(), 2u);
    }
    std::set<std::string> distinct(code.begin(), code.end());
    EXPECT_EQ(distinct.size(), 4u);
}

TEST(McMillanTest, BuildPrefixFreeCodeForExampleLengths) {
    auto code = build_prefix_free_code({1, 2, 3, 3});
    ASSERT_EQ(code.size(), 4u);
    EXPECT_EQ(code[0].size(), 1u);
    EXPECT_EQ(code[1].size(), 2u);
    EXPECT_EQ(code[2].size(), 3u);
    EXPECT_EQ(code[3].size(), 3u);
}

TEST(McMillanTest, BuildPrefixFreeCodeProducesPrefixFreeCode) {
    auto code = build_prefix_free_code({1, 2, 3, 3});
    for (std::size_t i = 0; i < code.size(); ++i) {
        for (std::size_t j = 0; j < code.size(); ++j) {
            if (i == j) continue;
            const std::string& a = code[i];
            const std::string& b = code[j];
            if (a.size() > b.size()) continue;
            EXPECT_NE(b.substr(0, a.size()), a)
                << "Codeword " << a << " (idx " << i
                << ") is a prefix of " << b << " (idx " << j << ")";
        }
    }
}

TEST(McMillanTest, BuildPrefixFreeCodeForEmptyLengthsReturnsEmpty) {
    auto code = build_prefix_free_code({});
    EXPECT_TRUE(code.empty());
}

TEST(McMillanTest, IsPrefixFreeReturnsTrueForValidCode) {
    EXPECT_TRUE(is_prefix_free({"0", "10", "110", "111"}));
}

TEST(McMillanTest, IsPrefixFreeReturnsFalseWhenViolating) {
    EXPECT_FALSE(is_prefix_free({"0", "01"}));
    EXPECT_FALSE(is_prefix_free({"01", "010"}));
}

TEST(McMillanTest, IsPrefixFreeReturnsTrueForEmptyCode) {
    EXPECT_TRUE(is_prefix_free({}));
}

TEST(McMillanTest, BuildPrefixFreeCodeOutputIsActuallyPrefixFree) {
    for (const std::vector<std::size_t>& lengths : {
        std::vector<std::size_t>{1, 2, 3, 3},
        std::vector<std::size_t>{2, 2, 2, 2},
        std::vector<std::size_t>{3, 3, 3, 3, 3, 3, 3, 3},
        std::vector<std::size_t>{1, 3, 3, 3, 3},
    }) {
        auto code = build_prefix_free_code(lengths);
        EXPECT_TRUE(is_prefix_free(code))
            << "Constructed code is not prefix-free for some input lengths";
    }
}
