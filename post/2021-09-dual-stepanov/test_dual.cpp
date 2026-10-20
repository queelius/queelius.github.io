/**
 * @file test_dual.cpp
 * @brief Tests for core dual number functionality
 */

#include <dual/dual.hpp>
#include <gtest/gtest.h>
#include <cmath>

// Use explicit namespace qualification to avoid dual::dual ambiguity
using dual::differentiate;

// =============================================================================
// Construction and access
// =============================================================================

TEST(DualTest, DefaultConstruction) {
    dual::dual<double> d;
    EXPECT_DOUBLE_EQ(d.value(), 0.0);
    EXPECT_DOUBLE_EQ(d.derivative(), 0.0);
}

TEST(DualTest, ValueConstruction) {
    dual::dual<double> d(3.0);
    EXPECT_DOUBLE_EQ(d.value(), 3.0);
    EXPECT_DOUBLE_EQ(d.derivative(), 0.0);
}

TEST(DualTest, FullConstruction) {
    dual::dual<double> d(3.0, 2.0);
    EXPECT_DOUBLE_EQ(d.value(), 3.0);
    EXPECT_DOUBLE_EQ(d.derivative(), 2.0);
}

TEST(DualTest, Variable) {
    auto x = dual::dual<double>::variable(5.0);
    EXPECT_DOUBLE_EQ(x.value(), 5.0);
    EXPECT_DOUBLE_EQ(x.derivative(), 1.0);
}

TEST(DualTest, VariableWithSeed) {
    auto x = dual::dual<double>::variable(5.0, 2.0);
    EXPECT_DOUBLE_EQ(x.value(), 5.0);
    EXPECT_DOUBLE_EQ(x.derivative(), 2.0);
}

TEST(DualTest, Constant) {
    auto c = dual::dual<double>::constant(7.0);
    EXPECT_DOUBLE_EQ(c.value(), 7.0);
    EXPECT_DOUBLE_EQ(c.derivative(), 0.0);
}

// =============================================================================
// Arithmetic operations
// =============================================================================

TEST(DualTest, Negation) {
    auto x = dual::dual<double>::variable(3.0);
    auto y = -x;
    EXPECT_DOUBLE_EQ(y.value(), -3.0);
    EXPECT_DOUBLE_EQ(y.derivative(), -1.0);
}

TEST(DualTest, Addition) {
    auto x = dual::dual<double>::variable(3.0);
    auto y = dual::dual<double>(2.0, 0.5);
    auto z = x + y;
    EXPECT_DOUBLE_EQ(z.value(), 5.0);
    EXPECT_DOUBLE_EQ(z.derivative(), 1.5);
}

TEST(DualTest, Subtraction) {
    auto x = dual::dual<double>::variable(5.0);
    auto y = dual::dual<double>(2.0, 0.5);
    auto z = x - y;
    EXPECT_DOUBLE_EQ(z.value(), 3.0);
    EXPECT_DOUBLE_EQ(z.derivative(), 0.5);
}

TEST(DualTest, Multiplication) {
    // f(x) = x * 3, f'(x) = 3
    auto x = dual::dual<double>::variable(2.0);
    auto c = dual::dual<double>::constant(3.0);
    auto y = x * c;
    EXPECT_DOUBLE_EQ(y.value(), 6.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 3.0);
}

TEST(DualTest, MultiplicationProductRule) {
    // f(x) = x * x = x^2, f'(x) = 2x
    auto x = dual::dual<double>::variable(3.0);
    auto y = x * x;
    EXPECT_DOUBLE_EQ(y.value(), 9.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 6.0);  // 2 * 3
}

TEST(DualTest, Division) {
    // f(x) = x / 2, f'(x) = 1/2
    auto x = dual::dual<double>::variable(6.0);
    auto c = dual::dual<double>::constant(2.0);
    auto y = x / c;
    EXPECT_DOUBLE_EQ(y.value(), 3.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 0.5);
}

TEST(DualTest, DivisionQuotientRule) {
    // f(x) = 1/x, f'(x) = -1/x^2
    auto one = dual::dual<double>::constant(1.0);
    auto x = dual::dual<double>::variable(2.0);
    auto y = one / x;
    EXPECT_DOUBLE_EQ(y.value(), 0.5);
    EXPECT_DOUBLE_EQ(y.derivative(), -0.25);  // -1/4
}

// =============================================================================
// Scalar operations
// =============================================================================

TEST(DualTest, AddScalar) {
    auto x = dual::dual<double>::variable(3.0);
    auto y = x + 2.0;
    EXPECT_DOUBLE_EQ(y.value(), 5.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 1.0);
}

TEST(DualTest, ScalarAdd) {
    auto x = dual::dual<double>::variable(3.0);
    auto y = 2.0 + x;
    EXPECT_DOUBLE_EQ(y.value(), 5.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 1.0);
}

TEST(DualTest, MultiplyScalar) {
    auto x = dual::dual<double>::variable(3.0);
    auto y = x * 2.0;
    EXPECT_DOUBLE_EQ(y.value(), 6.0);
    EXPECT_DOUBLE_EQ(y.derivative(), 2.0);
}

TEST(DualTest, ScalarDivide) {
    // f(x) = 6/x, f'(x) = -6/x^2
    auto x = dual::dual<double>::variable(2.0);
    auto y = 6.0 / x;
    EXPECT_DOUBLE_EQ(y.value(), 3.0);
    EXPECT_DOUBLE_EQ(y.derivative(), -1.5);  // -6/4
}

// =============================================================================
// Compound assignments
// =============================================================================

TEST(DualTest, CompoundAdd) {
    auto x = dual::dual<double>::variable(3.0);
    x += dual::dual<double>(2.0, 0.5);
    EXPECT_DOUBLE_EQ(x.value(), 5.0);
    EXPECT_DOUBLE_EQ(x.derivative(), 1.5);
}

TEST(DualTest, CompoundMultiply) {
    auto x = dual::dual<double>::variable(3.0);
    x *= dual::dual<double>::constant(2.0);
    EXPECT_DOUBLE_EQ(x.value(), 6.0);
    EXPECT_DOUBLE_EQ(x.derivative(), 2.0);
}

// =============================================================================
// Comparison operators
// =============================================================================

TEST(DualTest, Equality) {
    auto x = dual::dual<double>(3.0, 1.0);
    auto y = dual::dual<double>(3.0, 2.0);  // Different derivative
    EXPECT_TRUE(x == y);  // Comparison based on value only
}

TEST(DualTest, LessThan) {
    auto x = dual::dual<double>::variable(2.0);
    auto y = dual::dual<double>::variable(3.0);
    EXPECT_TRUE(x < y);
    EXPECT_FALSE(y < x);
}

// =============================================================================
// Polynomial derivatives
// =============================================================================

TEST(DualTest, Polynomial) {
    // f(x) = x^3 - 2x^2 + 3x - 1
    // f'(x) = 3x^2 - 4x + 3
    // At x = 2: f(2) = 8 - 8 + 6 - 1 = 5
    //           f'(2) = 12 - 8 + 3 = 7
    auto x = dual::dual<double>::variable(2.0);
    auto f = x*x*x - 2.0*x*x + 3.0*x - 1.0;

    EXPECT_DOUBLE_EQ(f.value(), 5.0);
    EXPECT_DOUBLE_EQ(f.derivative(), 7.0);
}

TEST(DualTest, RationalFunction) {
    // f(x) = (x^2 + 1) / (x - 1)
    // f'(x) = (2x(x-1) - (x^2+1)) / (x-1)^2 = (x^2 - 2x - 1) / (x-1)^2
    // At x = 3: f(3) = 10/2 = 5
    //           f'(3) = (9 - 6 - 1)/4 = 2/4 = 0.5
    auto x = dual::dual<double>::variable(3.0);
    auto f = (x*x + 1.0) / (x - 1.0);

    EXPECT_DOUBLE_EQ(f.value(), 5.0);
    EXPECT_DOUBLE_EQ(f.derivative(), 0.5);
}

// =============================================================================
// Convenience function
// =============================================================================

TEST(DualTest, DifferentiateFunction) {
    auto [value, deriv] = differentiate([](auto x) { return x * x * x; }, 2.0);
    EXPECT_DOUBLE_EQ(value, 8.0);   // 2^3
    EXPECT_DOUBLE_EQ(deriv, 12.0);  // 3 * 2^2
}

// =============================================================================
// Constexpr evaluation
// =============================================================================

TEST(DualTest, ConstexprComputation) {
    constexpr auto x = dual::dual<double>::variable(2.0);
    constexpr auto y = x * x + x;
    static_assert(y.value() == 6.0);
    static_assert(y.derivative() == 5.0);
    EXPECT_DOUBLE_EQ(y.value(), 6.0);
}

// =============================================================================
// Float type
// =============================================================================

TEST(DualTest, FloatType) {
    auto x = dual::dual<float>::variable(2.0f);
    auto y = x * x;
    EXPECT_FLOAT_EQ(y.value(), 4.0f);
    EXPECT_FLOAT_EQ(y.derivative(), 4.0f);
}
