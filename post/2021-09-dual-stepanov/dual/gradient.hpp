#pragma once

/**
 * @file gradient.hpp
 * @brief Gradient, Jacobian, and Hessian computation using dual numbers
 *
 * THIS MODULE TEACHES: Computing multivariable derivatives with forward-mode AD.
 *
 * Forward mode computes one directional derivative per pass:
 * - Gradient of f: R^n -> R requires n passes
 * - Jacobian of f: R^n -> R^m requires n passes (m outputs per pass)
 * - Hessian of f: R^n -> R requires n^2 passes (using dual2)
 *
 * For f: R^n -> R where n is large, reverse mode would be more efficient.
 * But forward mode is simpler and sufficient for many applications.
 */

#include "core.hpp"
#include "functions.hpp"
#include "higher_order.hpp"
#include <vector>
#include <array>
#include <functional>
#include <cmath>

namespace dual {

// =============================================================================
// Gradient computation
// =============================================================================

/**
 * Compute gradient of a scalar function using forward-mode AD.
 *
 * For f: R^n -> R, we need n forward passes, each computing one partial derivative.
 *
 * @param f Function taking vector<dual<T>> and returning dual<T>
 * @param x Point at which to compute gradient
 * @return Gradient vector [df/dx_0, df/dx_1, ..., df/dx_{n-1}]
 */
template<typename F, typename T>
std::vector<T> gradient(F&& f, const std::vector<T>& x) {
    std::vector<T> grad(x.size());

    for (std::size_t i = 0; i < x.size(); ++i) {
        // Create dual vector with derivative seed in direction i
        std::vector<dual<T>> x_dual(x.size());
        for (std::size_t j = 0; j < x.size(); ++j) {
            x_dual[j] = (i == j) ? dual<T>::variable(x[j])
                                 : dual<T>::constant(x[j]);
        }

        // Evaluate function and extract partial derivative
        auto result = f(x_dual);
        grad[i] = result.derivative();
    }

    return grad;
}

/**
 * Fixed-size gradient for compile-time known dimensions.
 */
template<std::size_t N, typename F, typename T>
std::array<T, N> gradient(F&& f, const std::array<T, N>& x) {
    std::array<T, N> grad;

    for (std::size_t i = 0; i < N; ++i) {
        std::array<dual<T>, N> x_dual;
        for (std::size_t j = 0; j < N; ++j) {
            x_dual[j] = (i == j) ? dual<T>::variable(x[j])
                                 : dual<T>::constant(x[j]);
        }

        auto result = f(x_dual);
        grad[i] = result.derivative();
    }

    return grad;
}

// =============================================================================
// Jacobian computation
// =============================================================================

/**
 * Compute Jacobian matrix of a vector function.
 *
 * For f: R^n -> R^m, the Jacobian J is an m x n matrix where J[i][j] = df_i/dx_j.
 *
 * Forward mode: n passes, each computing one column (all m derivatives wrt one variable).
 *
 * @param f Function taking vector<dual<T>> and returning vector<dual<T>>
 * @param x Point at which to compute Jacobian
 * @return Jacobian matrix J[i][j] = df_i/dx_j
 */
template<typename F, typename T>
std::vector<std::vector<T>> jacobian(F&& f, const std::vector<T>& x) {
    // First call to determine output dimension
    std::vector<dual<T>> x_test(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        x_test[i] = dual<T>::constant(x[i]);
    }
    auto y_test = f(x_test);
    std::size_t m = y_test.size();
    std::size_t n = x.size();

    std::vector<std::vector<T>> jac(m, std::vector<T>(n));

    for (std::size_t j = 0; j < n; ++j) {
        // Create dual vector with derivative seed in direction j
        std::vector<dual<T>> x_dual(n);
        for (std::size_t k = 0; k < n; ++k) {
            x_dual[k] = (j == k) ? dual<T>::variable(x[k])
                                 : dual<T>::constant(x[k]);
        }

        // Evaluate function
        auto y_dual = f(x_dual);

        // Extract j-th column of Jacobian
        for (std::size_t i = 0; i < m; ++i) {
            jac[i][j] = y_dual[i].derivative();
        }
    }

    return jac;
}

/**
 * Fixed-size Jacobian computation.
 */
template<std::size_t M, std::size_t N, typename F, typename T>
std::array<std::array<T, N>, M> jacobian(F&& f, const std::array<T, N>& x) {
    std::array<std::array<T, N>, M> jac;

    for (std::size_t j = 0; j < N; ++j) {
        std::array<dual<T>, N> x_dual;
        for (std::size_t k = 0; k < N; ++k) {
            x_dual[k] = (j == k) ? dual<T>::variable(x[k])
                                 : dual<T>::constant(x[k]);
        }

        auto y_dual = f(x_dual);

        for (std::size_t i = 0; i < M; ++i) {
            jac[i][j] = y_dual[i].derivative();
        }
    }

    return jac;
}

// =============================================================================
// Hessian computation
// =============================================================================

/**
 * Compute Hessian matrix (matrix of second partial derivatives).
 *
 * For f: R^n -> R, the Hessian H is an n x n matrix where H[i][j] = d^2f/dx_i dx_j.
 *
 * Uses dual2 (nested dual numbers) for second derivatives.
 *
 * @param f Function that works with dual2<T>
 * @param x Point at which to compute Hessian
 * @return Symmetric Hessian matrix
 */
template<typename F, typename T>
std::vector<std::vector<T>> hessian(F&& f, const std::vector<T>& x) {
    std::size_t n = x.size();
    std::vector<std::vector<T>> hess(n, std::vector<T>(n));

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            // Create dual2 vector with appropriate seeds
            // To compute d²f/dx_i dx_j:
            //   1. Differentiate wrt x_j (inner dual: x_j is variable)
            //   2. Differentiate that result wrt x_i (outer dual seeds x_i)
            std::vector<dual2<T>> x_dual2(n);

            for (std::size_t k = 0; k < n; ++k) {
                if (k == i && k == j) {
                    // Diagonal: d²f/dx_i² - both differentiations wrt same var
                    x_dual2[k] = dual2<T>(
                        dual<T>::variable(x[k]),
                        dual<T>::constant(T(1))
                    );
                } else if (k == j) {
                    // First differentiation wrt x_j: inner is variable
                    x_dual2[k] = dual2<T>(
                        dual<T>::variable(x[k]),
                        dual<T>::constant(T(0))
                    );
                } else if (k == i) {
                    // Second differentiation wrt x_i: seed outer derivative
                    x_dual2[k] = dual2<T>(
                        dual<T>::constant(x[k]),
                        dual<T>::constant(T(1))
                    );
                } else {
                    // Other variables: constant
                    x_dual2[k] = dual2<T>(
                        dual<T>::constant(x[k]),
                        dual<T>::constant(T(0))
                    );
                }
            }

            auto result = f(x_dual2);
            second_order_result<T> derivs(result);

            hess[i][j] = derivs.second;
            if (i != j) {
                hess[j][i] = derivs.second;  // Symmetric
            }
        }
    }

    return hess;
}

/**
 * Fixed-size Hessian computation.
 */
template<std::size_t N, typename F, typename T>
std::array<std::array<T, N>, N> hessian(F&& f, const std::array<T, N>& x) {
    std::array<std::array<T, N>, N> hess;

    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            // To compute d²f/dx_i dx_j:
            //   1. Differentiate wrt x_j (inner dual: x_j is variable)
            //   2. Differentiate that result wrt x_i (outer dual seeds x_i)
            std::array<dual2<T>, N> x_dual2;

            for (std::size_t k = 0; k < N; ++k) {
                if (k == i && k == j) {
                    // Diagonal: d²f/dx_i² - both differentiations wrt same var
                    x_dual2[k] = dual2<T>(
                        dual<T>::variable(x[k]),
                        dual<T>::constant(T(1))
                    );
                } else if (k == j) {
                    // First differentiation wrt x_j: inner is variable
                    x_dual2[k] = dual2<T>(
                        dual<T>::variable(x[k]),
                        dual<T>::constant(T(0))
                    );
                } else if (k == i) {
                    // Second differentiation wrt x_i: seed outer derivative
                    x_dual2[k] = dual2<T>(
                        dual<T>::constant(x[k]),
                        dual<T>::constant(T(1))
                    );
                } else {
                    x_dual2[k] = dual2<T>(
                        dual<T>::constant(x[k]),
                        dual<T>::constant(T(0))
                    );
                }
            }

            auto result = f(x_dual2);
            second_order_result<T> derivs(result);

            hess[i][j] = derivs.second;
            if (i != j) {
                hess[j][i] = derivs.second;
            }
        }
    }

    return hess;
}

// =============================================================================
// Directional derivatives
// =============================================================================

/**
 * Compute directional derivative of f at x in direction v.
 *
 * D_v f(x) = gradient(f, x) . v = sum_i (df/dx_i) * v_i
 *
 * Forward mode computes this in a single pass (seed derivative = v).
 */
template<typename F, typename T>
T directional_derivative(F&& f, const std::vector<T>& x, const std::vector<T>& v) {
    std::vector<dual<T>> x_dual(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        x_dual[i] = dual<T>(x[i], v[i]);  // Seed with direction v
    }

    auto result = f(x_dual);
    return result.derivative();
}

/**
 * Compute Jacobian-vector product: J(f, x) * v
 *
 * For f: R^n -> R^m and v in R^n, computes J*v in R^m.
 * Forward mode does this in a single pass.
 */
template<typename F, typename T>
std::vector<T> jacobian_vector_product(F&& f, const std::vector<T>& x, const std::vector<T>& v) {
    std::vector<dual<T>> x_dual(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        x_dual[i] = dual<T>(x[i], v[i]);
    }

    auto y_dual = f(x_dual);

    std::vector<T> result(y_dual.size());
    for (std::size_t i = 0; i < y_dual.size(); ++i) {
        result[i] = y_dual[i].derivative();
    }

    return result;
}

// =============================================================================
// Gradient norm and related utilities
// =============================================================================

/**
 * Compute L2 norm of gradient (useful for convergence tests).
 */
template<typename F, typename T>
T gradient_norm(F&& f, const std::vector<T>& x) {
    auto grad = gradient(std::forward<F>(f), x);
    T sum = T(0);
    for (const auto& g : grad) {
        sum += g * g;
    }
    return std::sqrt(sum);
}

/**
 * Check if gradient is approximately zero (stationary point).
 */
template<typename F, typename T>
bool is_stationary_point(F&& f, const std::vector<T>& x, T tolerance = T(1e-8)) {
    return gradient_norm(std::forward<F>(f), x) < tolerance;
}

/**
 * Evaluate function and compute gradient simultaneously.
 *
 * Returns pair (f(x), gradient(f, x)).
 * Slightly more efficient than computing them separately.
 */
template<typename F, typename T>
std::pair<T, std::vector<T>> value_and_gradient(F&& f, const std::vector<T>& x) {
    // First pass: get function value
    std::vector<dual<T>> x_const(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        x_const[i] = dual<T>::constant(x[i]);
    }
    T value = f(x_const).value();

    // Remaining passes: get gradient
    auto grad = gradient(std::forward<F>(f), x);

    return {value, grad};
}

} // namespace dual
