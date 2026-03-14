/**
 * @file test_numerical.cpp
 * @brief Tests for numerical differentiation schemes
 */

#include <dual/dual.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

using namespace dual::numerical;

constexpr double loose_tol = 1e-4;
constexpr double tight_tol = 1e-8;

// =============================================================================
// Basic finite differences
// =============================================================================

TEST(NumericalTest, ForwardDifference) {
    // f(x) = x^2, f'(x) = 2x
    auto f = [](double x) { return x * x; };
    double deriv = forward_difference(f, 3.0);
    EXPECT_NEAR(deriv, 6.0, loose_tol);
}

TEST(NumericalTest, BackwardDifference) {
    auto f = [](double x) { return x * x; };
    double deriv = backward_difference(f, 3.0);
    EXPECT_NEAR(deriv, 6.0, loose_tol);
}

TEST(NumericalTest, CentralDifference) {
    // Central difference is more accurate
    auto f = [](double x) { return x * x; };
    double deriv = central_difference(f, 3.0);
    EXPECT_NEAR(deriv, 6.0, tight_tol);
}

TEST(NumericalTest, FivePointStencil) {
    // Five-point stencil is even more accurate
    auto f = [](double x) { return std::sin(x); };
    double deriv = five_point_stencil(f, 1.0);
    EXPECT_NEAR(deriv, std::cos(1.0), tight_tol);
}

// =============================================================================
// Second derivatives
// =============================================================================

TEST(NumericalTest, SecondDerivativeCentral) {
    // f(x) = x^3, f''(x) = 6x
    auto f = [](double x) { return x * x * x; };
    double d2 = second_derivative_central(f, 2.0);
    EXPECT_NEAR(d2, 12.0, loose_tol);
}

TEST(NumericalTest, SecondDerivativeFivePoint) {
    // f(x) = sin(x), f''(x) = -sin(x)
    auto f = [](double x) { return std::sin(x); };
    double d2 = second_derivative_five_point(f, 1.0);
    EXPECT_NEAR(d2, -std::sin(1.0), tight_tol);
}

// =============================================================================
// Richardson extrapolation
// =============================================================================

TEST(NumericalTest, RichardsonExtrapolation) {
    auto f = [](double x) { return std::exp(x); };
    double deriv = richardson_extrapolation(f, 1.0);
    EXPECT_NEAR(deriv, std::exp(1.0), tight_tol);
}

// =============================================================================
// Adaptive differentiation
// =============================================================================

TEST(NumericalTest, AdaptiveDerivative) {
    auto f = [](double x) { return std::log(x); };
    auto [deriv, error] = adaptive_derivative(f, 2.0);
    EXPECT_NEAR(deriv, 0.5, tight_tol);
    EXPECT_LT(error, 1e-8);
}

// =============================================================================
// Multivariate functions
// =============================================================================

TEST(NumericalTest, PartialDerivative) {
    // f(x, y) = x^2 + xy + y^2
    // df/dx = 2x + y
    auto f = [](const std::vector<double>& v) {
        return v[0]*v[0] + v[0]*v[1] + v[1]*v[1];
    };

    std::vector<double> point = {1.0, 2.0};
    double dfdx = partial_derivative(f, point, 0);
    EXPECT_NEAR(dfdx, 4.0, loose_tol);  // 2*1 + 2

    double dfdy = partial_derivative(f, point, 1);
    EXPECT_NEAR(dfdy, 5.0, loose_tol);  // 1 + 2*2
}

TEST(NumericalTest, Gradient) {
    // f(x, y, z) = x^2 + 2y^2 + 3z^2
    auto f = [](const std::vector<double>& v) {
        return v[0]*v[0] + 2*v[1]*v[1] + 3*v[2]*v[2];
    };

    std::vector<double> point = {1.0, 2.0, 3.0};
    auto grad = gradient(f, point);

    EXPECT_NEAR(grad[0], 2.0, loose_tol);   // 2x
    EXPECT_NEAR(grad[1], 8.0, loose_tol);   // 4y
    EXPECT_NEAR(grad[2], 18.0, loose_tol);  // 6z
}

TEST(NumericalTest, Jacobian) {
    // f(x, y) = (x^2 + y, xy)
    // J = [[2x, 1], [y, x]]
    auto f = [](const std::vector<double>& v) {
        return std::vector<double>{v[0]*v[0] + v[1], v[0]*v[1]};
    };

    std::vector<double> point = {2.0, 3.0};
    auto jac = jacobian(f, point);

    EXPECT_NEAR(jac[0][0], 4.0, loose_tol);  // 2x = 4
    EXPECT_NEAR(jac[0][1], 1.0, loose_tol);  // 1
    EXPECT_NEAR(jac[1][0], 3.0, loose_tol);  // y = 3
    EXPECT_NEAR(jac[1][1], 2.0, loose_tol);  // x = 2
}

TEST(NumericalTest, Hessian) {
    // f(x, y) = x^2*y + y^3
    // df/dx = 2xy, df/dy = x^2 + 3y^2
    // d2f/dx2 = 2y, d2f/dxdy = 2x, d2f/dy2 = 6y
    auto f = [](const std::vector<double>& v) {
        return v[0]*v[0]*v[1] + v[1]*v[1]*v[1];
    };

    std::vector<double> point = {2.0, 3.0};
    auto hess = hessian(f, point);

    EXPECT_NEAR(hess[0][0], 6.0, loose_tol);   // 2y = 6
    EXPECT_NEAR(hess[0][1], 4.0, loose_tol);   // 2x = 4
    EXPECT_NEAR(hess[1][0], 4.0, loose_tol);   // symmetric
    EXPECT_NEAR(hess[1][1], 18.0, loose_tol);  // 6y = 18
}

// =============================================================================
// Comparison with automatic differentiation
// =============================================================================

TEST(NumericalTest, CompareWithAutomatic) {
    auto f = [](auto x) { return x * x * x - 2.0 * x + 1.0; };

    auto cmp = compare_derivatives(f, 2.0);

    // Both should give f'(2) = 3*4 - 2 = 10
    EXPECT_NEAR(cmp.automatic, 10.0, 1e-12);
    EXPECT_NEAR(cmp.numerical, 10.0, tight_tol);
    EXPECT_TRUE(cmp.agrees());
}

TEST(NumericalTest, CompareWithAutomaticTrig) {
    auto f = [](auto x) {
        using std::sin;
        using dual::sin;
        return sin(x * x);
    };

    auto cmp = compare_derivatives(f, 1.5);

    // f'(x) = 2x*cos(x^2)
    double expected = 3.0 * std::cos(2.25);
    EXPECT_NEAR(cmp.automatic, expected, 1e-12);
    EXPECT_NEAR(cmp.numerical, expected, tight_tol);
    EXPECT_TRUE(cmp.agrees());
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(NumericalTest, NearZero) {
    // f(x) = x^2 at x near 0
    auto f = [](double x) { return x * x; };
    double deriv = central_difference(f, 0.001);
    EXPECT_NEAR(deriv, 0.002, loose_tol);
}

TEST(NumericalTest, LargeValue) {
    // f(x) = x^2 at large x
    auto f = [](double x) { return x * x; };
    double deriv = central_difference(f, 1000.0, 1e-3);  // Larger h for large x
    EXPECT_NEAR(deriv, 2000.0, 1.0);  // Looser tolerance for large values
}

TEST(NumericalTest, OscillatoryFunction) {
    // Rapidly oscillating function requires smaller h
    auto f = [](double x) { return std::sin(100.0 * x); };
    double deriv = central_difference(f, 0.0, 1e-6);
    EXPECT_NEAR(deriv, 100.0, 1e-3);  // f'(0) = 100*cos(0) = 100
}
