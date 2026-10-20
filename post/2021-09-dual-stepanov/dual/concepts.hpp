#pragma once

/**
 * @file concepts.hpp
 * @brief Self-contained field concept for dual numbers
 *
 * This header provides the algebraic concept requirements for the dual library.
 * It is self-contained with no external dependencies.
 */

#include <concepts>
#include <type_traits>

namespace dual {

/**
 * Field concept: an algebraic structure supporting +, -, *, / with expected properties.
 *
 * A field is a set F with two operations (addition and multiplication) such that:
 * - (F, +) is an abelian group with identity 0
 * - (F \ {0}, *) is an abelian group with identity 1
 * - Multiplication distributes over addition
 *
 * This concept captures the syntactic requirements for types that behave like fields.
 */
template<typename T>
concept field = requires(T a, T b) {
    // Constructible from integers (for 0 and 1)
    { T(0) } -> std::convertible_to<T>;
    { T(1) } -> std::convertible_to<T>;

    // Additive operations
    { a + b } -> std::convertible_to<T>;
    { a - b } -> std::convertible_to<T>;
    { -a } -> std::convertible_to<T>;

    // Multiplicative operations
    { a * b } -> std::convertible_to<T>;
    { a / b } -> std::convertible_to<T>;

    // Comparison (for equality)
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

/**
 * Ordered field concept: a field with a total ordering compatible with operations.
 *
 * Required for comparisons and algorithms that need to compare magnitudes.
 */
template<typename T>
concept ordered_field = field<T> && requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
};

} // namespace dual
