#pragma once

/**
 * @file core.hpp
 * @brief Forward-mode automatic differentiation via dual numbers
 *
 * THIS MODULE TEACHES: Dual numbers (a + b*epsilon where epsilon^2 = 0) give us
 * automatic differentiation through operator overloading. Compute f(x) and f'(x)
 * simultaneously with no symbolic manipulation.
 *
 * Key insight: If we define arithmetic on dual numbers correctly,
 * computing f(dual(x, 1)) yields dual(f(x), f'(x)). The derivative
 * emerges from the algebra.
 *
 * C++ advantage: Zero runtime overhead - the dual number operations
 * inline completely, compiling to the same code as hand-written derivatives.
 */

#include "concepts.hpp"
#include <utility>

namespace dual {

/**
 * Dual number: represents a value and its derivative simultaneously.
 *
 * A dual number x + epsilon*y where epsilon^2 = 0.
 * When we evaluate f(a + epsilon) we get f(a) + epsilon*f'(a).
 *
 * @tparam T The underlying numeric type (must satisfy field concept)
 */
template<typename T>
    requires field<T>
class dual {
private:
    T value_;       // Function value
    T derivative_;  // Derivative value

public:
    using value_type = T;

    // Constructors
    constexpr dual() : value_(T(0)), derivative_(T(0)) {}

    constexpr explicit dual(T val, T deriv = T(0))
        : value_(val), derivative_(deriv) {}

    /**
     * Create a variable for differentiation (derivative = 1).
     * Use this for the variable you're differentiating with respect to.
     */
    static constexpr dual variable(T val, T seed = T(1)) {
        return dual(val, seed);
    }

    /**
     * Create a constant (derivative = 0).
     * Use this for parameters that don't vary.
     */
    static constexpr dual constant(T val) {
        return dual(val, T(0));
    }

    // Accessors
    constexpr T value() const { return value_; }
    constexpr T derivative() const { return derivative_; }

    // Arithmetic operations implementing the chain rule

    constexpr dual operator-() const {
        return dual(-value_, -derivative_);
    }

    constexpr dual operator+(const dual& other) const {
        return dual(value_ + other.value_, derivative_ + other.derivative_);
    }

    constexpr dual operator-(const dual& other) const {
        return dual(value_ - other.value_, derivative_ - other.derivative_);
    }

    constexpr dual operator*(const dual& other) const {
        // Product rule: (fg)' = f'g + fg'
        return dual(value_ * other.value_,
                   derivative_ * other.value_ + value_ * other.derivative_);
    }

    constexpr dual operator/(const dual& other) const {
        // Quotient rule: (f/g)' = (f'g - fg')/g^2
        T g_squared = other.value_ * other.value_;
        return dual(value_ / other.value_,
                   (derivative_ * other.value_ - value_ * other.derivative_) / g_squared);
    }

    // Compound assignments
    constexpr dual& operator+=(const dual& other) {
        value_ += other.value_;
        derivative_ += other.derivative_;
        return *this;
    }

    constexpr dual& operator-=(const dual& other) {
        value_ -= other.value_;
        derivative_ -= other.derivative_;
        return *this;
    }

    constexpr dual& operator*=(const dual& other) {
        derivative_ = derivative_ * other.value_ + value_ * other.derivative_;
        value_ *= other.value_;
        return *this;
    }

    constexpr dual& operator/=(const dual& other) {
        T g_squared = other.value_ * other.value_;
        derivative_ = (derivative_ * other.value_ - value_ * other.derivative_) / g_squared;
        value_ /= other.value_;
        return *this;
    }

    // Comparison (based on value only, derivative is tangent information)
    constexpr bool operator==(const dual& other) const {
        return value_ == other.value_;
    }

    constexpr bool operator!=(const dual& other) const {
        return value_ != other.value_;
    }

    constexpr bool operator<(const dual& other) const {
        return value_ < other.value_;
    }

    constexpr bool operator<=(const dual& other) const {
        return value_ <= other.value_;
    }

    constexpr bool operator>(const dual& other) const {
        return value_ > other.value_;
    }

    constexpr bool operator>=(const dual& other) const {
        return value_ >= other.value_;
    }

    // Mixed operations with scalars
    constexpr dual operator+(T scalar) const {
        return dual(value_ + scalar, derivative_);
    }

    constexpr dual operator-(T scalar) const {
        return dual(value_ - scalar, derivative_);
    }

    constexpr dual operator*(T scalar) const {
        return dual(value_ * scalar, derivative_ * scalar);
    }

    constexpr dual operator/(T scalar) const {
        return dual(value_ / scalar, derivative_ / scalar);
    }

    friend constexpr dual operator+(T scalar, const dual& d) {
        return d + scalar;
    }

    friend constexpr dual operator-(T scalar, const dual& d) {
        return dual(scalar - d.value_, -d.derivative_);
    }

    friend constexpr dual operator*(T scalar, const dual& d) {
        return d * scalar;
    }

    friend constexpr dual operator/(T scalar, const dual& d) {
        T d_squared = d.value_ * d.value_;
        return dual(scalar / d.value_, -scalar * d.derivative_ / d_squared);
    }
};

/**
 * Convenience function to compute f(x) and f'(x) in one call.
 *
 * @param f A function callable with dual<T>
 * @param x The point at which to evaluate
 * @return A pair (f(x), f'(x))
 */
template<typename F, typename T>
constexpr std::pair<T, T> differentiate(F&& f, T x) {
    auto result = f(dual<T>::variable(x));
    return {result.value(), result.derivative()};
}

} // namespace dual
