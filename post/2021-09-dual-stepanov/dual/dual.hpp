#pragma once

/**
 * @file dual.hpp
 * @brief Forward-mode automatic differentiation library
 *
 * This is the main umbrella header for the dual library.
 * Include this header to get access to all dual number functionality.
 *
 * Usage:
 * @code
 * #include <dual/dual.hpp>
 *
 * // Compute f(x) = x^2 and its derivative at x = 3
 * auto x = dual::dual<double>::variable(3.0);
 * auto y = x * x;
 * // y.value() == 9.0, y.derivative() == 6.0
 *
 * // Or use the convenience function
 * auto [value, deriv] = dual::differentiate([](auto x) { return x*x; }, 3.0);
 * @endcode
 *
 * For higher-order derivatives:
 * @code
 * // Second derivative using dual2
 * auto result = dual::differentiate2([](auto x) { return sin(x); }, 1.0);
 * // result.value, result.first, result.second
 *
 * // Arbitrary order using jets
 * auto derivs = dual::derivatives<5>([](auto x) { return exp(x); }, 1.0);
 * // derivs[0] = e, derivs[1] = e, ..., derivs[5] = e
 * @endcode
 *
 * For numerical verification:
 * @code
 * auto comparison = dual::numerical::compare_derivatives(
 *     [](auto x) { return sin(x); }, 1.0);
 * assert(comparison.agrees());
 * @endcode
 */

#include "concepts.hpp"
#include "core.hpp"
#include "functions.hpp"
#include "higher_order.hpp"
#include "gradient.hpp"
#include "numerical.hpp"
