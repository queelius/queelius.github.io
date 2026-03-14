/**
 * @file test_functions.cpp
 * @brief Tests for mathematical functions on dual numbers
 */

#include <dual/dual.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

// Functions like exp, sin, cos are found via ADL when called with dual arguments

constexpr double tol = 1e-10;

// =============================================================================
// Exponential and logarithmic
// =============================================================================

TEST(FunctionsTest, Exp) {
    // d/dx exp(x) = exp(x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = exp(x);
    EXPECT_NEAR(y.value(), std::exp(1.0), tol);
    EXPECT_NEAR(y.derivative(), std::exp(1.0), tol);
}

TEST(FunctionsTest, ExpChain) {
    // d/dx exp(2x) = 2*exp(2x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = exp(2.0 * x);
    EXPECT_NEAR(y.value(), std::exp(2.0), tol);
    EXPECT_NEAR(y.derivative(), 2.0 * std::exp(2.0), tol);
}

TEST(FunctionsTest, Log) {
    // d/dx log(x) = 1/x
    auto x = dual::dual<double>::variable(2.0);
    auto y = log(x);
    EXPECT_NEAR(y.value(), std::log(2.0), tol);
    EXPECT_NEAR(y.derivative(), 0.5, tol);
}

TEST(FunctionsTest, LogChain) {
    // d/dx log(x^2) = 2/x
    auto x = dual::dual<double>::variable(3.0);
    auto y = log(x * x);
    EXPECT_NEAR(y.value(), std::log(9.0), tol);
    EXPECT_NEAR(y.derivative(), 2.0 / 3.0, tol);
}

TEST(FunctionsTest, Log2) {
    auto x = dual::dual<double>::variable(8.0);
    auto y = log2(x);
    EXPECT_NEAR(y.value(), 3.0, tol);
    EXPECT_NEAR(y.derivative(), 1.0 / (8.0 * std::log(2.0)), tol);
}

TEST(FunctionsTest, Log10) {
    auto x = dual::dual<double>::variable(100.0);
    auto y = log10(x);
    EXPECT_NEAR(y.value(), 2.0, tol);
    EXPECT_NEAR(y.derivative(), 1.0 / (100.0 * std::log(10.0)), tol);
}

// =============================================================================
// Power functions
// =============================================================================

TEST(FunctionsTest, Sqrt) {
    // d/dx sqrt(x) = 1/(2*sqrt(x))
    auto x = dual::dual<double>::variable(4.0);
    auto y = sqrt(x);
    EXPECT_NEAR(y.value(), 2.0, tol);
    EXPECT_NEAR(y.derivative(), 0.25, tol);
}

TEST(FunctionsTest, SqrtChain) {
    // d/dx sqrt(x^2 + 1) = x/sqrt(x^2+1)
    auto x = dual::dual<double>::variable(3.0);
    auto y = sqrt(x*x + 1.0);
    EXPECT_NEAR(y.value(), std::sqrt(10.0), tol);
    EXPECT_NEAR(y.derivative(), 3.0 / std::sqrt(10.0), tol);
}

TEST(FunctionsTest, PowConstantExponent) {
    // d/dx x^3 = 3x^2
    auto x = dual::dual<double>::variable(2.0);
    auto y = pow(x, 3.0);
    EXPECT_NEAR(y.value(), 8.0, tol);
    EXPECT_NEAR(y.derivative(), 12.0, tol);
}

TEST(FunctionsTest, PowConstantBase) {
    // d/dx 2^x = 2^x * ln(2)
    auto x = dual::dual<double>::variable(3.0);
    auto y = pow(2.0, x);
    EXPECT_NEAR(y.value(), 8.0, tol);
    EXPECT_NEAR(y.derivative(), 8.0 * std::log(2.0), tol);
}

// =============================================================================
// Trigonometric functions
// =============================================================================

TEST(FunctionsTest, Sin) {
    // d/dx sin(x) = cos(x)
    auto x = dual::dual<double>::variable(std::numbers::pi / 6);
    auto y = sin(x);
    EXPECT_NEAR(y.value(), 0.5, tol);
    EXPECT_NEAR(y.derivative(), std::cos(std::numbers::pi / 6), tol);
}

TEST(FunctionsTest, Cos) {
    // d/dx cos(x) = -sin(x)
    auto x = dual::dual<double>::variable(std::numbers::pi / 3);
    auto y = cos(x);
    EXPECT_NEAR(y.value(), 0.5, tol);
    EXPECT_NEAR(y.derivative(), -std::sin(std::numbers::pi / 3), tol);
}

TEST(FunctionsTest, Tan) {
    // d/dx tan(x) = sec^2(x) = 1 + tan^2(x)
    auto x = dual::dual<double>::variable(std::numbers::pi / 4);
    auto y = tan(x);
    double tan_val = std::tan(std::numbers::pi / 4);
    EXPECT_NEAR(y.value(), tan_val, tol);
    EXPECT_NEAR(y.derivative(), 1.0 + tan_val * tan_val, tol);
}

TEST(FunctionsTest, SinChain) {
    // d/dx sin(x^2) = 2x*cos(x^2)
    auto x = dual::dual<double>::variable(2.0);
    auto y = sin(x * x);
    EXPECT_NEAR(y.value(), std::sin(4.0), tol);
    EXPECT_NEAR(y.derivative(), 4.0 * std::cos(4.0), tol);
}

TEST(FunctionsTest, TrigIdentity) {
    // sin^2(x) + cos^2(x) = 1, derivative = 0
    auto x = dual::dual<double>::variable(1.5);
    auto s = sin(x);
    auto c = cos(x);
    auto y = s*s + c*c;
    EXPECT_NEAR(y.value(), 1.0, tol);
    EXPECT_NEAR(y.derivative(), 0.0, tol);
}

// =============================================================================
// Inverse trigonometric functions
// =============================================================================

TEST(FunctionsTest, Asin) {
    // d/dx asin(x) = 1/sqrt(1-x^2)
    auto x = dual::dual<double>::variable(0.5);
    auto y = asin(x);
    EXPECT_NEAR(y.value(), std::asin(0.5), tol);
    EXPECT_NEAR(y.derivative(), 1.0 / std::sqrt(0.75), tol);
}

TEST(FunctionsTest, Acos) {
    // d/dx acos(x) = -1/sqrt(1-x^2)
    auto x = dual::dual<double>::variable(0.5);
    auto y = acos(x);
    EXPECT_NEAR(y.value(), std::acos(0.5), tol);
    EXPECT_NEAR(y.derivative(), -1.0 / std::sqrt(0.75), tol);
}

TEST(FunctionsTest, Atan) {
    // d/dx atan(x) = 1/(1+x^2)
    auto x = dual::dual<double>::variable(1.0);
    auto y = atan(x);
    EXPECT_NEAR(y.value(), std::atan(1.0), tol);
    EXPECT_NEAR(y.derivative(), 0.5, tol);
}

// =============================================================================
// Hyperbolic functions
// =============================================================================

TEST(FunctionsTest, Sinh) {
    // d/dx sinh(x) = cosh(x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = sinh(x);
    EXPECT_NEAR(y.value(), std::sinh(1.0), tol);
    EXPECT_NEAR(y.derivative(), std::cosh(1.0), tol);
}

TEST(FunctionsTest, Cosh) {
    // d/dx cosh(x) = sinh(x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = cosh(x);
    EXPECT_NEAR(y.value(), std::cosh(1.0), tol);
    EXPECT_NEAR(y.derivative(), std::sinh(1.0), tol);
}

TEST(FunctionsTest, Tanh) {
    // d/dx tanh(x) = sech^2(x) = 1 - tanh^2(x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = tanh(x);
    double tanh_val = std::tanh(1.0);
    EXPECT_NEAR(y.value(), tanh_val, tol);
    EXPECT_NEAR(y.derivative(), 1.0 - tanh_val * tanh_val, tol);
}

TEST(FunctionsTest, HyperbolicIdentity) {
    // cosh^2(x) - sinh^2(x) = 1, derivative = 0
    auto x = dual::dual<double>::variable(2.0);
    auto c = cosh(x);
    auto s = sinh(x);
    auto y = c*c - s*s;
    EXPECT_NEAR(y.value(), 1.0, tol);
    EXPECT_NEAR(y.derivative(), 0.0, tol);
}

// =============================================================================
// Inverse hyperbolic functions
// =============================================================================

TEST(FunctionsTest, Asinh) {
    // d/dx asinh(x) = 1/sqrt(x^2+1)
    auto x = dual::dual<double>::variable(2.0);
    auto y = asinh(x);
    EXPECT_NEAR(y.value(), std::asinh(2.0), tol);
    EXPECT_NEAR(y.derivative(), 1.0 / std::sqrt(5.0), tol);
}

TEST(FunctionsTest, Acosh) {
    // d/dx acosh(x) = 1/sqrt(x^2-1)
    auto x = dual::dual<double>::variable(2.0);
    auto y = acosh(x);
    EXPECT_NEAR(y.value(), std::acosh(2.0), tol);
    EXPECT_NEAR(y.derivative(), 1.0 / std::sqrt(3.0), tol);
}

TEST(FunctionsTest, Atanh) {
    // d/dx atanh(x) = 1/(1-x^2)
    auto x = dual::dual<double>::variable(0.5);
    auto y = atanh(x);
    EXPECT_NEAR(y.value(), std::atanh(0.5), tol);
    EXPECT_NEAR(y.derivative(), 1.0 / 0.75, tol);
}

// =============================================================================
// Special functions
// =============================================================================

TEST(FunctionsTest, Abs) {
    auto x_pos = dual::dual<double>::variable(3.0);
    auto y_pos = abs(x_pos);
    EXPECT_NEAR(y_pos.value(), 3.0, tol);
    EXPECT_NEAR(y_pos.derivative(), 1.0, tol);

    auto x_neg = dual::dual<double>::variable(-3.0);
    auto y_neg = abs(x_neg);
    EXPECT_NEAR(y_neg.value(), 3.0, tol);
    EXPECT_NEAR(y_neg.derivative(), -1.0, tol);
}

TEST(FunctionsTest, Hypot) {
    // d/dx hypot(x, y) = x/hypot(x,y) when y is constant
    auto x = dual::dual<double>::variable(3.0);
    auto y = dual::dual<double>::constant(4.0);
    auto h = hypot(x, y);
    EXPECT_NEAR(h.value(), 5.0, tol);
    EXPECT_NEAR(h.derivative(), 0.6, tol);  // 3/5
}

TEST(FunctionsTest, Erf) {
    // d/dx erf(x) = 2/sqrt(pi) * exp(-x^2)
    auto x = dual::dual<double>::variable(0.5);
    auto y = erf(x);
    double two_over_sqrt_pi = 2.0 / std::sqrt(std::numbers::pi);
    EXPECT_NEAR(y.value(), std::erf(0.5), tol);
    EXPECT_NEAR(y.derivative(), two_over_sqrt_pi * std::exp(-0.25), tol);
}

// =============================================================================
// Composite functions
// =============================================================================

TEST(FunctionsTest, CompositeExpSin) {
    // d/dx exp(sin(x)) = exp(sin(x)) * cos(x)
    auto x = dual::dual<double>::variable(1.0);
    auto y = exp(sin(x));
    EXPECT_NEAR(y.value(), std::exp(std::sin(1.0)), tol);
    EXPECT_NEAR(y.derivative(), std::exp(std::sin(1.0)) * std::cos(1.0), tol);
}

TEST(FunctionsTest, CompositeLogCosh) {
    // d/dx log(cosh(x)) = tanh(x)
    auto x = dual::dual<double>::variable(2.0);
    auto y = log(cosh(x));
    EXPECT_NEAR(y.value(), std::log(std::cosh(2.0)), tol);
    EXPECT_NEAR(y.derivative(), std::tanh(2.0), tol);
}

TEST(FunctionsTest, Gaussian) {
    // f(x) = exp(-x^2/2), f'(x) = -x * exp(-x^2/2)
    auto x = dual::dual<double>::variable(1.5);
    auto y = exp(-x*x / 2.0);
    double expected_val = std::exp(-1.125);
    EXPECT_NEAR(y.value(), expected_val, tol);
    EXPECT_NEAR(y.derivative(), -1.5 * expected_val, tol);
}
