#include <gtest/gtest.h>
#include "accumulator.hpp"

#include <cmath>
#include <vector>

using namespace stepanov;

// =============================================================================
// Concept compliance
// =============================================================================

TEST(AccumulatorTest, ConceptsSatisfied) {
    static_assert(Accumulator<kbn_sum<double>>);
    static_assert(Accumulator<kbn_sum<float>>);
    static_assert(Accumulator<welford<double>>);
    static_assert(Accumulator<min_accumulator<double>>);
    static_assert(Accumulator<min_accumulator<int>>);
    static_assert(Accumulator<max_accumulator<double>>);
    static_assert(Accumulator<max_accumulator<int>>);
    static_assert(Accumulator<count_accumulator<double>>);

    // Composition satisfies the concept too
    static_assert(Accumulator<parallel<kbn_sum<double>, welford<double>>>);
    static_assert(Accumulator<parallel<kbn_sum<double>, min_accumulator<double>>>);

    // Nested composition
    static_assert(Accumulator<
        parallel<parallel<kbn_sum<double>, min_accumulator<double>>,
                 max_accumulator<double>>>);
}

// =============================================================================
// kbn_sum tests
// =============================================================================

TEST(KbnSumTest, BasicAccumulation) {
    kbn_sum<double> s;
    s += 1.0;
    s += 2.0;
    s += 3.0;
    EXPECT_DOUBLE_EQ(s.eval(), 6.0);
}

TEST(KbnSumTest, CompensatedAccuracy) {
    // Classic test: large value then many small values
    kbn_sum<double> kbn;
    double naive = 0.0;

    kbn += 1.0;
    naive += 1.0;
    for (int i = 0; i < 1'000'000; ++i) {
        kbn += 1e-10;
        naive += 1e-10;
    }

    double expected = 1.0 + 1'000'000 * 1e-10;
    EXPECT_NEAR(kbn.eval(), expected, 1e-12);
    // Naive summation has larger error
    EXPECT_GT(std::abs(naive - expected), std::abs(kbn.eval() - expected));
}

TEST(KbnSumTest, Identity) {
    kbn_sum<double> empty;
    EXPECT_DOUBLE_EQ(empty.eval(), 0.0);

    kbn_sum<double> s;
    s += 42.0;
    s += empty;
    EXPECT_DOUBLE_EQ(s.eval(), 42.0);
}

TEST(KbnSumTest, Combination) {
    kbn_sum<double> a, b;
    a += 1.0;
    a += 2.0;
    b += 3.0;
    b += 4.0;
    a += b;
    EXPECT_DOUBLE_EQ(a.eval(), 10.0);
}

TEST(KbnSumTest, ExplicitConversion) {
    kbn_sum<double> s;
    s += 3.14;
    EXPECT_DOUBLE_EQ(static_cast<double>(s), 3.14);
}

TEST(KbnSumTest, Constexpr) {
    constexpr auto s = [] {
        kbn_sum<double> acc;
        acc += 1.0;
        acc += 2.0;
        acc += 3.0;
        return acc.eval();
    }();
    EXPECT_DOUBLE_EQ(s, 6.0);
}

// =============================================================================
// welford tests
// =============================================================================

TEST(WelfordTest, MeanOfSequence) {
    welford<double> w;
    for (double v : {1.0, 2.0, 3.0, 4.0, 5.0})
        w += v;
    EXPECT_DOUBLE_EQ(w.mean(), 3.0);
    EXPECT_EQ(w.size(), 5u);
}

TEST(WelfordTest, Variance) {
    welford<double> w;
    for (double v : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0})
        w += v;
    EXPECT_DOUBLE_EQ(w.mean(), 5.0);
    EXPECT_DOUBLE_EQ(w.variance(), 4.0);
    EXPECT_NEAR(w.sample_variance(), 32.0 / 7.0, 1e-12);
}

TEST(WelfordTest, ParallelCombination) {
    // Split data, combine — should match single pass
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

    welford<double> full;
    for (auto v : data) full += v;

    welford<double> left, right;
    for (std::size_t i = 0; i < 4; ++i) left += data[i];
    for (std::size_t i = 4; i < 8; ++i) right += data[i];
    left += right;

    EXPECT_NEAR(left.mean(), full.mean(), 1e-12);
    EXPECT_NEAR(left.variance(), full.variance(), 1e-12);
    EXPECT_EQ(left.size(), full.size());
}

TEST(WelfordTest, Identity) {
    welford<double> empty;
    EXPECT_DOUBLE_EQ(empty.mean(), 0.0);
    EXPECT_DOUBLE_EQ(empty.variance(), 0.0);
    EXPECT_EQ(empty.size(), 0u);

    welford<double> w;
    w += 5.0;
    w += empty;
    EXPECT_DOUBLE_EQ(w.mean(), 5.0);
}

TEST(WelfordTest, EvalReturnsMean) {
    welford<double> w;
    for (double v : {1.0, 2.0, 3.0})
        w += v;
    EXPECT_DOUBLE_EQ(w.eval(), w.mean());
}

// =============================================================================
// min/max/count tests
// =============================================================================

TEST(MinMaxCountTest, MinAccumulator) {
    min_accumulator<double> m;
    EXPECT_TRUE(m.empty());
    m += 5.0;
    m += 2.0;
    m += 8.0;
    m += 1.0;
    m += 9.0;
    EXPECT_DOUBLE_EQ(m.eval(), 1.0);
    EXPECT_FALSE(m.empty());
}

TEST(MinMaxCountTest, MaxAccumulator) {
    max_accumulator<double> m;
    EXPECT_TRUE(m.empty());
    m += 5.0;
    m += 2.0;
    m += 8.0;
    m += 1.0;
    m += 9.0;
    EXPECT_DOUBLE_EQ(m.eval(), 9.0);
    EXPECT_FALSE(m.empty());
}

TEST(MinMaxCountTest, CountAccumulator) {
    count_accumulator<double> c;
    EXPECT_EQ(c.eval(), 0u);
    c += 1.0;
    c += 2.0;
    c += 3.0;
    EXPECT_EQ(c.eval(), 3u);
}

TEST(MinMaxCountTest, MinMaxCombine) {
    min_accumulator<double> a, b;
    a += 5.0;
    a += 3.0;
    b += 7.0;
    b += 1.0;
    a += b;
    EXPECT_DOUBLE_EQ(a.eval(), 1.0);

    max_accumulator<double> c, d;
    c += 5.0;
    c += 3.0;
    d += 7.0;
    d += 1.0;
    c += d;
    EXPECT_DOUBLE_EQ(c.eval(), 7.0);
}

TEST(MinMaxCountTest, CountCombine) {
    count_accumulator<double> a, b;
    a += 1.0;
    a += 2.0;
    b += 3.0;
    b += 4.0;
    b += 5.0;
    a += b;
    EXPECT_EQ(a.eval(), 5u);
}

TEST(MinMaxCountTest, IntegerMinMax) {
    constexpr auto mn = [] {
        min_accumulator<int> m;
        m += 5;
        m += 3;
        m += 7;
        return m.eval();
    }();
    EXPECT_EQ(mn, 3);

    constexpr auto mx = [] {
        max_accumulator<int> m;
        m += 5;
        m += 3;
        m += 7;
        return m.eval();
    }();
    EXPECT_EQ(mx, 7);
}

// =============================================================================
// Parallel composition tests
// =============================================================================

TEST(ParallelTest, TwoAccumulators) {
    parallel<kbn_sum<double>, min_accumulator<double>> stats;
    for (double v : {3.0, 1.0, 4.0, 1.0, 5.0})
        stats += v;

    auto [sum, min] = stats.eval();
    EXPECT_DOUBLE_EQ(sum, 14.0);
    EXPECT_DOUBLE_EQ(min, 1.0);
}

TEST(ParallelTest, NestedComposition) {
    // Three statistics in one pass via nesting
    using inner = parallel<kbn_sum<double>, min_accumulator<double>>;
    using outer = parallel<inner, max_accumulator<double>>;

    outer stats;
    for (double v : {3.0, 1.0, 4.0, 1.0, 5.0})
        stats += v;

    auto [inner_result, max] = stats.eval();
    auto [sum, min] = inner_result;
    EXPECT_DOUBLE_EQ(sum, 14.0);
    EXPECT_DOUBLE_EQ(min, 1.0);
    EXPECT_DOUBLE_EQ(max, 5.0);
}

TEST(ParallelTest, PipeOperator) {
    auto stats = kbn_sum<double>{} | welford<double>{};
    for (double v : {1.0, 2.0, 3.0, 4.0, 5.0})
        stats += v;

    EXPECT_DOUBLE_EQ(stats.first().eval(), 15.0);
    EXPECT_DOUBLE_EQ(stats.second().mean(), 3.0);
}

TEST(ParallelTest, ChainedPipe) {
    // a | b | c creates parallel<parallel<a, b>, c>
    auto stats = kbn_sum<double>{} | min_accumulator<double>{} | max_accumulator<double>{};
    for (double v : {3.0, 1.0, 4.0, 1.0, 5.0, 9.0})
        stats += v;

    auto [inner, max] = stats.eval();
    auto [sum, min] = inner;
    EXPECT_DOUBLE_EQ(sum, 23.0);
    EXPECT_DOUBLE_EQ(min, 1.0);
    EXPECT_DOUBLE_EQ(max, 9.0);
}

TEST(ParallelTest, Combination) {
    parallel<kbn_sum<double>, count_accumulator<double>> a, b;
    a += 1.0;
    a += 2.0;
    b += 3.0;
    b += 4.0;
    a += b;

    auto [sum, count] = a.eval();
    EXPECT_DOUBLE_EQ(sum, 10.0);
    EXPECT_EQ(count, 4u);
}

// =============================================================================
// Monoid law tests
// =============================================================================

TEST(MonoidLawsTest, KbnSumIdentity) {
    kbn_sum<double> e;   // identity
    kbn_sum<double> a;
    a += 1.0;
    a += 2.0;
    a += 3.0;

    // Right identity: a += e
    auto ar = a;
    ar += e;
    EXPECT_DOUBLE_EQ(ar.eval(), a.eval());

    // Left identity: e += a
    auto el = e;
    el += a;
    EXPECT_DOUBLE_EQ(el.eval(), a.eval());
}

TEST(MonoidLawsTest, WelfordIdentity) {
    welford<double> e;
    welford<double> a;
    a += 1.0;
    a += 2.0;
    a += 3.0;

    auto ar = a;
    ar += e;
    EXPECT_DOUBLE_EQ(ar.mean(), a.mean());
    EXPECT_DOUBLE_EQ(ar.variance(), a.variance());

    auto el = e;
    el += a;
    EXPECT_DOUBLE_EQ(el.mean(), a.mean());
    EXPECT_DOUBLE_EQ(el.variance(), a.variance());
}

TEST(MonoidLawsTest, KbnSumAssociativity) {
    // (a += b) += c  should equal  a += (b += c)
    std::vector<double> va = {1.0, 2.0}, vb = {3.0, 4.0}, vc = {5.0, 6.0};

    kbn_sum<double> a1, b1, c1;
    for (auto v : va) a1 += v;
    for (auto v : vb) b1 += v;
    for (auto v : vc) c1 += v;

    // (a + b) + c
    auto ab = a1;
    ab += b1;
    auto abc_left = ab;
    abc_left += c1;

    // a + (b + c)
    auto bc = b1;
    bc += c1;
    auto abc_right = a1;
    abc_right += bc;

    EXPECT_DOUBLE_EQ(abc_left.eval(), abc_right.eval());
    EXPECT_DOUBLE_EQ(abc_left.eval(), 21.0);
}

TEST(MonoidLawsTest, WelfordAssociativity) {
    std::vector<double> va = {1.0, 2.0}, vb = {3.0, 4.0}, vc = {5.0, 6.0};

    welford<double> a, b, c;
    for (auto v : va) a += v;
    for (auto v : vb) b += v;
    for (auto v : vc) c += v;

    auto ab = a;
    ab += b;
    auto abc_left = ab;
    abc_left += c;

    auto bc = b;
    bc += c;
    auto abc_right = a;
    abc_right += bc;

    EXPECT_NEAR(abc_left.mean(), abc_right.mean(), 1e-12);
    EXPECT_NEAR(abc_left.variance(), abc_right.variance(), 1e-12);
}

TEST(MonoidLawsTest, MinAssociativity) {
    min_accumulator<double> a, b, c;
    a += 5.0;
    b += 2.0;
    c += 8.0;

    auto ab = a;
    ab += b;
    auto abc_left = ab;
    abc_left += c;

    auto bc = b;
    bc += c;
    auto abc_right = a;
    abc_right += bc;

    EXPECT_DOUBLE_EQ(abc_left.eval(), abc_right.eval());
    EXPECT_DOUBLE_EQ(abc_left.eval(), 2.0);
}

// =============================================================================
// fold tests
// =============================================================================

TEST(FoldTest, KbnSum) {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto result = fold<kbn_sum<double>>(data.begin(), data.end());
    EXPECT_DOUBLE_EQ(result.eval(), 15.0);
}

TEST(FoldTest, Welford) {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto result = fold<welford<double>>(data.begin(), data.end());
    EXPECT_DOUBLE_EQ(result.mean(), 3.0);
    EXPECT_EQ(result.size(), 5u);
}

TEST(FoldTest, ParallelFold) {
    std::vector<double> data = {3.0, 1.0, 4.0, 1.0, 5.0, 9.0};
    using stats = parallel<kbn_sum<double>,
                           parallel<min_accumulator<double>,
                                    max_accumulator<double>>>;

    auto result = fold<stats>(data.begin(), data.end());
    auto [sum, minmax] = result.eval();
    auto [mn, mx] = minmax;

    EXPECT_DOUBLE_EQ(sum, 23.0);
    EXPECT_DOUBLE_EQ(mn, 1.0);
    EXPECT_DOUBLE_EQ(mx, 9.0);
}

TEST(FoldTest, EmptyRange) {
    std::vector<double> empty;
    auto result = fold<kbn_sum<double>>(empty.begin(), empty.end());
    EXPECT_DOUBLE_EQ(result.eval(), 0.0);
}

TEST(FoldTest, SingleElement) {
    std::vector<double> data = {42.0};
    auto result = fold<welford<double>>(data.begin(), data.end());
    EXPECT_DOUBLE_EQ(result.mean(), 42.0);
    EXPECT_DOUBLE_EQ(result.variance(), 0.0);
    EXPECT_EQ(result.size(), 1u);
}
