#pragma once

/**
 * @file semiring.hpp
 * @brief Semirings and graph algorithms via matrix power
 *
 * A semiring (S, +, x, 0, 1) has two monoidal operations linked by
 * distributivity. The key observation: matrix multiplication over a
 * semiring is well-defined. Different semirings give different graph
 * algorithms from the SAME matrix power code.
 *
 *     Boolean semiring    -> reachability
 *     Tropical min        -> shortest paths
 *     Tropical max        -> longest paths
 *     Bottleneck          -> widest paths
 *     Counting            -> number of paths
 *
 * The power algorithm is the same one from peasant.hpp, applied to
 * matrices over a richer algebraic structure.
 *
 * Reference: Stepanov & Rose, "From Mathematics to Generic Programming"
 */

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <tuple>

namespace stepanov {

// =============================================================================
// The Semiring concept
// =============================================================================

/// A semiring has two monoidal operations (+, *) with identities (zero, one),
/// where * distributes over + and zero annihilates under *.
///
/// ADL free functions zero(x) and one(x) provide the identities,
/// matching the pattern from peasant.hpp.
///
template<typename S>
concept Semiring = std::semiregular<S> &&
    requires(S a, S b) {
        { a + b } -> std::convertible_to<S>;   // additive monoid
        { a * b } -> std::convertible_to<S>;   // multiplicative monoid
        { zero(a) } -> std::convertible_to<S>; // additive identity
        { one(a) } -> std::convertible_to<S>;  // multiplicative identity
    };

// =============================================================================
// Concrete semirings
// =============================================================================

// -----------------------------------------------------------------------------
// boolean_semiring: + is OR, * is AND, zero=false, one=true
// For reachability queries: "is there a path?"
// -----------------------------------------------------------------------------

struct boolean_semiring {
    bool val;

    constexpr boolean_semiring() : val(false) {}
    constexpr explicit boolean_semiring(bool v) : val(v) {}

    constexpr boolean_semiring operator+(boolean_semiring rhs) const {
        return boolean_semiring(val || rhs.val);
    }

    constexpr boolean_semiring operator*(boolean_semiring rhs) const {
        return boolean_semiring(val && rhs.val);
    }

    constexpr bool operator==(const boolean_semiring& rhs) const = default;
};

constexpr boolean_semiring zero(boolean_semiring) { return boolean_semiring(false); }
constexpr boolean_semiring one(boolean_semiring)  { return boolean_semiring(true); }

static_assert(Semiring<boolean_semiring>);

// -----------------------------------------------------------------------------
// tropical_min<T>: + is min, * is plus, zero=infinity, one=0
// For shortest path queries.
//
// This naming is counterintuitive but correct: the "addition" of the
// semiring is min (we pick the shorter path), and the "multiplication"
// is ordinary addition (path lengths compose by summing).
// -----------------------------------------------------------------------------

template<typename T>
struct tropical_min {
    T val;

    constexpr tropical_min() : val(std::numeric_limits<T>::infinity()) {}
    constexpr explicit tropical_min(T v) : val(v) {}

    constexpr tropical_min operator+(tropical_min rhs) const {
        return tropical_min(std::min(val, rhs.val));   // semiring "add" = min
    }

    constexpr tropical_min operator*(tropical_min rhs) const {
        // If either operand is infinity (the zero element), return infinity.
        // This enforces the annihilation axiom: zero * a = a * zero = zero.
        if (val == std::numeric_limits<T>::infinity() ||
            rhs.val == std::numeric_limits<T>::infinity())
            return tropical_min();   // zero
        return tropical_min(val + rhs.val);   // semiring "multiply" = add
    }

    constexpr bool operator==(const tropical_min& rhs) const = default;
};

template<typename T>
constexpr tropical_min<T> zero(tropical_min<T>) {
    return tropical_min<T>();   // infinity
}

template<typename T>
constexpr tropical_min<T> one(tropical_min<T>) {
    return tropical_min<T>(T(0));   // additive identity for the "multiply" = plus
}

static_assert(Semiring<tropical_min<double>>);

// -----------------------------------------------------------------------------
// tropical_max<T>: + is max, * is plus, zero=-infinity, one=0
// For longest/critical path queries.
// -----------------------------------------------------------------------------

template<typename T>
struct tropical_max {
    T val;

    constexpr tropical_max() : val(-std::numeric_limits<T>::infinity()) {}
    constexpr explicit tropical_max(T v) : val(v) {}

    constexpr tropical_max operator+(tropical_max rhs) const {
        return tropical_max(std::max(val, rhs.val));   // semiring "add" = max
    }

    constexpr tropical_max operator*(tropical_max rhs) const {
        if (val == -std::numeric_limits<T>::infinity() ||
            rhs.val == -std::numeric_limits<T>::infinity())
            return tropical_max();   // zero
        return tropical_max(val + rhs.val);   // semiring "multiply" = add
    }

    constexpr bool operator==(const tropical_max& rhs) const = default;
};

template<typename T>
constexpr tropical_max<T> zero(tropical_max<T>) {
    return tropical_max<T>();   // -infinity
}

template<typename T>
constexpr tropical_max<T> one(tropical_max<T>) {
    return tropical_max<T>(T(0));
}

static_assert(Semiring<tropical_max<double>>);

// -----------------------------------------------------------------------------
// bottleneck<T>: + is max, * is min, zero=-infinity, one=+infinity
// For widest/bottleneck path queries: the path capacity is the minimum
// edge capacity along the path, and we want the maximum such capacity.
// -----------------------------------------------------------------------------

template<typename T>
struct bottleneck {
    T val;

    constexpr bottleneck() : val(-std::numeric_limits<T>::infinity()) {}
    constexpr explicit bottleneck(T v) : val(v) {}

    constexpr bottleneck operator+(bottleneck rhs) const {
        return bottleneck(std::max(val, rhs.val));   // semiring "add" = max
    }

    constexpr bottleneck operator*(bottleneck rhs) const {
        return bottleneck(std::min(val, rhs.val));   // semiring "multiply" = min
    }

    constexpr bool operator==(const bottleneck& rhs) const = default;
};

template<typename T>
constexpr bottleneck<T> zero(bottleneck<T>) {
    return bottleneck<T>();   // -infinity
}

template<typename T>
constexpr bottleneck<T> one(bottleneck<T>) {
    return bottleneck<T>(std::numeric_limits<T>::infinity());   // +infinity
}

static_assert(Semiring<bottleneck<double>>);

// -----------------------------------------------------------------------------
// counting: + is addition, * is multiplication, zero=0, one=1
// Regular integer arithmetic. For counting the number of paths.
// -----------------------------------------------------------------------------

struct counting {
    int val;

    constexpr counting() : val(0) {}
    constexpr explicit counting(int v) : val(v) {}

    constexpr counting operator+(counting rhs) const {
        return counting(val + rhs.val);
    }

    constexpr counting operator*(counting rhs) const {
        return counting(val * rhs.val);
    }

    constexpr bool operator==(const counting& rhs) const = default;
};

constexpr counting zero(counting) { return counting(0); }
constexpr counting one(counting)  { return counting(1); }

static_assert(Semiring<counting>);

// =============================================================================
// Matrix over a semiring
// =============================================================================

/// Fixed-size square matrix parameterized on a Semiring type.
///
/// All entries default to zero(S{}). Matrix addition is element-wise
/// semiring addition. Matrix multiplication uses semiring operations.
/// This is the standard construction: if S is a semiring, then
/// Mat_n(S) is also a semiring.
///
template<Semiring S, std::size_t N>
class matrix {
    S data_[N][N];

public:
    constexpr matrix() {
        S z = zero(S{});
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                data_[i][j] = z;
    }

    static constexpr matrix identity() {
        matrix m;
        S id = one(S{});
        for (std::size_t i = 0; i < N; ++i)
            m.data_[i][i] = id;
        return m;
    }

    constexpr S& operator()(std::size_t i, std::size_t j) { return data_[i][j]; }
    constexpr const S& operator()(std::size_t i, std::size_t j) const { return data_[i][j]; }

    constexpr matrix operator+(const matrix& rhs) const {
        matrix result;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                result(i, j) = data_[i][j] + rhs(i, j);
        return result;
    }

    constexpr matrix operator*(const matrix& rhs) const {
        matrix result;
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t k = 0; k < N; ++k) {
                if (data_[i][k] == zero(S{})) continue;
                for (std::size_t j = 0; j < N; ++j)
                    result(i, j) = result(i, j) + data_[i][k] * rhs(k, j);
            }
        return result;
    }

    constexpr bool operator==(const matrix& rhs) const {
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = 0; j < N; ++j)
                if (!(data_[i][j] == rhs(i, j))) return false;
        return true;
    }
};

// =============================================================================
// Power: repeated squaring for matrices over any semiring
// =============================================================================

/// Compute base^exp by repeated squaring.
///
/// This is the same algorithm as peasant::power(), applied to matrices
/// over a semiring. For an adjacency matrix A, A^k(i,j) gives the
/// k-hop answer in the semiring's domain:
///   - boolean:  is there a path of length exactly k?
///   - tropical: shortest path using exactly k edges?
///   - counting: how many paths of length exactly k?
///
template<Semiring S, std::size_t N>
constexpr matrix<S, N> power(matrix<S, N> base, std::size_t exp) {
    auto result = matrix<S, N>::identity();
    while (exp > 0) {
        if (exp & 1) result = result * base;
        base = base * base;
        exp >>= 1;
    }
    return result;
}

// =============================================================================
// Closure: A* = I + A + A^2 + ... + A^(N-1)
// =============================================================================

/// Compute the transitive closure sum: I + A + A^2 + ... + A^(max_hops).
///
/// For reachability, this answers "is there a path of any length up to
/// max_hops?" For shortest paths, it finds the shortest path using at
/// most max_hops edges. With max_hops = N-1 (the default), this covers
/// all simple paths in an N-node graph.
///
template<Semiring S, std::size_t N>
constexpr matrix<S, N> closure(const matrix<S, N>& adj, std::size_t max_hops = N - 1) {
    auto result = matrix<S, N>::identity();
    auto ak = matrix<S, N>::identity();
    for (std::size_t k = 1; k <= max_hops; ++k) {
        ak = ak * adj;
        result = result + ak;
    }
    return result;
}

// =============================================================================
// Convenience: build adjacency matrix from edge list
// =============================================================================

/// Construct an adjacency matrix from a list of (from, to, weight) edges.
///
template<Semiring S, std::size_t N>
constexpr matrix<S, N> adjacency(
    std::initializer_list<std::tuple<std::size_t, std::size_t, S>> edges)
{
    matrix<S, N> m;
    for (auto& [i, j, w] : edges)
        m(i, j) = w;
    return m;
}

} // namespace stepanov
