#include <gtest/gtest.h>
#include "homomorphism.hpp"

#include <cmath>
#include <functional>

using namespace stepanov;

// =============================================================================
// Concept compliance
// =============================================================================

TEST(HomomorphismTest, MonoidConceptsSatisfied) {
    static_assert(Monoid<additive<int>>);
    static_assert(Monoid<additive<double>>);
    static_assert(Monoid<multiplicative<int>>);
    static_assert(Monoid<multiplicative<double>>);
    static_assert(Monoid<string_monoid>);
    static_assert(Monoid<list_monoid<int>>);
    static_assert(Monoid<list_monoid<double>>);
    static_assert(Monoid<max_monoid<int>>);
    static_assert(Monoid<max_monoid<double>>);
}

// =============================================================================
// Monoid law tests: associativity and identity
// =============================================================================

TEST(HomomorphismTest, AdditiveMonoidLaws) {
    additive<int> a(3), b(5), c(7);

    // Associativity: op(op(a, b), c) == op(a, op(b, c))
    EXPECT_EQ(op(op(a, b), c), op(a, op(b, c)));

    // Identity: op(a, identity) == a, op(identity, a) == a
    EXPECT_EQ(op(a, identity(a)), a);
    EXPECT_EQ(op(identity(a), a), a);
}

TEST(HomomorphismTest, MultiplicativeMonoidLaws) {
    multiplicative<int> a(3), b(5), c(7);

    EXPECT_EQ(op(op(a, b), c), op(a, op(b, c)));
    EXPECT_EQ(op(a, identity(a)), a);
    EXPECT_EQ(op(identity(a), a), a);
}

TEST(HomomorphismTest, StringMonoidLaws) {
    string_monoid a("hello"), b(" "), c("world");

    EXPECT_EQ(op(op(a, b), c), op(a, op(b, c)));
    EXPECT_EQ(op(a, identity(a)), a);
    EXPECT_EQ(op(identity(a), a), a);
}

TEST(HomomorphismTest, ListMonoidLaws) {
    list_monoid<int> a({1, 2}), b({3}), c({4, 5, 6});

    EXPECT_EQ(op(op(a, b), c), op(a, op(b, c)));
    EXPECT_EQ(op(a, identity(a)), a);
    EXPECT_EQ(op(identity(a), a), a);
}

TEST(HomomorphismTest, MaxMonoidLaws) {
    max_monoid<int> a(3), b(7), c(5);

    EXPECT_EQ(op(op(a, b), c), op(a, op(b, c)));
    EXPECT_EQ(op(a, identity(a)), a);
    EXPECT_EQ(op(identity(a), a), a);
}

// =============================================================================
// Homomorphism property tests
// =============================================================================

TEST(HomomorphismTest, LengthIsHomomorphism) {
    string_monoid a("hello"), b("world");

    // f(op(a, b)) == op(f(a), f(b))
    EXPECT_TRUE((is_homomorphism<string_monoid, additive<int>>(length_hom, a, b)));

    // Check explicitly: length("hello" + "world") == length("hello") + length("world")
    EXPECT_EQ(length_hom(op(a, b)), op(length_hom(a), length_hom(b)));
    EXPECT_EQ(length_hom(op(a, b)).value, 10);
}

TEST(HomomorphismTest, SumIsHomomorphism) {
    list_monoid<int> a({1, 2, 3}), b({4, 5});

    EXPECT_TRUE((is_homomorphism<list_monoid<int>, additive<int>>(sum_hom, a, b)));

    // sum([1,2,3] ++ [4,5]) == sum([1,2,3]) + sum([4,5])
    EXPECT_EQ(sum_hom(op(a, b)).value, 15);
    EXPECT_EQ(op(sum_hom(a), sum_hom(b)).value, 15);
}

TEST(HomomorphismTest, ProductIsHomomorphism) {
    list_monoid<int> a({2, 3}), b({4, 5});

    EXPECT_TRUE((is_homomorphism<list_monoid<int>, multiplicative<int>>(product_hom, a, b)));

    // prod([2,3] ++ [4,5]) == prod([2,3]) * prod([4,5])
    EXPECT_EQ(product_hom(op(a, b)).value, 120);
    EXPECT_EQ(op(product_hom(a), product_hom(b)).value, 120);
}

TEST(HomomorphismTest, LogIsHomomorphism) {
    multiplicative<double> a(2.0), b(3.0);

    // log(a * b) should equal log(a) + log(b)
    auto lhs = log_hom(op(a, b));
    auto rhs = op(log_hom(a), log_hom(b));
    EXPECT_NEAR(lhs.value, rhs.value, 1e-12);
    EXPECT_NEAR(lhs.value, std::log(6.0), 1e-12);
}

TEST(HomomorphismTest, CountIsHomomorphism) {
    list_monoid<int> a({10, 20, 30}), b({40, 50});

    EXPECT_TRUE((is_homomorphism<list_monoid<int>, additive<int>>(count_hom<int>, a, b)));

    EXPECT_EQ(count_hom(op(a, b)).value, 5);
    EXPECT_EQ(op(count_hom(a), count_hom(b)).value, 5);
}

// =============================================================================
// fold tests
// =============================================================================

TEST(HomomorphismTest, FoldAsSum) {
    std::vector<int> xs = {1, 2, 3, 4, 5};

    auto result = fold<additive<int>>(
        [](int x) { return additive<int>(x); }, xs);

    EXPECT_EQ(result.value, 15);
}

TEST(HomomorphismTest, FoldAsProduct) {
    std::vector<int> xs = {1, 2, 3, 4, 5};

    auto result = fold<multiplicative<int>>(
        [](int x) { return multiplicative<int>(x); }, xs);

    EXPECT_EQ(result.value, 120);
}

TEST(HomomorphismTest, FoldAsStringConcat) {
    std::vector<std::string> xs = {"hello", " ", "world"};

    auto result = fold<string_monoid>(
        [](const std::string& s) { return string_monoid(s); }, xs);

    EXPECT_EQ(result.value, "hello world");
}

TEST(HomomorphismTest, FoldWithMapping) {
    // Fold with a non-trivial mapping: square each element, then sum
    std::vector<int> xs = {1, 2, 3};

    auto result = fold<additive<int>>(
        [](int x) { return additive<int>(x * x); }, xs);

    EXPECT_EQ(result.value, 14);  // 1 + 4 + 9
}

TEST(HomomorphismTest, FoldOverMonoidElements) {
    // When the elements are already in the target monoid, use the
    // identity-function overload.
    std::vector<additive<int>> xs = {
        additive<int>(10), additive<int>(20), additive<int>(30)
    };

    auto result = fold(xs);
    EXPECT_EQ(result.value, 60);
}

TEST(HomomorphismTest, FoldEmptyList) {
    std::vector<int> empty;

    auto sum = fold<additive<int>>(
        [](int x) { return additive<int>(x); }, empty);
    EXPECT_EQ(sum.value, 0);

    auto prod = fold<multiplicative<int>>(
        [](int x) { return multiplicative<int>(x); }, empty);
    EXPECT_EQ(prod.value, 1);
}

// =============================================================================
// Identity preservation tests
// =============================================================================

TEST(HomomorphismTest, LengthPreservesIdentity) {
    // length("") == 0
    EXPECT_TRUE((preserves_identity<string_monoid, additive<int>>(length_hom)));
}

TEST(HomomorphismTest, SumPreservesIdentity) {
    // sum([]) == 0
    EXPECT_TRUE((preserves_identity<list_monoid<int>, additive<int>>(sum_hom)));
}

TEST(HomomorphismTest, ProductPreservesIdentity) {
    // prod([]) == 1
    EXPECT_TRUE((preserves_identity<list_monoid<int>, multiplicative<int>>(product_hom)));
}

TEST(HomomorphismTest, CountPreservesIdentity) {
    // count([]) == 0
    EXPECT_TRUE((preserves_identity<list_monoid<int>, additive<int>>(count_hom<int>)));
}

// =============================================================================
// Composition of homomorphisms is a homomorphism
// =============================================================================

TEST(HomomorphismTest, CompositionIsHomomorphism) {
    // If f: A -> B and g: B -> C are homomorphisms,
    // then g . f: A -> C is a homomorphism.
    //
    // f = count: list_monoid<int> -> additive<int>
    // g = "double it": additive<int> -> additive<int>   (x -> 2*x)
    //
    // g is a homomorphism because 2*(a+b) = 2*a + 2*b.
    // So g . f should be a homomorphism too.

    auto double_it = [](const additive<int>& a) {
        return additive<int>(2 * a.value);
    };

    auto composed = [&](const list_monoid<int>& xs) {
        return double_it(count_hom(xs));
    };

    list_monoid<int> a({1, 2, 3}), b({4, 5});

    // Verify the composition preserves the monoid operation
    EXPECT_TRUE((is_homomorphism<list_monoid<int>, additive<int>>(composed, a, b)));

    // Verify explicitly
    EXPECT_EQ(composed(op(a, b)).value, 10);     // 2 * count([1,2,3,4,5]) = 2*5
    EXPECT_EQ(op(composed(a), composed(b)).value, 10);  // 2*3 + 2*2
}

// =============================================================================
// Non-homomorphism: square under addition
// =============================================================================

TEST(HomomorphismTest, SquareIsNotHomomorphism) {
    // square: additive<int> -> additive<int>
    // f(x) = x^2
    //
    // This is NOT a homomorphism because (a+b)^2 != a^2 + b^2 in general.
    // For example: (2+3)^2 = 25, but 2^2 + 3^2 = 13.

    auto square = [](const additive<int>& a) {
        return additive<int>(a.value * a.value);
    };

    additive<int> a(2), b(3);

    EXPECT_FALSE((is_homomorphism<additive<int>, additive<int>>(square, a, b)));

    // Verify: f(op(a,b)) = (2+3)^2 = 25, op(f(a),f(b)) = 4+9 = 13
    EXPECT_EQ(square(op(a, b)).value, 25);
    EXPECT_EQ(op(square(a), square(b)).value, 13);
}

TEST(HomomorphismTest, AbsIsNotHomomorphism) {
    // abs: additive<int> -> additive<int>
    // |a + b| != |a| + |b| in general.
    // For example: |3 + (-5)| = 2, but |3| + |-5| = 8.

    auto abs_fn = [](const additive<int>& a) {
        return additive<int>(a.value < 0 ? -a.value : a.value);
    };

    additive<int> a(3), b(-5);

    EXPECT_FALSE((is_homomorphism<additive<int>, additive<int>>(abs_fn, a, b)));
}

// =============================================================================
// Homomorphism with multiple witness pairs
// =============================================================================

TEST(HomomorphismTest, LengthHomomorphismMultiplePairs) {
    // Test the homomorphism property for many pairs of strings.
    std::vector<string_monoid> test_strings = {
        string_monoid(""), string_monoid("a"), string_monoid("hello"),
        string_monoid("world"), string_monoid("foo bar")
    };

    for (const auto& a : test_strings) {
        for (const auto& b : test_strings) {
            EXPECT_TRUE((is_homomorphism<string_monoid, additive<int>>(
                length_hom, a, b)))
                << "Failed for \"" << a.value << "\" and \"" << b.value << "\"";
        }
    }
}

TEST(HomomorphismTest, LogHomomorphismMultiplePairs) {
    // Test for several positive multiplicative values.
    std::vector<multiplicative<double>> test_vals = {
        multiplicative<double>(1.0),
        multiplicative<double>(2.0),
        multiplicative<double>(3.5),
        multiplicative<double>(0.1),
        multiplicative<double>(100.0)
    };

    for (const auto& a : test_vals) {
        for (const auto& b : test_vals) {
            auto lhs = log_hom(op(a, b));
            auto rhs = op(log_hom(a), log_hom(b));
            EXPECT_NEAR(lhs.value, rhs.value, 1e-10)
                << "Failed for " << a.value << " and " << b.value;
        }
    }
}
