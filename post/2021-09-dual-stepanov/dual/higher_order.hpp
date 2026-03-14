#pragma once

/**
 * @file higher_order.hpp
 * @brief Higher-order derivatives via nested dual numbers and Taylor jets
 *
 * Two approaches for computing higher derivatives:
 *
 * 1. Nested dual numbers: dual<dual<T>> gives second derivatives
 *    - Simple but requires careful initialization
 *    - Each nesting level doubles the computation
 *
 * 2. Taylor jets: jet<T, N> stores first N Taylor coefficients
 *    - More efficient for many derivatives
 *    - Natural for truncated power series
 */

#include "core.hpp"
#include "functions.hpp"
#include <array>

namespace dual {

// =============================================================================
// Nested dual numbers for second derivatives
// =============================================================================

/**
 * Type alias for second-order dual numbers.
 * dual2<T> computes f(x), f'(x), and f''(x) simultaneously.
 */
template<typename T>
using dual2 = dual<dual<T>>;

/**
 * Create a second-order dual variable for computing second derivatives.
 *
 * Initialization: To compute f''(x), we set up:
 *   outer.value = inner dual for f, f'
 *   outer.derivative = inner dual tracking how f' changes
 *
 * @param val The point at which to evaluate
 * @return A properly initialized dual2 for second-order differentiation
 */
template<typename T>
constexpr dual2<T> make_dual2_variable(T val) {
    // Inner dual: value = x, derivative = 1 (we're differentiating wrt x)
    // Outer dual: value = inner dual, derivative = (1, 0) (how inner.value changes)
    return dual2<T>(
        dual<T>(val, T(1)),   // inner: x + epsilon
        dual<T>(T(1), T(0))   // d/dx of (x + epsilon) = 1 + 0*epsilon
    );
}

/**
 * Container for extracted derivatives from dual2.
 */
template<typename T>
struct second_order_result {
    T value;      // f(x)
    T first;      // f'(x)
    T second;     // f''(x)

    constexpr second_order_result(const dual2<T>& d)
        : value(d.value().value())
        , first(d.value().derivative())
        , second(d.derivative().derivative()) {}
};

/**
 * Compute f(x), f'(x), f''(x) in one pass.
 */
template<typename F, typename T>
constexpr second_order_result<T> differentiate2(F&& f, T x) {
    return second_order_result<T>(f(make_dual2_variable(x)));
}

// =============================================================================
// Taylor jets for arbitrary-order derivatives
// =============================================================================

/**
 * Taylor jet: stores coefficients of the Taylor series truncated at order N.
 *
 * If f(a + h) = c0 + c1*h + c2*h^2/2! + ... + cN*h^N/N! + O(h^(N+1))
 * then jet stores [c0, c1, c2, ..., cN] = [f(a), f'(a), f''(a), ..., f^(N)(a)]
 *
 * Arithmetic propagates these coefficients using Leibniz rules.
 *
 * @tparam T The underlying numeric type
 * @tparam N The maximum derivative order (N+1 coefficients stored)
 */
template<typename T, std::size_t N>
    requires field<T>
class jet {
private:
    std::array<T, N + 1> coeffs_;  // coeffs_[k] = f^(k)(a) / k!

public:
    using value_type = T;
    static constexpr std::size_t order = N;

    // Constructors
    constexpr jet() : coeffs_{} {
        coeffs_.fill(T(0));
    }

    constexpr explicit jet(T val) : coeffs_{} {
        coeffs_.fill(T(0));
        coeffs_[0] = val;
    }

    constexpr jet(std::initializer_list<T> init) : coeffs_{} {
        coeffs_.fill(T(0));
        std::size_t i = 0;
        for (auto it = init.begin(); it != init.end() && i <= N; ++it, ++i) {
            coeffs_[i] = *it;
        }
    }

    /**
     * Create a jet representing the variable x at point a.
     * The Taylor series of x around a is: a + h
     * So coefficients are [a, 1, 0, 0, ...]
     */
    static constexpr jet variable(T a) {
        jet j;
        j.coeffs_[0] = a;
        if constexpr (N >= 1) {
            j.coeffs_[1] = T(1);
        }
        return j;
    }

    /**
     * Create a jet representing a constant.
     * The Taylor series of c is just [c, 0, 0, ...]
     */
    static constexpr jet constant(T c) {
        jet j;
        j.coeffs_[0] = c;
        return j;
    }

    // Accessors
    constexpr T operator[](std::size_t k) const {
        return k <= N ? coeffs_[k] : T(0);
    }

    constexpr T& operator[](std::size_t k) {
        return coeffs_[k];
    }

    constexpr T value() const { return coeffs_[0]; }

    /**
     * Get the k-th derivative at the expansion point.
     * Note: coeffs_[k] stores f^(k)/k!, so we multiply by k!
     */
    constexpr T derivative(std::size_t k = 1) const {
        if (k > N) return T(0);
        T factorial = T(1);
        for (std::size_t i = 2; i <= k; ++i) {
            factorial *= T(i);
        }
        return coeffs_[k] * factorial;
    }

    // Arithmetic operations
    constexpr jet operator-() const {
        jet result;
        for (std::size_t i = 0; i <= N; ++i) {
            result.coeffs_[i] = -coeffs_[i];
        }
        return result;
    }

    constexpr jet operator+(const jet& other) const {
        jet result;
        for (std::size_t i = 0; i <= N; ++i) {
            result.coeffs_[i] = coeffs_[i] + other.coeffs_[i];
        }
        return result;
    }

    constexpr jet operator-(const jet& other) const {
        jet result;
        for (std::size_t i = 0; i <= N; ++i) {
            result.coeffs_[i] = coeffs_[i] - other.coeffs_[i];
        }
        return result;
    }

    constexpr jet operator*(const jet& other) const {
        // (f*g)^(n) = sum_{k=0}^{n} C(n,k) f^(k) g^(n-k)
        // But we store f^(k)/k!, so:
        // (fg)[n] = sum_{k=0}^{n} f[k] * g[n-k]
        jet result;
        for (std::size_t n = 0; n <= N; ++n) {
            T sum = T(0);
            for (std::size_t k = 0; k <= n; ++k) {
                sum += coeffs_[k] * other.coeffs_[n - k];
            }
            result.coeffs_[n] = sum;
        }
        return result;
    }

    constexpr jet operator/(const jet& other) const {
        // If h = f/g, then f = g*h
        // h[n] = (f[n] - sum_{k=0}^{n-1} g[n-k]*h[k]) / g[0]
        jet result;
        for (std::size_t n = 0; n <= N; ++n) {
            T sum = coeffs_[n];
            for (std::size_t k = 0; k < n; ++k) {
                sum -= other.coeffs_[n - k] * result.coeffs_[k];
            }
            result.coeffs_[n] = sum / other.coeffs_[0];
        }
        return result;
    }

    // Scalar operations
    constexpr jet operator+(T scalar) const {
        jet result = *this;
        result.coeffs_[0] += scalar;
        return result;
    }

    constexpr jet operator-(T scalar) const {
        jet result = *this;
        result.coeffs_[0] -= scalar;
        return result;
    }

    constexpr jet operator*(T scalar) const {
        jet result;
        for (std::size_t i = 0; i <= N; ++i) {
            result.coeffs_[i] = coeffs_[i] * scalar;
        }
        return result;
    }

    constexpr jet operator/(T scalar) const {
        jet result;
        for (std::size_t i = 0; i <= N; ++i) {
            result.coeffs_[i] = coeffs_[i] / scalar;
        }
        return result;
    }

    friend constexpr jet operator+(T scalar, const jet& j) { return j + scalar; }
    friend constexpr jet operator-(T scalar, const jet& j) { return jet(scalar) - j; }
    friend constexpr jet operator*(T scalar, const jet& j) { return j * scalar; }
    friend constexpr jet operator/(T scalar, const jet& j) { return jet(scalar) / j; }

    // Comparison
    constexpr bool operator==(const jet& other) const {
        return coeffs_[0] == other.coeffs_[0];
    }

    constexpr bool operator!=(const jet& other) const {
        return coeffs_[0] != other.coeffs_[0];
    }
};

// =============================================================================
// Mathematical functions for jets
// =============================================================================

template<typename T, std::size_t N>
jet<T, N> exp(const jet<T, N>& x) {
    // exp(f)[n] = sum_{k=0}^{n-1} (n-k)/n * f[n-k] * exp(f)[k]
    // with exp(f)[0] = exp(f[0])
    jet<T, N> result;
    result[0] = std::exp(x[0]);
    for (std::size_t n = 1; n <= N; ++n) {
        T sum = T(0);
        for (std::size_t k = 0; k < n; ++k) {
            sum += T(n - k) * x[n - k] * result[k];
        }
        result[n] = sum / T(n);
    }
    return result;
}

template<typename T, std::size_t N>
jet<T, N> log(const jet<T, N>& x) {
    // log(f)[n] = (f[n] - sum_{k=1}^{n-1} k/n * log(f)[k] * f[n-k]) / f[0]
    jet<T, N> result;
    result[0] = std::log(x[0]);
    for (std::size_t n = 1; n <= N; ++n) {
        T sum = x[n];
        for (std::size_t k = 1; k < n; ++k) {
            sum -= T(k) / T(n) * result[k] * x[n - k];
        }
        result[n] = sum / x[0];
    }
    return result;
}

template<typename T, std::size_t N>
jet<T, N> sqrt(const jet<T, N>& x) {
    // If g = sqrt(f), then g² = f
    // (g*g)[n] = sum_{k=0}^n g[k]*g[n-k] = f[n]
    // 2*g[0]*g[n] + sum_{k=1}^{n-1} g[k]*g[n-k] = f[n]
    // g[n] = (f[n] - sum_{k=1}^{n-1} g[k]*g[n-k]) / (2*g[0])
    jet<T, N> result;
    result[0] = std::sqrt(x[0]);
    for (std::size_t n = 1; n <= N; ++n) {
        T sum = x[n];
        for (std::size_t k = 1; k < n; ++k) {
            sum -= result[k] * result[n - k];
        }
        result[n] = sum / (T(2) * result[0]);
    }
    return result;
}

template<typename T, std::size_t N>
jet<T, N> sin(const jet<T, N>& x);

template<typename T, std::size_t N>
jet<T, N> cos(const jet<T, N>& x);

template<typename T, std::size_t N>
jet<T, N> sin(const jet<T, N>& x) {
    // sin(f)[n] = sum_{k=0}^{n-1} (n-k)/n * f[n-k] * cos(f)[k]
    auto c = cos(x);
    jet<T, N> result;
    result[0] = std::sin(x[0]);
    for (std::size_t n = 1; n <= N; ++n) {
        T sum = T(0);
        for (std::size_t k = 0; k < n; ++k) {
            sum += T(n - k) * x[n - k] * c[k];
        }
        result[n] = sum / T(n);
    }
    return result;
}

template<typename T, std::size_t N>
jet<T, N> cos(const jet<T, N>& x) {
    // cos(f)[n] = -sum_{k=0}^{n-1} (n-k)/n * f[n-k] * sin(f)[k]
    jet<T, N> result;
    result[0] = std::cos(x[0]);

    // Pre-compute sin values iteratively
    jet<T, N> s;
    s[0] = std::sin(x[0]);

    for (std::size_t n = 1; n <= N; ++n) {
        // Update sin[n] using cos values computed so far
        T sin_sum = T(0);
        for (std::size_t k = 0; k < n; ++k) {
            sin_sum += T(n - k) * x[n - k] * result[k];
        }
        s[n] = sin_sum / T(n);

        // Compute cos[n] using sin values
        T cos_sum = T(0);
        for (std::size_t k = 0; k < n; ++k) {
            cos_sum += T(n - k) * x[n - k] * s[k];
        }
        result[n] = -cos_sum / T(n);
    }
    return result;
}

template<typename T, std::size_t N>
jet<T, N> pow(const jet<T, N>& base, T exponent) {
    // f^a = exp(a * log(f))
    return exp(exponent * log(base));
}

/**
 * Compute derivatives up to order N at point x.
 */
template<std::size_t N, typename F, typename T>
std::array<T, N + 1> derivatives(F&& f, T x) {
    auto result = f(jet<T, N>::variable(x));
    std::array<T, N + 1> derivs;
    for (std::size_t k = 0; k <= N; ++k) {
        derivs[k] = result.derivative(k);
    }
    return derivs;
}

} // namespace dual
