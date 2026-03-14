/**
 * @file test_higher_order.cpp
 * @brief Tests for higher-order derivatives via dual2 and jets
 */

#include <dual/dual.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

using dual::differentiate2;
using dual::jet;
using dual::derivatives;

constexpr double tol = 1e-9;

// =============================================================================
// Second-order dual numbers (dual2)
// =============================================================================

TEST(HigherOrderTest, Dual2Quadratic) {
    // f(x) = x^2, f'(x) = 2x, f''(x) = 2
    auto result = differentiate2([](auto x) { return x * x; }, 3.0);
    EXPECT_NEAR(result.value, 9.0, tol);
    EXPECT_NEAR(result.first, 6.0, tol);
    EXPECT_NEAR(result.second, 2.0, tol);
}

TEST(HigherOrderTest, Dual2Cubic) {
    // f(x) = x^3, f'(x) = 3x^2, f''(x) = 6x
    auto result = differentiate2([](auto x) { return x * x * x; }, 2.0);
    EXPECT_NEAR(result.value, 8.0, tol);
    EXPECT_NEAR(result.first, 12.0, tol);
    EXPECT_NEAR(result.second, 12.0, tol);
}

TEST(HigherOrderTest, Dual2Sin) {
    // f(x) = sin(x), f'(x) = cos(x), f''(x) = -sin(x)
    double x = 1.0;
    auto result = differentiate2([](auto t) { return sin(t); }, x);
    EXPECT_NEAR(result.value, std::sin(x), tol);
    EXPECT_NEAR(result.first, std::cos(x), tol);
    EXPECT_NEAR(result.second, -std::sin(x), tol);
}

TEST(HigherOrderTest, Dual2Exp) {
    // f(x) = exp(x), all derivatives equal exp(x)
    double x = 1.0;
    auto result = differentiate2([](auto t) { return exp(t); }, x);
    double e = std::exp(x);
    EXPECT_NEAR(result.value, e, tol);
    EXPECT_NEAR(result.first, e, tol);
    EXPECT_NEAR(result.second, e, tol);
}

TEST(HigherOrderTest, Dual2Composite) {
    // f(x) = exp(-x^2), f'(x) = -2x*exp(-x^2), f''(x) = (4x^2 - 2)*exp(-x^2)
    double x = 1.0;
    auto result = differentiate2([](auto t) { return exp(-t * t); }, x);
    double e = std::exp(-1.0);
    EXPECT_NEAR(result.value, e, tol);
    EXPECT_NEAR(result.first, -2.0 * e, tol);
    EXPECT_NEAR(result.second, 2.0 * e, tol);  // (4 - 2)*exp(-1)
}

// =============================================================================
// Taylor jets
// =============================================================================

TEST(JetTest, Construction) {
    jet<double, 3> j;
    EXPECT_DOUBLE_EQ(j[0], 0.0);
    EXPECT_DOUBLE_EQ(j[1], 0.0);
    EXPECT_DOUBLE_EQ(j[2], 0.0);
    EXPECT_DOUBLE_EQ(j[3], 0.0);
}

TEST(JetTest, Variable) {
    auto j = jet<double, 3>::variable(2.0);
    EXPECT_DOUBLE_EQ(j[0], 2.0);
    EXPECT_DOUBLE_EQ(j[1], 1.0);
    EXPECT_DOUBLE_EQ(j[2], 0.0);
}

TEST(JetTest, Constant) {
    auto j = jet<double, 3>::constant(5.0);
    EXPECT_DOUBLE_EQ(j[0], 5.0);
    EXPECT_DOUBLE_EQ(j[1], 0.0);
}

TEST(JetTest, Addition) {
    jet<double, 2> a{1.0, 2.0, 3.0};
    jet<double, 2> b{4.0, 5.0, 6.0};
    auto c = a + b;
    EXPECT_DOUBLE_EQ(c[0], 5.0);
    EXPECT_DOUBLE_EQ(c[1], 7.0);
    EXPECT_DOUBLE_EQ(c[2], 9.0);
}

TEST(JetTest, Multiplication) {
    // (1 + 2h + 3h^2) * (1 + h) = 1 + 3h + 5h^2 + 3h^3
    // But jets store f^(k)/k!, so we work with normalized coefficients
    auto x = jet<double, 2>::variable(1.0);  // 1 + h
    auto y = x * x;  // (1+h)^2 = 1 + 2h + h^2
    EXPECT_DOUBLE_EQ(y[0], 1.0);  // value
    EXPECT_DOUBLE_EQ(y[1], 2.0);  // first deriv / 1!
    EXPECT_DOUBLE_EQ(y[2], 1.0);  // second deriv / 2!
}

TEST(JetTest, Division) {
    // 1/(1+h) = 1 - h + h^2 - h^3 + ...
    auto one = jet<double, 3>::constant(1.0);
    auto x = jet<double, 3>::variable(1.0);
    auto y = one / x;

    // At x=1: 1/x = 1, d/dx(1/x) = -1, d^2/dx^2(1/x) = 2
    // jet stores f^(k)/k!, so: [1, -1, 2/2, -6/6] = [1, -1, 1, -1]
    EXPECT_DOUBLE_EQ(y[0], 1.0);
    EXPECT_DOUBLE_EQ(y[1], -1.0);
    EXPECT_DOUBLE_EQ(y[2], 1.0);
    EXPECT_DOUBLE_EQ(y[3], -1.0);
}

TEST(JetTest, ExpDerivatives) {
    // All derivatives of exp(x) at x=a equal exp(a)
    auto derivs = derivatives<4>([](auto x) { return exp(x); }, 1.0);
    double e = std::exp(1.0);
    for (size_t k = 0; k <= 4; ++k) {
        EXPECT_NEAR(derivs[k], e, tol) << "k = " << k;
    }
}

TEST(JetTest, SinDerivatives) {
    // sin(x) derivatives cycle: sin, cos, -sin, -cos, sin, ...
    double x = 1.0;
    auto derivs = derivatives<4>([](auto t) { return sin(t); }, x);

    EXPECT_NEAR(derivs[0], std::sin(x), tol);
    EXPECT_NEAR(derivs[1], std::cos(x), tol);
    EXPECT_NEAR(derivs[2], -std::sin(x), tol);
    EXPECT_NEAR(derivs[3], -std::cos(x), tol);
    EXPECT_NEAR(derivs[4], std::sin(x), tol);
}

TEST(JetTest, CosDerivatives) {
    // cos(x) derivatives cycle: cos, -sin, -cos, sin, cos, ...
    double x = 1.0;
    auto derivs = derivatives<4>([](auto t) { return cos(t); }, x);

    EXPECT_NEAR(derivs[0], std::cos(x), tol);
    EXPECT_NEAR(derivs[1], -std::sin(x), tol);
    EXPECT_NEAR(derivs[2], -std::cos(x), tol);
    EXPECT_NEAR(derivs[3], std::sin(x), tol);
    EXPECT_NEAR(derivs[4], std::cos(x), tol);
}

TEST(JetTest, LogDerivatives) {
    // d^n/dx^n log(x) = (-1)^(n+1) * (n-1)! / x^n for n >= 1
    double x = 2.0;
    auto derivs = derivatives<4>([](auto t) { return log(t); }, x);

    EXPECT_NEAR(derivs[0], std::log(x), tol);
    EXPECT_NEAR(derivs[1], 1.0/x, tol);                    // 1/2
    EXPECT_NEAR(derivs[2], -1.0/(x*x), tol);               // -1/4
    EXPECT_NEAR(derivs[3], 2.0/(x*x*x), tol);              // 2/8 = 1/4
    EXPECT_NEAR(derivs[4], -6.0/(x*x*x*x), tol);           // -6/16 = -3/8
}

TEST(JetTest, SqrtDerivatives) {
    double x = 4.0;
    auto derivs = derivatives<3>([](auto t) { return sqrt(t); }, x);

    // sqrt(4) = 2
    EXPECT_NEAR(derivs[0], 2.0, tol);
    // d/dx sqrt(x) = 1/(2*sqrt(x)) = 1/4 at x=4
    EXPECT_NEAR(derivs[1], 0.25, tol);
    // d^2/dx^2 sqrt(x) = -1/(4*x^(3/2)) = -1/32 at x=4
    EXPECT_NEAR(derivs[2], -1.0/32.0, tol);
}

TEST(JetTest, PolynomialExact) {
    // f(x) = x^3 - 2x^2 + 3x - 1
    // f'(x) = 3x^2 - 4x + 3
    // f''(x) = 6x - 4
    // f'''(x) = 6
    // f''''(x) = 0
    auto f = [](auto x) {
        return x*x*x - 2.0*x*x + 3.0*x - 1.0;
    };

    double x = 2.0;
    auto derivs = derivatives<4>(f, x);

    EXPECT_NEAR(derivs[0], 5.0, tol);   // 8 - 8 + 6 - 1
    EXPECT_NEAR(derivs[1], 7.0, tol);   // 12 - 8 + 3
    EXPECT_NEAR(derivs[2], 8.0, tol);   // 12 - 4
    EXPECT_NEAR(derivs[3], 6.0, tol);   // constant
    EXPECT_NEAR(derivs[4], 0.0, tol);   // zero for polynomial degree 3
}

TEST(JetTest, DerivativeExtraction) {
    // Test that derivative() correctly scales by factorial
    auto j = jet<double, 3>::variable(1.0);
    auto y = exp(j);

    double e = std::exp(1.0);

    // derivative(k) should return the actual k-th derivative, not f^(k)/k!
    EXPECT_NEAR(y.derivative(0), e, tol);
    EXPECT_NEAR(y.derivative(1), e, tol);
    EXPECT_NEAR(y.derivative(2), e, tol);
    EXPECT_NEAR(y.derivative(3), e, tol);
}
