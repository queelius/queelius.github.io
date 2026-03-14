#include <gtest/gtest.h>
#include "free_algebra.hpp"

#include <string>
#include <limits>

using namespace stepanov;

// =============================================================================
// Concept Compliance
// =============================================================================

TEST(FreeAlgebraTest, MonoidConceptSatisfied) {
    static_assert(Monoid<free_monoid<int>>);
    static_assert(Monoid<free_monoid<std::string>>);
    static_assert(Monoid<free_commutative_monoid<int>>);
    static_assert(Monoid<free_commutative_monoid<std::string>>);
    static_assert(Monoid<additive<int>>);
    static_assert(Monoid<additive<double>>);
    static_assert(Monoid<multiplicative<int>>);
    static_assert(Monoid<max_monoid<int>>);
    static_assert(Monoid<string_concat>);
}

// =============================================================================
// Free Monoid Laws
// =============================================================================

TEST(FreeMonoidTest, AssociativityOfConcatenation) {
    free_monoid<int> a{1, 2};
    free_monoid<int> b{3};
    free_monoid<int> c{4, 5};

    auto ab_c = op(op(a, b), c);
    auto a_bc = op(a, op(b, c));
    EXPECT_EQ(ab_c, a_bc);
}

TEST(FreeMonoidTest, IdentityElement) {
    free_monoid<int> a{1, 2, 3};
    auto e = identity(a);

    EXPECT_EQ(op(a, e), a);
    EXPECT_EQ(op(e, a), a);
    EXPECT_TRUE(e.empty());
}

TEST(FreeMonoidTest, NonCommutativity) {
    // The free monoid is NOT commutative. This is the point.
    free_monoid<int> a{1, 2};
    free_monoid<int> b{3, 4};

    EXPECT_NE(op(a, b), op(b, a));
}

TEST(FreeMonoidTest, NonIdempotency) {
    // [a] op [a] = [a, a], not [a].
    free_monoid<int> a{1};
    EXPECT_NE(op(a, a), a);
    EXPECT_EQ(op(a, a), (free_monoid<int>{1, 1}));
}

TEST(FreeMonoidTest, SingletonConstructor) {
    free_monoid<int> a(42);
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(a.elements()[0], 42);
}

TEST(FreeMonoidTest, ConcatenationContents) {
    free_monoid<int> a{1, 2};
    free_monoid<int> b{3, 4, 5};
    auto c = op(a, b);

    std::vector<int> expected{1, 2, 3, 4, 5};
    EXPECT_EQ(c.elements(), expected);
}

// =============================================================================
// Universal Property Tests
// =============================================================================

TEST(UniversalPropertyTest, ExtendOnEmptyList) {
    // extend(f, []) = identity
    free_monoid<int> empty{};
    auto result = extend<additive<int>>(
        [](int x) { return additive<int>(x); }, empty);
    EXPECT_EQ(result, identity(additive<int>{}));
}

TEST(UniversalPropertyTest, ExtendOnSingleton) {
    // extend(f, [x]) = f(x)
    free_monoid<int> single(42);
    auto result = extend<additive<int>>(
        [](int x) { return additive<int>(x); }, single);
    EXPECT_EQ(result, additive<int>(42));
}

TEST(UniversalPropertyTest, ExtendComputesCorrectly) {
    // extend(f, [a, b, c]) = op(op(f(a), f(b)), f(c))
    free_monoid<int> xs{1, 2, 3};
    auto f = [](int x) { return additive<int>(x); };

    auto via_extend = extend<additive<int>>(f, xs);
    auto manual = op(op(f(1), f(2)), f(3));

    EXPECT_EQ(via_extend, manual);
}

TEST(UniversalPropertyTest, ExtendPreservesOp) {
    // extend(f, op(xs, ys)) == op(extend(f, xs), extend(f, ys))
    // This is the homomorphism property.
    free_monoid<int> xs{1, 2, 3};
    free_monoid<int> ys{4, 5};
    auto f = [](int x) { return additive<int>(x); };

    auto lhs = extend<additive<int>>(f, op(xs, ys));
    auto rhs = op(extend<additive<int>>(f, xs),
                   extend<additive<int>>(f, ys));

    EXPECT_EQ(lhs, rhs);
}

TEST(UniversalPropertyTest, ExtendPreservesOpMultiplicative) {
    // Same test, different target monoid.
    free_monoid<int> xs{2, 3};
    free_monoid<int> ys{4, 5};
    auto f = [](int x) { return multiplicative<int>(x); };

    auto lhs = extend<multiplicative<int>>(f, op(xs, ys));
    auto rhs = op(extend<multiplicative<int>>(f, xs),
                   extend<multiplicative<int>>(f, ys));

    EXPECT_EQ(lhs, rhs);
}

TEST(UniversalPropertyTest, ExtendPreservesIdentity) {
    // extend(f, identity) = identity
    free_monoid<int> empty{};
    auto f = [](int x) { return multiplicative<int>(x); };

    auto result = extend<multiplicative<int>>(f, empty);
    EXPECT_EQ(result, identity(multiplicative<int>{}));
}

// =============================================================================
// Concrete Homomorphisms
// =============================================================================

TEST(HomomorphismTest, SumViaExtend) {
    // sum([1, 2, 3]) = 6
    free_monoid<int> xs{1, 2, 3};
    auto result = extend<additive<int>>(
        [](int x) { return additive<int>(x); }, xs);
    EXPECT_EQ(result.value(), 6);
}

TEST(HomomorphismTest, ProductViaExtend) {
    // product([2, 3, 4]) = 24
    free_monoid<int> xs{2, 3, 4};
    auto result = extend<multiplicative<int>>(
        [](int x) { return multiplicative<int>(x); }, xs);
    EXPECT_EQ(result.value(), 24);
}

TEST(HomomorphismTest, LengthViaExtend) {
    // length is the homomorphism that maps every element to 1 under addition.
    free_monoid<std::string> xs{"a", "bb", "ccc"};
    auto result = extend<additive<int>>(
        [](const std::string&) { return additive<int>(1); }, xs);
    EXPECT_EQ(result.value(), 3);
}

TEST(HomomorphismTest, MaxViaExtend) {
    // max([3, 1, 4, 1, 5]) = 5
    free_monoid<int> xs{3, 1, 4, 1, 5};
    auto result = extend<max_monoid<int>>(
        [](int x) { return max_monoid<int>(x); }, xs);
    EXPECT_EQ(result.value(), 5);
}

TEST(HomomorphismTest, StringConcatViaExtend) {
    free_monoid<std::string> xs{"hello", " ", "world"};
    auto result = extend<string_concat>(
        [](const std::string& s) { return string_concat(s); }, xs);
    EXPECT_EQ(result.value, "hello world");
}

TEST(HomomorphismTest, EmptySum) {
    free_monoid<int> empty{};
    auto result = extend<additive<int>>(
        [](int x) { return additive<int>(x); }, empty);
    EXPECT_EQ(result.value(), 0);
}

TEST(HomomorphismTest, EmptyProduct) {
    free_monoid<int> empty{};
    auto result = extend<multiplicative<int>>(
        [](int x) { return multiplicative<int>(x); }, empty);
    EXPECT_EQ(result.value(), 1);
}

// =============================================================================
// Collapse Tests (free_monoid<M> -> M)
// =============================================================================

TEST(CollapseTest, CollapseAdditiveMonoid) {
    // collapse flattens a list of additive values using op
    free_monoid<additive<int>> xs{additive<int>(10),
                                   additive<int>(20),
                                   additive<int>(30)};
    auto result = collapse<additive<int>>(xs);
    EXPECT_EQ(result.value(), 60);
}

TEST(CollapseTest, CollapseMultiplicativeMonoid) {
    free_monoid<multiplicative<int>> xs{multiplicative<int>(2),
                                         multiplicative<int>(3),
                                         multiplicative<int>(5)};
    auto result = collapse<multiplicative<int>>(xs);
    EXPECT_EQ(result.value(), 30);
}

TEST(CollapseTest, CollapseEmpty) {
    free_monoid<additive<int>> empty{};
    auto result = collapse<additive<int>>(empty);
    EXPECT_EQ(result.value(), 0);
}

TEST(CollapseTest, CollapseStringConcat) {
    free_monoid<string_concat> xs{string_concat("foo"),
                                    string_concat("bar")};
    auto result = collapse<string_concat>(xs);
    EXPECT_EQ(result.value, "foobar");
}

// =============================================================================
// Free Commutative Monoid Tests
// =============================================================================

TEST(FreeCommutativeMonoidTest, Commutativity) {
    // Order does not matter.
    free_commutative_monoid<int> ab{1, 2};
    free_commutative_monoid<int> ba{2, 1};
    EXPECT_EQ(ab, ba);
}

TEST(FreeCommutativeMonoidTest, NonIdempotency) {
    // Multiplicity matters.
    free_commutative_monoid<int> a{1};
    free_commutative_monoid<int> aa{1, 1};
    EXPECT_NE(a, aa);
}

TEST(FreeCommutativeMonoidTest, Associativity) {
    free_commutative_monoid<int> a{1, 2};
    free_commutative_monoid<int> b{3};
    free_commutative_monoid<int> c{2, 4};

    auto ab_c = op(op(a, b), c);
    auto a_bc = op(a, op(b, c));
    EXPECT_EQ(ab_c, a_bc);
}

TEST(FreeCommutativeMonoidTest, IdentityElement) {
    free_commutative_monoid<int> a{1, 2, 3};
    auto e = identity(a);

    EXPECT_EQ(op(a, e), a);
    EXPECT_EQ(op(e, a), a);
    EXPECT_TRUE(e.empty());
}

TEST(FreeCommutativeMonoidTest, UnionOperation) {
    free_commutative_monoid<int> a{1, 2, 2};
    free_commutative_monoid<int> b{2, 3};
    auto c = op(a, b);

    EXPECT_EQ(c.count(1), 1u);
    EXPECT_EQ(c.count(2), 3u);  // 2 from a + 1 from b
    EXPECT_EQ(c.count(3), 1u);
    EXPECT_EQ(c.size(), 5u);
}

TEST(FreeCommutativeMonoidTest, SingletonConstructor) {
    free_commutative_monoid<int> a(42);
    EXPECT_EQ(a.count(42), 1u);
    EXPECT_EQ(a.size(), 1u);
}

TEST(FreeCommutativeMonoidTest, CommutativityOfOp) {
    // op(a, b) == op(b, a) for the free commutative monoid.
    free_commutative_monoid<int> a{1, 1, 2};
    free_commutative_monoid<int> b{2, 3, 3};

    EXPECT_EQ(op(a, b), op(b, a));
}

// =============================================================================
// Contrast: free monoid is NOT commutative, free commutative monoid IS
// =============================================================================

TEST(ContrastTest, FreeMonoidNotCommutative) {
    free_monoid<int> a{1};
    free_monoid<int> b{2};
    EXPECT_NE(op(a, b), op(b, a));
}

TEST(ContrastTest, FreeCommutativeMonoidIsCommutative) {
    free_commutative_monoid<int> a(1);
    free_commutative_monoid<int> b(2);
    EXPECT_EQ(op(a, b), op(b, a));
}
