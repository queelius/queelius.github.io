/**
 * @file test_gradient.cpp
 * @brief Tests for gradient, Jacobian, and Hessian computation
 */

#include <dual/dual.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

using dual::gradient;
using dual::jacobian;
using dual::hessian;
using dual::directional_derivative;
using dual::jacobian_vector_product;
using dual::gradient_norm;
using dual::is_stationary_point;
using dual::value_and_gradient;

constexpr double tol = 1e-9;

// =============================================================================
// Gradient tests
// =============================================================================

TEST(GradientTest, SimpleQuadratic) {
    // f(x, y) = x^2 + y^2
    // grad = [2x, 2y]
    auto f = [](const auto& v) {
        return v[0]*v[0] + v[1]*v[1];
    };

    std::vector<double> x = {3.0, 4.0};
    auto grad = gradient(f, x);

    EXPECT_NEAR(grad[0], 6.0, tol);  // 2*3
    EXPECT_NEAR(grad[1], 8.0, tol);  // 2*4
}

TEST(GradientTest, MixedTerms) {
    // f(x, y, z) = x*y + y*z + z*x
    // df/dx = y + z, df/dy = x + z, df/dz = y + x
    auto f = [](const auto& v) {
        return v[0]*v[1] + v[1]*v[2] + v[2]*v[0];
    };

    std::vector<double> x = {1.0, 2.0, 3.0};
    auto grad = gradient(f, x);

    EXPECT_NEAR(grad[0], 5.0, tol);  // y + z = 2 + 3
    EXPECT_NEAR(grad[1], 4.0, tol);  // x + z = 1 + 3
    EXPECT_NEAR(grad[2], 3.0, tol);  // y + x = 2 + 1
}

TEST(GradientTest, Rosenbrock) {
    // f(x, y) = (1-x)^2 + 100(y-x^2)^2
    // df/dx = -2(1-x) - 400x(y-x^2)
    // df/dy = 200(y-x^2)
    auto rosenbrock = [](const auto& v) {
        auto t1 = (1.0 - v[0]) * (1.0 - v[0]);
        auto t2 = 100.0 * (v[1] - v[0]*v[0]) * (v[1] - v[0]*v[0]);
        return t1 + t2;
    };

    std::vector<double> x = {1.0, 1.0};  // At minimum, gradient should be zero
    auto grad = gradient(rosenbrock, x);

    EXPECT_NEAR(grad[0], 0.0, tol);
    EXPECT_NEAR(grad[1], 0.0, tol);
}

TEST(GradientTest, FixedSize) {
    // f(x, y) = sin(x) * cos(y)
    // df/dx = cos(x) * cos(y)
    // df/dy = -sin(x) * sin(y)
    auto f = [](const auto& v) {
        using std::sin; using std::cos;
        using dual::sin; using dual::cos;
        return sin(v[0]) * cos(v[1]);
    };

    std::array<double, 2> x = {1.0, 2.0};
    auto grad = gradient<2>(f, x);

    EXPECT_NEAR(grad[0], std::cos(1.0) * std::cos(2.0), tol);
    EXPECT_NEAR(grad[1], -std::sin(1.0) * std::sin(2.0), tol);
}

// =============================================================================
// Jacobian tests
// =============================================================================

TEST(JacobianTest, LinearMap) {
    // f(x, y) = [2x + 3y, x - y]
    // J = [[2, 3], [1, -1]]
    auto f = [](const auto& v) {
        return std::vector{2.0*v[0] + 3.0*v[1], v[0] - v[1]};
    };

    std::vector<double> x = {1.0, 2.0};
    auto jac = jacobian(f, x);

    EXPECT_NEAR(jac[0][0], 2.0, tol);
    EXPECT_NEAR(jac[0][1], 3.0, tol);
    EXPECT_NEAR(jac[1][0], 1.0, tol);
    EXPECT_NEAR(jac[1][1], -1.0, tol);
}

TEST(JacobianTest, NonlinearMap) {
    // f(x, y) = [x^2 + y, x*y]
    // J = [[2x, 1], [y, x]]
    auto f = [](const auto& v) {
        return std::vector{v[0]*v[0] + v[1], v[0]*v[1]};
    };

    std::vector<double> x = {2.0, 3.0};
    auto jac = jacobian(f, x);

    EXPECT_NEAR(jac[0][0], 4.0, tol);  // 2x = 4
    EXPECT_NEAR(jac[0][1], 1.0, tol);
    EXPECT_NEAR(jac[1][0], 3.0, tol);  // y = 3
    EXPECT_NEAR(jac[1][1], 2.0, tol);  // x = 2
}

TEST(JacobianTest, PolarToCartesian) {
    // f(r, theta) = [r*cos(theta), r*sin(theta)]
    // J = [[cos(theta), -r*sin(theta)], [sin(theta), r*cos(theta)]]
    auto f = [](const auto& v) {
        using std::sin; using std::cos;
        using dual::sin; using dual::cos;
        return std::vector{v[0]*cos(v[1]), v[0]*sin(v[1])};
    };

    double r = 2.0, theta = std::numbers::pi / 4;
    std::vector<double> x = {r, theta};
    auto jac = jacobian(f, x);

    EXPECT_NEAR(jac[0][0], std::cos(theta), tol);
    EXPECT_NEAR(jac[0][1], -r * std::sin(theta), tol);
    EXPECT_NEAR(jac[1][0], std::sin(theta), tol);
    EXPECT_NEAR(jac[1][1], r * std::cos(theta), tol);
}

// =============================================================================
// Hessian tests
// =============================================================================

TEST(HessianTest, Quadratic) {
    // f(x, y) = x^2 + 2xy + 3y^2
    // H = [[2, 2], [2, 6]]
    // Note: Use addition instead of scalar multiplication to work with nested dual types
    auto f = [](const auto& v) {
        auto two_xy = v[0]*v[1] + v[0]*v[1];  // 2xy
        auto three_y2 = v[1]*v[1] + v[1]*v[1] + v[1]*v[1];  // 3y^2
        return v[0]*v[0] + two_xy + three_y2;
    };

    std::vector<double> x = {1.0, 2.0};
    auto hess = hessian(f, x);

    EXPECT_NEAR(hess[0][0], 2.0, tol);
    EXPECT_NEAR(hess[0][1], 2.0, tol);
    EXPECT_NEAR(hess[1][0], 2.0, tol);
    EXPECT_NEAR(hess[1][1], 6.0, tol);
}

TEST(HessianTest, CubicPolynomial) {
    // f(x, y) = x^3 + x*y^2
    // df/dx = 3x^2 + y^2, df/dy = 2xy
    // d2f/dx2 = 6x, d2f/dxdy = 2y, d2f/dy2 = 2x
    auto f = [](const auto& v) {
        return v[0]*v[0]*v[0] + v[0]*v[1]*v[1];
    };

    std::vector<double> x = {2.0, 3.0};
    auto hess = hessian(f, x);

    EXPECT_NEAR(hess[0][0], 12.0, tol);  // 6x = 12
    EXPECT_NEAR(hess[0][1], 6.0, tol);   // 2y = 6
    EXPECT_NEAR(hess[1][0], 6.0, tol);   // symmetric
    EXPECT_NEAR(hess[1][1], 4.0, tol);   // 2x = 4
}

TEST(HessianTest, ExpFunction) {
    // f(x, y) = exp(x + y)
    // All second derivatives equal exp(x + y)
    auto f = [](const auto& v) {
        using std::exp;
        using dual::exp;
        return exp(v[0] + v[1]);
    };

    std::vector<double> x = {1.0, 2.0};
    auto hess = hessian(f, x);
    double e = std::exp(3.0);

    EXPECT_NEAR(hess[0][0], e, tol);
    EXPECT_NEAR(hess[0][1], e, tol);
    EXPECT_NEAR(hess[1][0], e, tol);
    EXPECT_NEAR(hess[1][1], e, tol);
}

// =============================================================================
// Directional derivative tests
// =============================================================================

TEST(DirectionalDerivativeTest, GradientDotDirection) {
    // f(x, y) = x^2 + y^2
    // D_v f = 2x*v_x + 2y*v_y
    auto f = [](const auto& v) {
        return v[0]*v[0] + v[1]*v[1];
    };

    std::vector<double> x = {3.0, 4.0};
    std::vector<double> v = {1.0, 0.0};  // Direction along x-axis

    double dd = directional_derivative(f, x, v);
    EXPECT_NEAR(dd, 6.0, tol);  // 2*3*1 + 2*4*0

    v = {0.0, 1.0};  // Direction along y-axis
    dd = directional_derivative(f, x, v);
    EXPECT_NEAR(dd, 8.0, tol);  // 2*3*0 + 2*4*1

    v = {1.0 / std::sqrt(2.0), 1.0 / std::sqrt(2.0)};  // Diagonal
    dd = directional_derivative(f, x, v);
    EXPECT_NEAR(dd, (6.0 + 8.0) / std::sqrt(2.0), tol);
}

// =============================================================================
// Jacobian-vector product tests
// =============================================================================

TEST(JVPTest, LinearFunction) {
    // f(x, y) = [2x + y, x - y]
    // J * v = [[2, 1], [1, -1]] * [1, 2]^T = [4, -1]
    auto f = [](const auto& v) {
        return std::vector{2.0*v[0] + v[1], v[0] - v[1]};
    };

    std::vector<double> x = {1.0, 2.0};
    std::vector<double> v = {1.0, 2.0};

    auto jvp = jacobian_vector_product(f, x, v);
    EXPECT_NEAR(jvp[0], 4.0, tol);   // 2*1 + 1*2
    EXPECT_NEAR(jvp[1], -1.0, tol);  // 1*1 - 1*2
}

// =============================================================================
// Utility function tests
// =============================================================================

TEST(UtilityTest, GradientNorm) {
    // f(x, y) = x^2 + y^2
    // grad = [2x, 2y] at (3, 4), |grad| = sqrt(36 + 64) = 10
    auto f = [](const auto& v) {
        return v[0]*v[0] + v[1]*v[1];
    };

    std::vector<double> x = {3.0, 4.0};
    double norm = gradient_norm(f, x);
    EXPECT_NEAR(norm, 10.0, tol);
}

TEST(UtilityTest, StationaryPoint) {
    // Rosenbrock has minimum at (1, 1)
    auto rosenbrock = [](const auto& v) {
        auto t1 = (1.0 - v[0]) * (1.0 - v[0]);
        auto t2 = 100.0 * (v[1] - v[0]*v[0]) * (v[1] - v[0]*v[0]);
        return t1 + t2;
    };

    std::vector<double> at_min = {1.0, 1.0};
    std::vector<double> not_min = {0.0, 0.0};

    EXPECT_TRUE(is_stationary_point(rosenbrock, at_min));
    EXPECT_FALSE(is_stationary_point(rosenbrock, not_min));
}

TEST(UtilityTest, ValueAndGradient) {
    auto f = [](const auto& v) {
        return v[0]*v[0] + v[1]*v[1];
    };

    std::vector<double> x = {3.0, 4.0};
    auto [value, grad] = value_and_gradient(f, x);

    EXPECT_NEAR(value, 25.0, tol);
    EXPECT_NEAR(grad[0], 6.0, tol);
    EXPECT_NEAR(grad[1], 8.0, tol);
}
