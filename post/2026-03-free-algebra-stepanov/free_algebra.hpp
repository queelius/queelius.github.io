#pragma once

/**
 * @file free_algebra.hpp
 * @brief Free monoids, free commutative monoids, and the universal property
 *
 * A free algebra on a set S is the most general algebraic structure
 * satisfying a given set of axioms, built from generators in S.
 *
 * The free monoid on S is the set of finite sequences (lists) over S,
 * with concatenation as the operation and the empty sequence as identity.
 * No equations hold except those forced by the monoid axioms: associativity
 * and identity. In particular, [a, b] != [b, a] and [a, a] != [a].
 *
 * The free commutative monoid on S is the set of multisets over S.
 * Commutativity is imposed, so order doesn't matter, but multiplicity does.
 *
 * The universal property: for any function f: S -> M where M is a monoid,
 * there exists a unique homomorphism extend(f): free_monoid<S> -> M.
 * That homomorphism is fold. The universal property says fold is the
 * only structure-preserving way to interpret a list in a monoid.
 *
 * Reference: Stepanov & Rose, "From Mathematics to Generic Programming"
 */

#include <concepts>
#include <functional>
#include <map>
#include <numeric>
#include <vector>

namespace stepanov {

// =============================================================================
// Monoid concept
// =============================================================================

/// A Monoid has an associative binary operation and an identity element.
///
/// Operations are free functions discoverable via ADL:
///   op(a, b)       — the binary operation
///   identity(a)    — the identity element (argument used only for type)
///
template<typename M>
concept Monoid = std::semiregular<M> &&
    requires(M a, M b) {
        { op(a, b) } -> std::convertible_to<M>;
        { identity(a) } -> std::convertible_to<M>;
    };

// =============================================================================
// The free monoid: lists
// =============================================================================

/// The free monoid on T.
/// Elements are sequences (vectors) of T.
/// Operation is concatenation. Identity is the empty sequence.
///
/// "Free" means: no equations hold except those forced by the monoid axioms.
/// [a, b, c] is never equal to [b, a, c] (no commutativity imposed).
/// [a, a] is never equal to [a] (no idempotency imposed).
/// The structure is as general as possible.
///
template<typename T>
class free_monoid {
    std::vector<T> elements_;

public:
    using value_type = T;

    free_monoid() = default;
    explicit free_monoid(T x) : elements_{std::move(x)} {}
    explicit free_monoid(std::vector<T> xs) : elements_(std::move(xs)) {}
    free_monoid(std::initializer_list<T> xs) : elements_(xs) {}

    const std::vector<T>& elements() const { return elements_; }
    std::size_t size() const { return elements_.size(); }
    bool empty() const { return elements_.empty(); }

    bool operator==(const free_monoid&) const = default;
};

/// Monoid operation: concatenation.
template<typename T>
free_monoid<T> op(const free_monoid<T>& a, const free_monoid<T>& b) {
    std::vector<T> result;
    result.reserve(a.size() + b.size());
    result.insert(result.end(), a.elements().begin(), a.elements().end());
    result.insert(result.end(), b.elements().begin(), b.elements().end());
    return free_monoid<T>(std::move(result));
}

/// Monoid identity: the empty sequence.
template<typename T>
free_monoid<T> identity(const free_monoid<T>&) {
    return free_monoid<T>{};
}

static_assert(Monoid<free_monoid<int>>);
static_assert(Monoid<free_monoid<std::string>>);

// =============================================================================
// The universal property: extend (= fold)
// =============================================================================

/// The universal property of the free monoid.
///
/// Given f: T -> M, extend(f) is the unique monoid homomorphism
/// from free_monoid<T> to M such that extend(f)([x]) = f(x).
///
/// extend(f)([a, b, c]) = op(op(f(a), f(b)), f(c))
///
/// This IS fold. The universal property says fold is the only
/// structure-preserving way to interpret a list in a monoid.
///
template<Monoid M, typename T, typename F>
    requires std::invocable<F, T> &&
             std::convertible_to<std::invoke_result_t<F, T>, M>
M extend(F f, const free_monoid<T>& xs) {
    M result = identity(M{});
    for (const auto& x : xs.elements())
        result = op(result, std::invoke(f, x));
    return result;
}

/// Special case: when T = M and f = identity function.
/// This is just "collapse the list using the monoid operation."
template<Monoid M>
M collapse(const free_monoid<M>& xs) {
    return extend<M>([](const M& x) { return x; }, xs);
}

// =============================================================================
// Target monoids for demonstration
// =============================================================================

/// T under addition. Identity: 0.
template<typename T>
class additive {
    T value_;

public:
    additive() : value_(T{0}) {}
    explicit additive(T v) : value_(std::move(v)) {}

    T value() const { return value_; }

    bool operator==(const additive&) const = default;
};

template<typename T>
additive<T> op(const additive<T>& a, const additive<T>& b) {
    return additive<T>(a.value() + b.value());
}

template<typename T>
additive<T> identity(const additive<T>&) {
    return additive<T>{};
}

static_assert(Monoid<additive<int>>);
static_assert(Monoid<additive<double>>);

/// T under multiplication. Identity: 1.
template<typename T>
class multiplicative {
    T value_;

public:
    multiplicative() : value_(T{1}) {}
    explicit multiplicative(T v) : value_(std::move(v)) {}

    T value() const { return value_; }

    bool operator==(const multiplicative&) const = default;
};

template<typename T>
multiplicative<T> op(const multiplicative<T>& a, const multiplicative<T>& b) {
    return multiplicative<T>(a.value() * b.value());
}

template<typename T>
multiplicative<T> identity(const multiplicative<T>&) {
    return multiplicative<T>{};
}

static_assert(Monoid<multiplicative<int>>);

/// T under max. Identity: lowest value.
template<typename T>
class max_monoid {
    T value_;

public:
    max_monoid() : value_(std::numeric_limits<T>::lowest()) {}
    explicit max_monoid(T v) : value_(std::move(v)) {}

    T value() const { return value_; }

    bool operator==(const max_monoid&) const = default;
};

template<typename T>
max_monoid<T> op(const max_monoid<T>& a, const max_monoid<T>& b) {
    return max_monoid<T>(a.value() > b.value() ? a.value() : b.value());
}

template<typename T>
max_monoid<T> identity(const max_monoid<T>&) {
    return max_monoid<T>{};
}

static_assert(Monoid<max_monoid<int>>);

/// std::string under concatenation. Identity: "".
struct string_concat {
    std::string value;

    string_concat() : value{} {}
    explicit string_concat(std::string s) : value(std::move(s)) {}

    bool operator==(const string_concat&) const = default;
};

inline string_concat op(const string_concat& a, const string_concat& b) {
    return string_concat(a.value + b.value);
}

inline string_concat identity(const string_concat&) {
    return string_concat{};
}

static_assert(Monoid<string_concat>);

// =============================================================================
// The free commutative monoid: multisets
// =============================================================================

/// The free commutative monoid on T.
/// Elements are multisets: unordered collections with multiplicity.
/// Operation is multiset union (add counts). Identity is the empty multiset.
///
/// Unlike free_monoid, order does not matter: {a, b} == {b, a}.
/// But multiplicity does: {a, a} != {a}.
///
/// T must be ordered (for std::map). This is a representation choice,
/// not an algebraic requirement.
///
template<typename T>
class free_commutative_monoid {
    std::map<T, std::size_t> counts_;

public:
    using value_type = T;

    free_commutative_monoid() = default;

    explicit free_commutative_monoid(T x) {
        counts_[std::move(x)] = 1;
    }

    explicit free_commutative_monoid(std::map<T, std::size_t> c)
        : counts_(std::move(c)) {}

    /// Construct from a list of elements. Order is forgotten, counts are kept.
    free_commutative_monoid(std::initializer_list<T> xs) {
        for (const auto& x : xs)
            ++counts_[x];
    }

    const std::map<T, std::size_t>& counts() const { return counts_; }

    std::size_t count(const T& x) const {
        auto it = counts_.find(x);
        return it != counts_.end() ? it->second : 0;
    }

    std::size_t size() const {
        std::size_t n = 0;
        for (const auto& [_, c] : counts_) n += c;
        return n;
    }

    bool empty() const { return counts_.empty(); }

    bool operator==(const free_commutative_monoid&) const = default;
};

/// Monoid operation: multiset union (add counts).
template<typename T>
free_commutative_monoid<T> op(const free_commutative_monoid<T>& a,
                              const free_commutative_monoid<T>& b) {
    auto result = a.counts();
    for (const auto& [elem, count] : b.counts())
        result[elem] += count;
    return free_commutative_monoid<T>(std::move(result));
}

/// Monoid identity: the empty multiset.
template<typename T>
free_commutative_monoid<T> identity(const free_commutative_monoid<T>&) {
    return free_commutative_monoid<T>{};
}

static_assert(Monoid<free_commutative_monoid<int>>);
static_assert(Monoid<free_commutative_monoid<std::string>>);

} // namespace stepanov
