#pragma once

/**
 * @file homomorphism.hpp
 * @brief Structure-preserving maps between monoids
 *
 * A homomorphism f: A -> B satisfies f(op(a, b)) = op(f(a), f(b)).
 * This is the mathematical concept that unifies fold, evaluation,
 * and the peasant algorithm: all are maps that preserve algebraic
 * structure.
 *
 * The deepest example: fold. Lists are the free monoid. fold is the
 * unique homomorphism from the free monoid to any target monoid.
 * This is WHY fold works.
 *
 * To define a monoid, provide free functions op(a, b) and identity(a)
 * discoverable via ADL in namespace stepanov.
 *
 * Reference: Stepanov & Rose, "From Mathematics to Generic Programming"
 */

#include <concepts>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace stepanov {

// =============================================================================
// The Monoid concept
// =============================================================================

/// A Monoid has an associative binary operation and an identity element.
///
///   op(a, b)      binary operation
///   identity(a)   identity element (argument used only for type deduction)
///
/// These are ADL free functions, as with zero/one in the peasant post.
/// Here we use op/identity because we're working at a more abstract
/// level: a single generic binary operation, not specifically addition
/// or multiplication.
///
template<typename M>
concept Monoid = std::semiregular<M> &&
    requires(M a, M b) {
        { op(a, b) } -> std::convertible_to<M>;
        { identity(a) } -> std::convertible_to<M>;
    };

// =============================================================================
// Concrete monoids
// =============================================================================

/// A wrapper making T a monoid under addition.
/// op = +, identity = 0.
template<typename T>
struct additive {
    T value{};

    constexpr additive() = default;
    constexpr explicit additive(T v) : value(v) {}

    constexpr bool operator==(const additive&) const = default;
};

template<typename T>
constexpr additive<T> op(const additive<T>& a, const additive<T>& b) {
    return additive<T>(a.value + b.value);
}

template<typename T>
constexpr additive<T> identity(const additive<T>&) {
    return additive<T>(T{0});
}

static_assert(Monoid<additive<int>>);
static_assert(Monoid<additive<double>>);

/// A wrapper making T a monoid under multiplication.
/// op = *, identity = 1.
template<typename T>
struct multiplicative {
    T value{};

    constexpr multiplicative() = default;
    constexpr explicit multiplicative(T v) : value(v) {}

    constexpr bool operator==(const multiplicative&) const = default;
};

template<typename T>
constexpr multiplicative<T> op(const multiplicative<T>& a, const multiplicative<T>& b) {
    return multiplicative<T>(a.value * b.value);
}

template<typename T>
constexpr multiplicative<T> identity(const multiplicative<T>&) {
    return multiplicative<T>(T{1});
}

static_assert(Monoid<multiplicative<int>>);
static_assert(Monoid<multiplicative<double>>);

/// std::string as a monoid under concatenation.
/// op = +, identity = "".
///
/// We wrap it so the ADL free functions live in namespace stepanov.
struct string_monoid {
    std::string value;

    string_monoid() = default;
    explicit string_monoid(std::string s) : value(std::move(s)) {}

    bool operator==(const string_monoid&) const = default;
};

inline string_monoid op(const string_monoid& a, const string_monoid& b) {
    return string_monoid(a.value + b.value);
}

inline string_monoid identity(const string_monoid&) {
    return string_monoid("");
}

static_assert(Monoid<string_monoid>);

/// std::vector<T> as a monoid under concatenation.
/// This is the FREE monoid on T: the most general monoid whose
/// elements are finite sequences of T values.
///
/// op = concatenation, identity = empty vector.
template<typename T>
struct list_monoid {
    std::vector<T> value;

    list_monoid() = default;
    explicit list_monoid(std::vector<T> v) : value(std::move(v)) {}

    bool operator==(const list_monoid&) const = default;
};

template<typename T>
list_monoid<T> op(const list_monoid<T>& a, const list_monoid<T>& b) {
    std::vector<T> result;
    result.reserve(a.value.size() + b.value.size());
    result.insert(result.end(), a.value.begin(), a.value.end());
    result.insert(result.end(), b.value.begin(), b.value.end());
    return list_monoid<T>(std::move(result));
}

template<typename T>
list_monoid<T> identity(const list_monoid<T>&) {
    return list_monoid<T>();
}

static_assert(Monoid<list_monoid<int>>);

/// T under max. identity = numeric_limits<T>::lowest() (negative infinity
/// for the type).
template<typename T>
struct max_monoid {
    T value = std::numeric_limits<T>::lowest();

    constexpr max_monoid() = default;
    constexpr explicit max_monoid(T v) : value(v) {}

    constexpr bool operator==(const max_monoid&) const = default;
};

template<typename T>
constexpr max_monoid<T> op(const max_monoid<T>& a, const max_monoid<T>& b) {
    return max_monoid<T>(a.value > b.value ? a.value : b.value);
}

template<typename T>
constexpr max_monoid<T> identity(const max_monoid<T>&) {
    return max_monoid<T>(std::numeric_limits<T>::lowest());
}

static_assert(Monoid<max_monoid<int>>);

// =============================================================================
// Homomorphism verification
// =============================================================================

/// Check the homomorphism property: f(op(a, b)) == op(f(a), f(b))
/// This is a runtime property test, not a proof, but it catches violations.
template<Monoid A, Monoid B, typename F>
bool is_homomorphism(F f, const A& a1, const A& a2) {
    return f(op(a1, a2)) == op(f(a1), f(a2));
}

/// Check identity preservation: f(identity_A) == identity_B
template<Monoid A, Monoid B, typename F>
bool preserves_identity(F f) {
    A a{};
    return f(identity(a)) == identity(B{});
}

// =============================================================================
// fold: the universal homomorphism from the free monoid
// =============================================================================

/// fold: the universal homomorphism from list_monoid<T> to any monoid M.
/// Given a function f: T -> M, fold extends it to a homomorphism
/// from list_monoid<T> to M.
///
/// fold(f, [a, b, c]) = op(op(f(a), f(b)), f(c))
///
/// This works because lists are the FREE monoid: the most general
/// monoid on a set of generators. For any monoid M and any function
/// f: T -> M, there is exactly one homomorphism from list_monoid<T>
/// to M that extends f. That homomorphism is fold.
///
template<Monoid M, typename T, typename F>
    requires std::invocable<F, T> && std::convertible_to<std::invoke_result_t<F, T>, M>
M fold(F f, const std::vector<T>& xs) {
    M result = identity(M{});
    for (const auto& x : xs)
        result = op(result, std::invoke(f, x));
    return result;
}

/// Special case: when T is already a monoid and f is the identity.
template<Monoid M>
M fold(const std::vector<M>& xs) {
    M result = identity(M{});
    for (const auto& x : xs)
        result = op(result, x);
    return result;
}

// =============================================================================
// Examples of homomorphisms
// =============================================================================

/// length: string_monoid -> additive<int>
/// length(s1 + s2) = length(s1) + length(s2)
///
/// The length function preserves the monoid structure: concatenation
/// in strings maps to addition in integers.
inline additive<int> length_hom(const string_monoid& s) {
    return additive<int>(static_cast<int>(s.value.size()));
}

/// sum: list_monoid<int> -> additive<int>
/// sum(xs ++ ys) = sum(xs) + sum(ys)
inline additive<int> sum_hom(const list_monoid<int>& xs) {
    int s = 0;
    for (int x : xs.value)
        s += x;
    return additive<int>(s);
}

/// product: list_monoid<int> -> multiplicative<int>
/// prod(xs ++ ys) = prod(xs) * prod(ys)
inline multiplicative<int> product_hom(const list_monoid<int>& xs) {
    int p = 1;
    for (int x : xs.value)
        p *= x;
    return multiplicative<int>(p);
}

/// count: list_monoid<T> -> additive<int>
/// count(xs ++ ys) = count(xs) + count(ys)
template<typename T>
additive<int> count_hom(const list_monoid<T>& xs) {
    return additive<int>(static_cast<int>(xs.value.size()));
}

/// log: multiplicative<double> -> additive<double>
/// log(a * b) = log(a) + log(b)
///
/// The logarithm is a homomorphism. This is the algebraic content
/// of the logarithm's defining property.
inline additive<double> log_hom(const multiplicative<double>& x) {
    return additive<double>(std::log(x.value));
}

} // namespace stepanov
