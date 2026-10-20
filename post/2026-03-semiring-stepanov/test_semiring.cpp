#include <gtest/gtest.h>
#include "semiring.hpp"

using namespace stepanov;

// =============================================================================
// Concept Compliance
// =============================================================================

TEST(SemiringTest, ConceptsSatisfied) {
    static_assert(Semiring<boolean_semiring>);
    static_assert(Semiring<tropical_min<double>>);
    static_assert(Semiring<tropical_min<float>>);
    static_assert(Semiring<tropical_max<double>>);
    static_assert(Semiring<tropical_max<float>>);
    static_assert(Semiring<bottleneck<double>>);
    static_assert(Semiring<bottleneck<float>>);
    static_assert(Semiring<counting>);
}

// =============================================================================
// Semiring Axiom Tests
// =============================================================================

// --- boolean_semiring ---

TEST(BooleanSemiringTest, AdditiveIdentity) {
    boolean_semiring t(true), f(false);
    EXPECT_EQ(t + zero(t), t);
    EXPECT_EQ(zero(t) + t, t);
    EXPECT_EQ(f + zero(f), f);
}

TEST(BooleanSemiringTest, MultiplicativeIdentity) {
    boolean_semiring t(true), f(false);
    EXPECT_EQ(t * one(t), t);
    EXPECT_EQ(one(t) * t, t);
    EXPECT_EQ(f * one(f), f);
}

TEST(BooleanSemiringTest, Annihilation) {
    boolean_semiring t(true), f(false);
    EXPECT_EQ(zero(t) * t, zero(t));
    EXPECT_EQ(t * zero(t), zero(t));
    EXPECT_EQ(zero(f) * f, zero(f));
}

TEST(BooleanSemiringTest, Commutativity) {
    boolean_semiring t(true), f(false);
    EXPECT_EQ(t + f, f + t);
    EXPECT_EQ(t * f, f * t);
}

TEST(BooleanSemiringTest, Distributivity) {
    boolean_semiring a(true), b(false), c(true);
    // a * (b + c) == (a * b) + (a * c)
    EXPECT_EQ(a * (b + c), (a * b) + (a * c));
    // (b + c) * a == (b * a) + (c * a)
    EXPECT_EQ((b + c) * a, (b * a) + (c * a));
}

// --- tropical_min<double> ---

TEST(TropicalMinTest, AdditiveIdentity) {
    tropical_min<double> a(3.0);
    EXPECT_EQ(a + zero(a), a);
    EXPECT_EQ(zero(a) + a, a);
}

TEST(TropicalMinTest, MultiplicativeIdentity) {
    tropical_min<double> a(3.0);
    EXPECT_EQ(a * one(a), a);
    EXPECT_EQ(one(a) * a, a);
}

TEST(TropicalMinTest, Annihilation) {
    tropical_min<double> a(3.0);
    EXPECT_EQ(zero(a) * a, zero(a));
    EXPECT_EQ(a * zero(a), zero(a));
}

TEST(TropicalMinTest, AddIsMin) {
    tropical_min<double> a(3.0), b(5.0);
    EXPECT_EQ((a + b).val, 3.0);
}

TEST(TropicalMinTest, MulIsPlus) {
    tropical_min<double> a(3.0), b(5.0);
    EXPECT_EQ((a * b).val, 8.0);
}

TEST(TropicalMinTest, Associativity) {
    tropical_min<double> a(2.0), b(5.0), c(3.0);
    EXPECT_EQ((a + b) + c, a + (b + c));
    EXPECT_EQ((a * b) * c, a * (b * c));
}

TEST(TropicalMinTest, Distributivity) {
    tropical_min<double> a(2.0), b(5.0), c(3.0);
    EXPECT_EQ(a * (b + c), (a * b) + (a * c));
}

// --- tropical_max<double> ---

TEST(TropicalMaxTest, AdditiveIdentity) {
    tropical_max<double> a(3.0);
    EXPECT_EQ(a + zero(a), a);
    EXPECT_EQ(zero(a) + a, a);
}

TEST(TropicalMaxTest, MultiplicativeIdentity) {
    tropical_max<double> a(3.0);
    EXPECT_EQ(a * one(a), a);
    EXPECT_EQ(one(a) * a, a);
}

TEST(TropicalMaxTest, Annihilation) {
    tropical_max<double> a(3.0);
    EXPECT_EQ(zero(a) * a, zero(a));
    EXPECT_EQ(a * zero(a), zero(a));
}

TEST(TropicalMaxTest, AddIsMax) {
    tropical_max<double> a(3.0), b(5.0);
    EXPECT_EQ((a + b).val, 5.0);
}

TEST(TropicalMaxTest, MulIsPlus) {
    tropical_max<double> a(3.0), b(5.0);
    EXPECT_EQ((a * b).val, 8.0);
}

TEST(TropicalMaxTest, Distributivity) {
    tropical_max<double> a(2.0), b(5.0), c(3.0);
    EXPECT_EQ(a * (b + c), (a * b) + (a * c));
}

// --- bottleneck<double> ---

TEST(BottleneckTest, AdditiveIdentity) {
    bottleneck<double> a(3.0);
    EXPECT_EQ(a + zero(a), a);
    EXPECT_EQ(zero(a) + a, a);
}

TEST(BottleneckTest, MultiplicativeIdentity) {
    bottleneck<double> a(3.0);
    EXPECT_EQ(a * one(a), a);
    EXPECT_EQ(one(a) * a, a);
}

TEST(BottleneckTest, Annihilation) {
    bottleneck<double> a(3.0);
    EXPECT_EQ(zero(a) * a, zero(a));
    EXPECT_EQ(a * zero(a), zero(a));
}

TEST(BottleneckTest, AddIsMax) {
    bottleneck<double> a(3.0), b(5.0);
    EXPECT_EQ((a + b).val, 5.0);
}

TEST(BottleneckTest, MulIsMin) {
    bottleneck<double> a(3.0), b(5.0);
    EXPECT_EQ((a * b).val, 3.0);
}

TEST(BottleneckTest, Distributivity) {
    bottleneck<double> a(2.0), b(5.0), c(3.0);
    EXPECT_EQ(a * (b + c), (a * b) + (a * c));
}

// --- counting ---

TEST(CountingTest, AdditiveIdentity) {
    counting a(5);
    EXPECT_EQ(a + zero(a), a);
    EXPECT_EQ(zero(a) + a, a);
}

TEST(CountingTest, MultiplicativeIdentity) {
    counting a(5);
    EXPECT_EQ(a * one(a), a);
    EXPECT_EQ(one(a) * a, a);
}

TEST(CountingTest, Annihilation) {
    counting a(5);
    EXPECT_EQ(zero(a) * a, zero(a));
    EXPECT_EQ(a * zero(a), zero(a));
}

TEST(CountingTest, Distributivity) {
    counting a(2), b(3), c(4);
    EXPECT_EQ(a * (b + c), (a * b) + (a * c));
    EXPECT_EQ((b + c) * a, (b * a) + (c * a));
}

// =============================================================================
// Matrix Arithmetic Tests
// =============================================================================

TEST(MatrixTest, DefaultIsZero) {
    matrix<counting, 3> m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            EXPECT_EQ(m(i, j), zero(counting{}));
}

TEST(MatrixTest, Identity) {
    auto I = matrix<counting, 3>::identity();
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            EXPECT_EQ(I(i, j), i == j ? one(counting{}) : zero(counting{}));
}

TEST(MatrixTest, IdentityIsMultiplicativeUnit) {
    auto I = matrix<counting, 3>::identity();
    matrix<counting, 3> A;
    A(0, 1) = counting(2);
    A(1, 2) = counting(3);
    A(2, 0) = counting(1);

    EXPECT_EQ(A * I, A);
    EXPECT_EQ(I * A, A);
}

TEST(MatrixTest, Addition) {
    matrix<counting, 2> A, B;
    A(0, 0) = counting(1); A(0, 1) = counting(2);
    A(1, 0) = counting(3); A(1, 1) = counting(4);
    B(0, 0) = counting(5); B(0, 1) = counting(6);
    B(1, 0) = counting(7); B(1, 1) = counting(8);

    auto C = A + B;
    EXPECT_EQ(C(0, 0), counting(6));
    EXPECT_EQ(C(0, 1), counting(8));
    EXPECT_EQ(C(1, 0), counting(10));
    EXPECT_EQ(C(1, 1), counting(12));
}

TEST(MatrixTest, Multiplication) {
    // [1 2] * [5 6] = [1*5+2*7  1*6+2*8] = [19 22]
    // [3 4]   [7 8]   [3*5+4*7  3*6+4*8]   [43 50]
    matrix<counting, 2> A, B;
    A(0, 0) = counting(1); A(0, 1) = counting(2);
    A(1, 0) = counting(3); A(1, 1) = counting(4);
    B(0, 0) = counting(5); B(0, 1) = counting(6);
    B(1, 0) = counting(7); B(1, 1) = counting(8);

    auto C = A * B;
    EXPECT_EQ(C(0, 0), counting(19));
    EXPECT_EQ(C(0, 1), counting(22));
    EXPECT_EQ(C(1, 0), counting(43));
    EXPECT_EQ(C(1, 1), counting(50));
}

// =============================================================================
// Graph Algorithm Tests
//
// Test graph (4 nodes):
//
//     0 --2--> 1 --3--> 3
//     |        |
//     5        1
//     v        v
//     2 --4--> 3
//
// Edges: 0->1 (2), 0->2 (5), 1->3 (3), 1->2 (1), 2->3 (4)
//
// =============================================================================

// --- Boolean semiring: reachability ---

TEST(GraphTest, BooleanReachability) {
    using B = boolean_semiring;
    auto adj = adjacency<B, 4>({
        {0, 1, B(true)},
        {0, 2, B(true)},
        {1, 3, B(true)},
        {1, 2, B(true)},
        {2, 3, B(true)},
    });

    auto reach = closure(adj);

    // 0 can reach everything
    EXPECT_TRUE(reach(0, 1).val);
    EXPECT_TRUE(reach(0, 2).val);
    EXPECT_TRUE(reach(0, 3).val);

    // 1 can reach 2 and 3
    EXPECT_TRUE(reach(1, 2).val);
    EXPECT_TRUE(reach(1, 3).val);

    // 3 is a sink: cannot reach anything
    EXPECT_FALSE(reach(3, 0).val);
    EXPECT_FALSE(reach(3, 1).val);
    EXPECT_FALSE(reach(3, 2).val);

    // Self-reachability (from identity in closure)
    EXPECT_TRUE(reach(0, 0).val);
    EXPECT_TRUE(reach(3, 3).val);
}

// --- Tropical min: shortest paths ---

TEST(GraphTest, TropicalMinShortestPaths) {
    using T = tropical_min<double>;
    auto adj = adjacency<T, 4>({
        {0, 1, T(2.0)},
        {0, 2, T(5.0)},
        {1, 3, T(3.0)},
        {1, 2, T(1.0)},
        {2, 3, T(4.0)},
    });

    auto dist = closure(adj);

    // 0->1: direct edge, weight 2
    EXPECT_DOUBLE_EQ(dist(0, 1).val, 2.0);

    // 0->2: direct=5, via 1=2+1=3. Shortest is 3.
    EXPECT_DOUBLE_EQ(dist(0, 2).val, 3.0);

    // 0->3: via 1=2+3=5, via 2=5+4=9, via 1,2=2+1+4=7. Shortest is 5.
    EXPECT_DOUBLE_EQ(dist(0, 3).val, 5.0);

    // 1->3: direct=3, via 2=1+4=5. Shortest is 3.
    EXPECT_DOUBLE_EQ(dist(1, 3).val, 3.0);

    // No path from 3 to 0: should be infinity
    EXPECT_EQ(dist(3, 0), zero(T{}));
}

// --- Tropical max: longest paths ---

TEST(GraphTest, TropicalMaxLongestPaths) {
    using T = tropical_max<double>;
    auto adj = adjacency<T, 4>({
        {0, 1, T(2.0)},
        {0, 2, T(5.0)},
        {1, 3, T(3.0)},
        {1, 2, T(1.0)},
        {2, 3, T(4.0)},
    });

    auto dist = closure(adj);

    // 0->3: paths are 2+3=5, 5+4=9, 2+1+4=7. Longest is 9.
    EXPECT_DOUBLE_EQ(dist(0, 3).val, 9.0);

    // 0->2: direct=5, via 1=2+1=3. Longest is 5.
    EXPECT_DOUBLE_EQ(dist(0, 2).val, 5.0);

    // 1->3: direct=3, via 2=1+4=5. Longest is 5.
    EXPECT_DOUBLE_EQ(dist(1, 3).val, 5.0);
}

// --- Bottleneck: widest paths ---

TEST(GraphTest, BottleneckWidestPaths) {
    using B = bottleneck<double>;
    auto adj = adjacency<B, 4>({
        {0, 1, B(2.0)},
        {0, 2, B(5.0)},
        {1, 3, B(3.0)},
        {1, 2, B(1.0)},
        {2, 3, B(4.0)},
    });

    auto cap = closure(adj);

    // 0->3: paths with bottlenecks min(2,3)=2, min(5,4)=4, min(2,1,4)=1.
    // Widest (max of bottlenecks) is 4.
    EXPECT_DOUBLE_EQ(cap(0, 3).val, 4.0);

    // 0->2: direct=5, via 1=min(2,1)=1. Widest is 5.
    EXPECT_DOUBLE_EQ(cap(0, 2).val, 5.0);

    // 0->1: direct=2 (only path). Widest is 2.
    EXPECT_DOUBLE_EQ(cap(0, 1).val, 2.0);
}

// --- Counting: number of paths ---

TEST(GraphTest, CountingPaths) {
    using C = counting;
    auto adj = adjacency<C, 4>({
        {0, 1, C(1)},
        {0, 2, C(1)},
        {1, 3, C(1)},
        {1, 2, C(1)},
        {2, 3, C(1)},
    });

    auto paths = closure(adj);

    // 0->1: 1 path (direct)
    EXPECT_EQ(paths(0, 1).val, 1);

    // 0->2: 2 paths (direct, via 1)
    EXPECT_EQ(paths(0, 2).val, 2);

    // 0->3: 3 paths (0->1->3, 0->2->3, 0->1->2->3)
    EXPECT_EQ(paths(0, 3).val, 3);

    // 1->3: 2 paths (direct, via 2)
    EXPECT_EQ(paths(1, 3).val, 2);
}

// =============================================================================
// Power Algorithm Tests (k-hop paths)
// =============================================================================

TEST(PowerTest, IdentityAtZero) {
    matrix<counting, 3> A;
    A(0, 1) = counting(1);
    A(1, 2) = counting(1);

    auto result = power(A, std::size_t(0));
    auto expected = matrix<counting, 3>::identity();
    EXPECT_EQ(result, expected);
}

TEST(PowerTest, BaseAtOne) {
    matrix<counting, 3> A;
    A(0, 1) = counting(1);
    A(1, 2) = counting(1);

    auto result = power(A, std::size_t(1));
    EXPECT_EQ(result, A);
}

TEST(PowerTest, CountingKHopPaths) {
    // Linear chain: 0->1->2->3
    using C = counting;
    auto adj = adjacency<C, 4>({
        {0, 1, C(1)},
        {1, 2, C(1)},
        {2, 3, C(1)},
    });

    // A^1: direct edges (1-hop paths)
    auto a1 = power(adj, std::size_t(1));
    EXPECT_EQ(a1(0, 1).val, 1);
    EXPECT_EQ(a1(0, 2).val, 0);
    EXPECT_EQ(a1(0, 3).val, 0);

    // A^2: 2-hop paths
    auto a2 = power(adj, std::size_t(2));
    EXPECT_EQ(a2(0, 2).val, 1);  // 0->1->2
    EXPECT_EQ(a2(0, 3).val, 0);  // no 2-hop path from 0 to 3

    // A^3: 3-hop paths
    auto a3 = power(adj, std::size_t(3));
    EXPECT_EQ(a3(0, 3).val, 1);  // 0->1->2->3
}

TEST(PowerTest, BooleanKHopReachability) {
    using B = boolean_semiring;
    auto adj = adjacency<B, 3>({
        {0, 1, B(true)},
        {1, 2, B(true)},
    });

    // A^1: can 0 reach 2 in 1 hop? No.
    auto a1 = power(adj, std::size_t(1));
    EXPECT_FALSE(a1(0, 2).val);

    // A^2: can 0 reach 2 in 2 hops? Yes (0->1->2).
    auto a2 = power(adj, std::size_t(2));
    EXPECT_TRUE(a2(0, 2).val);
}

TEST(PowerTest, TropicalMinKHop) {
    using T = tropical_min<double>;
    // Complete graph on 3 nodes with different weights
    auto adj = adjacency<T, 3>({
        {0, 1, T(1.0)},
        {1, 2, T(2.0)},
        {0, 2, T(10.0)},
    });

    // A^1: shortest 1-hop path from 0 to 2 is 10
    auto a1 = power(adj, std::size_t(1));
    EXPECT_DOUBLE_EQ(a1(0, 2).val, 10.0);

    // A^2: shortest 2-hop path from 0 to 2 is 1+2=3
    auto a2 = power(adj, std::size_t(2));
    EXPECT_DOUBLE_EQ(a2(0, 2).val, 3.0);
}

TEST(PowerTest, CountingDiamondGraph) {
    // Diamond graph: 0->1, 0->2, 1->3, 2->3
    using C = counting;
    auto adj = adjacency<C, 4>({
        {0, 1, C(1)},
        {0, 2, C(1)},
        {1, 3, C(1)},
        {2, 3, C(1)},
    });

    // A^2: 2-hop paths from 0 to 3: 0->1->3 and 0->2->3 = 2 paths
    auto a2 = power(adj, std::size_t(2));
    EXPECT_EQ(a2(0, 3).val, 2);
}

// =============================================================================
// Adjacency convenience function
// =============================================================================

TEST(AdjacencyTest, BuildsCorrectMatrix) {
    using T = tropical_min<double>;
    auto adj = adjacency<T, 3>({
        {0, 1, T(2.5)},
        {1, 2, T(3.7)},
    });

    EXPECT_DOUBLE_EQ(adj(0, 1).val, 2.5);
    EXPECT_DOUBLE_EQ(adj(1, 2).val, 3.7);
    EXPECT_EQ(adj(0, 2), zero(T{}));  // no edge = infinity
    EXPECT_EQ(adj(2, 0), zero(T{}));
}
