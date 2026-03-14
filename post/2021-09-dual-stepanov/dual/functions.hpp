#pragma once

/**
 * @file functions.hpp
 * @brief Mathematical functions for dual numbers
 *
 * Each function implements the chain rule automatically.
 * For f(g(x)), we have f(g(x))' = f'(g(x)) * g'(x).
 * The derivative part of the input carries g'(x), and we multiply
 * by f'(g(x)) to get the final derivative.
 *
 * ADL (Argument-Dependent Lookup) is used to support nested dual numbers.
 * This allows dual<dual<T>> to work correctly for second derivatives.
 */

#include "core.hpp"
#include <cmath>

namespace dual {

// =============================================================================
// Basic functions
// =============================================================================

template<typename T>
constexpr dual<T> sqrt(const dual<T>& x) {
    using std::sqrt;
    T sqrt_val = sqrt(x.value());
    // d/dx sqrt(x) = 1/(2*sqrt(x))
    return dual<T>(sqrt_val, x.derivative() / (T(2) * sqrt_val));
}

template<typename T>
constexpr dual<T> cbrt(const dual<T>& x) {
    using std::cbrt;
    T cbrt_val = cbrt(x.value());
    // d/dx cbrt(x) = 1/(3*cbrt(x)^2)
    return dual<T>(cbrt_val, x.derivative() / (T(3) * cbrt_val * cbrt_val));
}

template<typename T>
constexpr dual<T> abs(const dual<T>& x) {
    if (x.value() > T(0)) {
        return x;
    } else if (x.value() < T(0)) {
        return -x;
    } else {
        // Non-differentiable at 0, return 0 derivative as convention
        return dual<T>(T(0), T(0));
    }
}

// =============================================================================
// Exponential and logarithmic functions
// =============================================================================

template<typename T>
constexpr dual<T> exp(const dual<T>& x) {
    using std::exp;
    T exp_val = exp(x.value());
    // d/dx exp(x) = exp(x)
    return dual<T>(exp_val, x.derivative() * exp_val);
}

template<typename T>
constexpr dual<T> exp2(const dual<T>& x) {
    using std::exp2;
    using std::log;
    T exp2_val = exp2(x.value());
    // d/dx 2^x = 2^x * ln(2)
    return dual<T>(exp2_val, x.derivative() * exp2_val * log(T(2)));
}

template<typename T>
constexpr dual<T> expm1(const dual<T>& x) {
    using std::expm1;
    using std::exp;
    // d/dx (exp(x) - 1) = exp(x)
    return dual<T>(expm1(x.value()), x.derivative() * exp(x.value()));
}

template<typename T>
constexpr dual<T> log(const dual<T>& x) {
    using std::log;
    // d/dx ln(x) = 1/x
    return dual<T>(log(x.value()), x.derivative() / x.value());
}

template<typename T>
constexpr dual<T> log2(const dual<T>& x) {
    using std::log;
    using std::log2;
    // d/dx log2(x) = 1/(x * ln(2))
    return dual<T>(log2(x.value()), x.derivative() / (x.value() * log(T(2))));
}

template<typename T>
constexpr dual<T> log10(const dual<T>& x) {
    using std::log;
    using std::log10;
    // d/dx log10(x) = 1/(x * ln(10))
    return dual<T>(log10(x.value()), x.derivative() / (x.value() * log(T(10))));
}

template<typename T>
constexpr dual<T> log1p(const dual<T>& x) {
    using std::log1p;
    // d/dx ln(1+x) = 1/(1+x)
    return dual<T>(log1p(x.value()), x.derivative() / (T(1) + x.value()));
}

// =============================================================================
// Trigonometric functions
// =============================================================================

template<typename T>
constexpr dual<T> sin(const dual<T>& x) {
    using std::sin;
    using std::cos;
    // d/dx sin(x) = cos(x)
    return dual<T>(sin(x.value()), x.derivative() * cos(x.value()));
}

template<typename T>
constexpr dual<T> cos(const dual<T>& x) {
    using std::sin;
    using std::cos;
    // d/dx cos(x) = -sin(x)
    return dual<T>(cos(x.value()), -x.derivative() * sin(x.value()));
}

template<typename T>
constexpr dual<T> tan(const dual<T>& x) {
    using std::tan;
    T tan_val = tan(x.value());
    T sec_squared = T(1) + tan_val * tan_val;  // sec^2(x) = 1 + tan^2(x)
    // d/dx tan(x) = sec^2(x)
    return dual<T>(tan_val, x.derivative() * sec_squared);
}

// =============================================================================
// Inverse trigonometric functions
// =============================================================================

template<typename T>
constexpr dual<T> asin(const dual<T>& x) {
    using std::asin;
    using std::sqrt;
    // d/dx asin(x) = 1/sqrt(1 - x^2)
    return dual<T>(asin(x.value()),
                  x.derivative() / sqrt(T(1) - x.value() * x.value()));
}

template<typename T>
constexpr dual<T> acos(const dual<T>& x) {
    using std::acos;
    using std::sqrt;
    // d/dx acos(x) = -1/sqrt(1 - x^2)
    return dual<T>(acos(x.value()),
                  -x.derivative() / sqrt(T(1) - x.value() * x.value()));
}

template<typename T>
constexpr dual<T> atan(const dual<T>& x) {
    using std::atan;
    // d/dx atan(x) = 1/(1 + x^2)
    return dual<T>(atan(x.value()),
                  x.derivative() / (T(1) + x.value() * x.value()));
}

template<typename T>
constexpr dual<T> atan2(const dual<T>& y, const dual<T>& x) {
    using std::atan2;
    T denom = x.value() * x.value() + y.value() * y.value();
    // d/dy atan2(y,x) = x/(x^2 + y^2)
    // d/dx atan2(y,x) = -y/(x^2 + y^2)
    return dual<T>(atan2(y.value(), x.value()),
                  (y.derivative() * x.value() - y.value() * x.derivative()) / denom);
}

// =============================================================================
// Hyperbolic functions
// =============================================================================

template<typename T>
constexpr dual<T> sinh(const dual<T>& x) {
    using std::sinh;
    using std::cosh;
    // d/dx sinh(x) = cosh(x)
    return dual<T>(sinh(x.value()), x.derivative() * cosh(x.value()));
}

template<typename T>
constexpr dual<T> cosh(const dual<T>& x) {
    using std::sinh;
    using std::cosh;
    // d/dx cosh(x) = sinh(x)
    return dual<T>(cosh(x.value()), x.derivative() * sinh(x.value()));
}

template<typename T>
constexpr dual<T> tanh(const dual<T>& x) {
    using std::tanh;
    T tanh_val = tanh(x.value());
    // d/dx tanh(x) = sech^2(x) = 1 - tanh^2(x)
    return dual<T>(tanh_val, x.derivative() * (T(1) - tanh_val * tanh_val));
}

// =============================================================================
// Inverse hyperbolic functions
// =============================================================================

template<typename T>
constexpr dual<T> asinh(const dual<T>& x) {
    using std::asinh;
    using std::sqrt;
    // d/dx asinh(x) = 1/sqrt(x^2 + 1)
    return dual<T>(asinh(x.value()),
                  x.derivative() / sqrt(x.value() * x.value() + T(1)));
}

template<typename T>
constexpr dual<T> acosh(const dual<T>& x) {
    using std::acosh;
    using std::sqrt;
    // d/dx acosh(x) = 1/sqrt(x^2 - 1)
    return dual<T>(acosh(x.value()),
                  x.derivative() / sqrt(x.value() * x.value() - T(1)));
}

template<typename T>
constexpr dual<T> atanh(const dual<T>& x) {
    using std::atanh;
    // d/dx atanh(x) = 1/(1 - x^2)
    return dual<T>(atanh(x.value()),
                  x.derivative() / (T(1) - x.value() * x.value()));
}

// =============================================================================
// Power functions
// =============================================================================

template<typename T>
constexpr dual<T> pow(const dual<T>& base, const dual<T>& exponent) {
    using std::pow;
    using std::log;
    // For f(x) = base^exponent where both are dual:
    // ln(f) = exponent * ln(base)
    // f'/f = exponent' * ln(base) + exponent * base'/base
    // f' = f * (exponent' * ln(base) + exponent * base'/base)
    T val = pow(base.value(), exponent.value());
    T deriv = val * (exponent.derivative() * log(base.value()) +
                     exponent.value() * base.derivative() / base.value());
    return dual<T>(val, deriv);
}

template<typename T>
constexpr dual<T> pow(const dual<T>& base, T exponent) {
    using std::pow;
    // f(x) = x^n, f'(x) = n * x^(n-1)
    T val = pow(base.value(), exponent);
    return dual<T>(val, base.derivative() * exponent * pow(base.value(), exponent - T(1)));
}

template<typename T>
constexpr dual<T> pow(T base, const dual<T>& exponent) {
    using std::pow;
    using std::log;
    // f(x) = a^x, f'(x) = a^x * ln(a)
    T val = pow(base, exponent.value());
    return dual<T>(val, exponent.derivative() * val * log(base));
}

// =============================================================================
// Special functions
// =============================================================================

template<typename T>
constexpr dual<T> hypot(const dual<T>& x, const dual<T>& y) {
    using std::hypot;
    T h = hypot(x.value(), y.value());
    if (h == T(0)) {
        return dual<T>(T(0), T(0));
    }
    // d/dx hypot(x,y) = x/hypot(x,y)
    // d/dy hypot(x,y) = y/hypot(x,y)
    return dual<T>(h, (x.value() * x.derivative() + y.value() * y.derivative()) / h);
}

template<typename T>
constexpr dual<T> erf(const dual<T>& x) {
    using std::erf;
    using std::exp;
    // d/dx erf(x) = 2/sqrt(pi) * exp(-x^2)
    constexpr T two_over_sqrt_pi = T(1.1283791670955126);  // 2/sqrt(pi)
    return dual<T>(erf(x.value()),
                  x.derivative() * two_over_sqrt_pi * exp(-x.value() * x.value()));
}

template<typename T>
constexpr dual<T> erfc(const dual<T>& x) {
    using std::erfc;
    using std::exp;
    // d/dx erfc(x) = -2/sqrt(pi) * exp(-x^2)
    constexpr T two_over_sqrt_pi = T(1.1283791670955126);
    return dual<T>(erfc(x.value()),
                  -x.derivative() * two_over_sqrt_pi * exp(-x.value() * x.value()));
}

} // namespace dual
