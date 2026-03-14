#pragma once

/**
 * @file numerical.hpp
 * @brief Numerical differentiation via finite differences
 *
 * While dual numbers provide exact derivatives (to machine precision),
 * numerical differentiation is useful for:
 * - Functions without analytical form (black-box functions)
 * - Verifying automatic differentiation implementations
 * - Understanding the tradeoffs between approaches
 *
 * This header provides various finite difference schemes with
 * different accuracy/cost tradeoffs.
 */

#include "concepts.hpp"
#include "core.hpp"
#include <cmath>
#include <functional>

namespace dual::numerical {

// Alias to avoid name collision with outer dual namespace
template<typename T>
using dual_t = ::dual::dual<T>;

/**
 * Forward difference: f'(x) ~ (f(x+h) - f(x)) / h
 *
 * Error: O(h) - first order accurate
 * Cost: 1 extra function evaluation
 *
 * @param f The function to differentiate
 * @param x The point at which to evaluate the derivative
 * @param h Step size (default 1e-8)
 * @return Approximate f'(x)
 */
template<typename F, typename T>
T forward_difference(F&& f, T x, T h = T(1e-8)) {
    return (f(x + h) - f(x)) / h;
}

/**
 * Backward difference: f'(x) ~ (f(x) - f(x-h)) / h
 *
 * Error: O(h) - first order accurate
 * Cost: 1 extra function evaluation
 */
template<typename F, typename T>
T backward_difference(F&& f, T x, T h = T(1e-8)) {
    return (f(x) - f(x - h)) / h;
}

/**
 * Central difference: f'(x) ~ (f(x+h) - f(x-h)) / (2h)
 *
 * Error: O(h^2) - second order accurate
 * Cost: 2 function evaluations (but often more accurate for same h)
 */
template<typename F, typename T>
T central_difference(F&& f, T x, T h = T(1e-5)) {
    return (f(x + h) - f(x - h)) / (T(2) * h);
}

/**
 * Five-point stencil: higher accuracy central difference
 *
 * f'(x) ~ (-f(x+2h) + 8f(x+h) - 8f(x-h) + f(x-2h)) / (12h)
 *
 * Error: O(h^4) - fourth order accurate
 * Cost: 4 function evaluations
 */
template<typename F, typename T>
T five_point_stencil(F&& f, T x, T h = T(1e-3)) {
    return (-f(x + T(2)*h) + T(8)*f(x + h) - T(8)*f(x - h) + f(x - T(2)*h)) / (T(12) * h);
}

/**
 * Second derivative via central difference:
 * f''(x) ~ (f(x+h) - 2f(x) + f(x-h)) / h^2
 *
 * Error: O(h^2)
 */
template<typename F, typename T>
T second_derivative_central(F&& f, T x, T h = T(1e-4)) {
    return (f(x + h) - T(2)*f(x) + f(x - h)) / (h * h);
}

/**
 * Five-point second derivative:
 * f''(x) ~ (-f(x+2h) + 16f(x+h) - 30f(x) + 16f(x-h) - f(x-2h)) / (12h^2)
 *
 * Error: O(h^4)
 */
template<typename F, typename T>
T second_derivative_five_point(F&& f, T x, T h = T(1e-2)) {
    T h2 = h * h;
    return (-f(x + T(2)*h) + T(16)*f(x + h) - T(30)*f(x) +
            T(16)*f(x - h) - f(x - T(2)*h)) / (T(12) * h2);
}

/**
 * Richardson extrapolation: improves accuracy by combining estimates at different h.
 *
 * If an estimate A(h) has error O(h^p), then
 * A_improved = (2^p * A(h/2) - A(h)) / (2^p - 1)
 * has error O(h^(p+1)) or O(h^(p+2)) depending on the scheme.
 *
 * @param f The function to differentiate
 * @param x The point at which to evaluate
 * @param h Initial step size
 * @param p Order of the underlying method (2 for central difference)
 * @return Improved derivative estimate
 */
template<typename F, typename T>
T richardson_extrapolation(F&& f, T x, T h = T(1e-3), int p = 2) {
    T a_h = central_difference(f, x, h);
    T a_h2 = central_difference(f, x, h / T(2));
    T factor = std::pow(T(2), p);
    return (factor * a_h2 - a_h) / (factor - T(1));
}

/**
 * Adaptive step size selection based on function behavior.
 *
 * Uses the difference between forward and central differences
 * to estimate error and adjust step size.
 */
template<typename F, typename T>
std::pair<T, T> adaptive_derivative(F&& f, T x, T tol = T(1e-10)) {
    T h = T(1e-4);
    T prev_estimate = central_difference(f, x, h);

    for (int iter = 0; iter < 20; ++iter) {
        h /= T(2);
        T curr_estimate = central_difference(f, x, h);
        T error = std::abs(curr_estimate - prev_estimate);

        if (error < tol) {
            // Use Richardson extrapolation for final answer
            return {(T(4) * curr_estimate - prev_estimate) / T(3), error};
        }

        prev_estimate = curr_estimate;
    }

    // Return best estimate even if tolerance not met
    return {prev_estimate, std::abs(central_difference(f, x, h/T(2)) - prev_estimate)};
}

/**
 * Compute partial derivative of a multivariate function.
 *
 * @param f Function taking vector<T> and returning T
 * @param x Point at which to differentiate
 * @param i Index of variable to differentiate with respect to
 * @param h Step size
 * @return df/dx_i at point x
 */
template<typename F, typename T>
T partial_derivative(F&& f, const std::vector<T>& x, std::size_t i, T h = T(1e-5)) {
    std::vector<T> x_plus = x;
    std::vector<T> x_minus = x;
    x_plus[i] += h;
    x_minus[i] -= h;
    return (f(x_plus) - f(x_minus)) / (T(2) * h);
}

/**
 * Compute gradient of a scalar function of multiple variables.
 */
template<typename F, typename T>
std::vector<T> gradient(F&& f, const std::vector<T>& x, T h = T(1e-5)) {
    std::vector<T> grad(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        grad[i] = partial_derivative(f, x, i, h);
    }
    return grad;
}

/**
 * Compute Jacobian matrix of a vector-valued function.
 *
 * @param f Function taking vector<T> and returning vector<T>
 * @param x Point at which to compute Jacobian
 * @return Jacobian matrix J where J[i][j] = df_i/dx_j
 */
template<typename F, typename T>
std::vector<std::vector<T>> jacobian(F&& f, const std::vector<T>& x, T h = T(1e-5)) {
    auto f0 = f(x);
    std::size_t m = f0.size();
    std::size_t n = x.size();

    std::vector<std::vector<T>> jac(m, std::vector<T>(n));

    for (std::size_t j = 0; j < n; ++j) {
        std::vector<T> x_plus = x;
        std::vector<T> x_minus = x;
        x_plus[j] += h;
        x_minus[j] -= h;

        auto f_plus = f(x_plus);
        auto f_minus = f(x_minus);

        for (std::size_t i = 0; i < m; ++i) {
            jac[i][j] = (f_plus[i] - f_minus[i]) / (T(2) * h);
        }
    }

    return jac;
}

/**
 * Compute Hessian matrix (matrix of second partial derivatives).
 */
template<typename F, typename T>
std::vector<std::vector<T>> hessian(F&& f, const std::vector<T>& x, T h = T(1e-4)) {
    std::size_t n = x.size();
    std::vector<std::vector<T>> hess(n, std::vector<T>(n));

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            // Mixed partial: (f(x+ei+ej) - f(x+ei-ej) - f(x-ei+ej) + f(x-ei-ej)) / (4h^2)
            std::vector<T> x_pp = x, x_pm = x, x_mp = x, x_mm = x;
            x_pp[i] += h; x_pp[j] += h;
            x_pm[i] += h; x_pm[j] -= h;
            x_mp[i] -= h; x_mp[j] += h;
            x_mm[i] -= h; x_mm[j] -= h;

            hess[i][j] = (f(x_pp) - f(x_pm) - f(x_mp) + f(x_mm)) / (T(4) * h * h);
            hess[j][i] = hess[i][j];  // Symmetric
        }
    }

    return hess;
}

/**
 * Compare numerical and automatic differentiation for verification.
 */
template<typename T>
struct derivative_comparison {
    T automatic;    // Result from dual numbers
    T numerical;    // Result from finite differences
    T absolute_error;
    T relative_error;

    bool agrees(T tol = T(1e-6)) const {
        return relative_error < tol || absolute_error < tol;
    }
};

template<typename F, typename T>
derivative_comparison<T> compare_derivatives(F&& f, T x, T h = T(1e-5)) {
    // Compute numerical derivative
    T numerical = five_point_stencil(f, x, h);

    // Compute automatic derivative using dual numbers
    dual_t<T> x_dual = dual_t<T>::variable(x);
    auto auto_result = f(x_dual);
    T automatic = auto_result.derivative();

    T abs_err = std::abs(automatic - numerical);
    T rel_err = abs_err / (std::abs(automatic) + T(1e-15));

    return {automatic, numerical, abs_err, rel_err};
}

} // namespace dual::numerical
