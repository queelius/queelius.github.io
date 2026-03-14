#pragma once

/**
 * @file lattice.hpp
 * @brief Lattices, fixed-point iteration, and abstract interpretation
 *
 * A lattice is a partially ordered set where every two elements have
 * a greatest lower bound (meet) and a least upper bound (join). This
 * is the algebraic structure behind abstract interpretation, type
 * inference, and dataflow analysis.
 *
 * The key algorithm: Tarski's fixed-point theorem. Any monotone
 * function on a complete lattice has a least fixed point, computable
 * by iterating from bottom. Lattice structure determines the
 * algorithm, just as monoid structure determines power-by-squaring.
 *
 * To adapt your own type, provide meet(), join(), bottom(), and top()
 * as free functions findable via ADL.
 *
 * Reference: Davey & Priestley, "Introduction to Lattices and Order"
 */

#include <algorithm>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>

namespace stepanov {

// =============================================================================
// Lattice concepts
// =============================================================================

/// A lattice: a set with meet (greatest lower bound) and join (least
/// upper bound). Both operations are idempotent, commutative, and
/// associative, and they satisfy the absorption laws.
template<typename L>
concept Lattice = std::semiregular<L> &&
    requires(L a, L b) {
        { meet(a, b) } -> std::convertible_to<L>;  // greatest lower bound
        { join(a, b) } -> std::convertible_to<L>;   // least upper bound
    };

/// A bounded lattice adds a least element (bottom) and a greatest
/// element (top). Bottom is the identity for join, top is the
/// identity for meet.
template<typename L>
concept BoundedLattice = Lattice<L> &&
    requires(L a) {
        { bottom(a) } -> std::convertible_to<L>;    // least element
        { top(a) } -> std::convertible_to<L>;        // greatest element
    };

// =============================================================================
// sign_lattice: abstract sign domain
// =============================================================================

/// The sign lattice: abstract interpretation's classic example.
///
/// Hasse diagram:
///
///        top          (unknown: could be anything)
///       / | \
///    neg zero pos     (known sign)
///       \ | /
///       bottom        (unreachable / no information)
///
/// This is a bounded lattice with five elements. Meet is greatest
/// lower bound, join is least upper bound in the diagram above.
///
enum class sign_lattice : std::uint8_t {
    bot,       // bottom: unreachable
    negative,
    zero,
    positive,
    top        // top: unknown sign
};

constexpr sign_lattice bottom(sign_lattice) { return sign_lattice::bot; }
constexpr sign_lattice top(sign_lattice)    { return sign_lattice::top; }

/// Meet: greatest lower bound.
/// Two elements have a common lower bound only if one is below the
/// other, or both are the same. Otherwise the meet is bottom.
constexpr sign_lattice meet(sign_lattice a, sign_lattice b) {
    if (a == b) return a;
    if (a == sign_lattice::top) return b;
    if (b == sign_lattice::top) return a;
    if (a == sign_lattice::bot) return sign_lattice::bot;
    if (b == sign_lattice::bot) return sign_lattice::bot;
    // Two distinct non-top, non-bottom elements: incomparable
    return sign_lattice::bot;
}

/// Join: least upper bound.
/// Dual of meet.
constexpr sign_lattice join(sign_lattice a, sign_lattice b) {
    if (a == b) return a;
    if (a == sign_lattice::bot) return b;
    if (b == sign_lattice::bot) return a;
    if (a == sign_lattice::top) return sign_lattice::top;
    if (b == sign_lattice::top) return sign_lattice::top;
    // Two distinct non-top, non-bottom elements: incomparable
    return sign_lattice::top;
}

/// Abstract addition on the sign domain.
constexpr sign_lattice abstract_add(sign_lattice a, sign_lattice b) {
    if (a == sign_lattice::bot || b == sign_lattice::bot)
        return sign_lattice::bot;
    if (a == sign_lattice::top || b == sign_lattice::top)
        return sign_lattice::top;
    if (a == sign_lattice::zero) return b;
    if (b == sign_lattice::zero) return a;
    if (a == b) return a;  // neg+neg=neg, pos+pos=pos
    // neg+pos or pos+neg: could be anything
    return sign_lattice::top;
}

/// Abstract multiplication on the sign domain.
constexpr sign_lattice abstract_mul(sign_lattice a, sign_lattice b) {
    if (a == sign_lattice::bot || b == sign_lattice::bot)
        return sign_lattice::bot;
    if (a == sign_lattice::zero || b == sign_lattice::zero)
        return sign_lattice::zero;
    if (a == sign_lattice::top || b == sign_lattice::top)
        return sign_lattice::top;
    // Both are negative or positive
    return (a == b) ? sign_lattice::positive : sign_lattice::negative;
}

static_assert(BoundedLattice<sign_lattice>);

// =============================================================================
// interval<T>: closed intervals ordered by inclusion
// =============================================================================

/// Closed interval [lo, hi]. The lattice ordering is inclusion:
///   [a,b] <= [c,d]  iff  c <= a and b <= d
///
/// Meet is intersection, join is the smallest enclosing interval.
/// The empty interval is bottom, the full range is top.
///
template<typename T>
    requires std::totally_ordered<T>
class interval {
    T lo_{};
    T hi_{};
    bool empty_ = true;

public:
    constexpr interval() = default;
    constexpr interval(T lo, T hi)
        : lo_(lo <= hi ? lo : T{}), hi_(lo <= hi ? hi : T{}),
          empty_(lo > hi) {}

    constexpr bool is_empty() const { return empty_; }
    constexpr T lo() const { return lo_; }
    constexpr T hi() const { return hi_; }

    constexpr bool operator==(const interval&) const = default;

    /// Does this interval contain value x?
    constexpr bool contains(T x) const {
        return !empty_ && lo_ <= x && x <= hi_;
    }
};

/// Bottom: empty interval.
template<typename T>
constexpr interval<T> bottom(interval<T>) { return interval<T>{}; }

/// Top: the entire range of T.
template<typename T>
constexpr interval<T> top(interval<T>) {
    return interval<T>(std::numeric_limits<T>::lowest(),
                       std::numeric_limits<T>::max());
}

/// Meet: intersection. Empty if disjoint.
template<typename T>
constexpr interval<T> meet(interval<T> a, interval<T> b) {
    if (a.is_empty() || b.is_empty()) return interval<T>{};
    T lo = std::max(a.lo(), b.lo());
    T hi = std::min(a.hi(), b.hi());
    if (lo > hi) return interval<T>{};
    return interval<T>(lo, hi);
}

/// Join: smallest enclosing interval.
template<typename T>
constexpr interval<T> join(interval<T> a, interval<T> b) {
    if (a.is_empty()) return b;
    if (b.is_empty()) return a;
    return interval<T>(std::min(a.lo(), b.lo()),
                       std::max(a.hi(), b.hi()));
}

static_assert(BoundedLattice<interval<int>>);
static_assert(BoundedLattice<interval<double>>);

// =============================================================================
// divisor_lattice: positive integers ordered by divisibility
// =============================================================================

/// Positive integers under divisibility ordering.
///
///   a <= b  iff  a divides b
///   meet = gcd (greatest common divisor)
///   join = lcm (least common multiple)
///   bottom = 1 (divides everything)
///   top = 0 (everything divides 0)
///
/// This is a classic mathematical example: lattice structure appears
/// naturally in number theory.
///
class divisor_lattice {
    unsigned val_ = 1;

public:
    constexpr divisor_lattice() = default;
    constexpr explicit divisor_lattice(unsigned v) : val_(v) {}

    constexpr unsigned value() const { return val_; }
    constexpr bool operator==(const divisor_lattice&) const = default;
};

constexpr divisor_lattice bottom(divisor_lattice) { return divisor_lattice{1}; }
constexpr divisor_lattice top(divisor_lattice)    { return divisor_lattice{0}; }

constexpr divisor_lattice meet(divisor_lattice a, divisor_lattice b) {
    return divisor_lattice{std::gcd(a.value(), b.value())};
}

constexpr divisor_lattice join(divisor_lattice a, divisor_lattice b) {
    if (a.value() == 0) return divisor_lattice{0};
    if (b.value() == 0) return divisor_lattice{0};
    return divisor_lattice{std::lcm(a.value(), b.value())};
}

static_assert(BoundedLattice<divisor_lattice>);

// =============================================================================
// powerset<N>: subsets of {0, ..., N-1} as bitsets
// =============================================================================

/// Subsets of {0, ..., N-1} ordered by inclusion.
///
///   meet = intersection (bitwise AND)
///   join = union (bitwise OR)
///   bottom = empty set
///   top = full set
///
/// Uses uint64_t as the backing store, so N <= 64.
///
template<std::size_t N>
    requires (N <= 64)
class powerset {
    std::uint64_t bits_ = 0;

public:
    constexpr powerset() = default;
    constexpr explicit powerset(std::uint64_t bits)
        : bits_(bits & mask()) {}

    constexpr std::uint64_t bits() const { return bits_; }
    constexpr bool operator==(const powerset&) const = default;

    /// Test membership of element i.
    constexpr bool contains(std::size_t i) const {
        return i < N && (bits_ & (std::uint64_t{1} << i)) != 0;
    }

    /// Number of elements in the set.
    constexpr std::size_t size() const {
        return static_cast<std::size_t>(__builtin_popcountll(bits_));
    }

    /// Insert element i.
    constexpr powerset& insert(std::size_t i) {
        if (i < N) bits_ |= (std::uint64_t{1} << i);
        return *this;
    }

private:
    static constexpr std::uint64_t mask() {
        if constexpr (N == 64) return ~std::uint64_t{0};
        else return (std::uint64_t{1} << N) - 1;
    }
};

template<std::size_t N>
constexpr powerset<N> bottom(powerset<N>) { return powerset<N>{0}; }

template<std::size_t N>
constexpr powerset<N> top(powerset<N>) {
    if constexpr (N == 64) return powerset<N>{~std::uint64_t{0}};
    else return powerset<N>{(std::uint64_t{1} << N) - 1};
}

template<std::size_t N>
constexpr powerset<N> meet(powerset<N> a, powerset<N> b) {
    return powerset<N>{a.bits() & b.bits()};
}

template<std::size_t N>
constexpr powerset<N> join(powerset<N> a, powerset<N> b) {
    return powerset<N>{a.bits() | b.bits()};
}

static_assert(BoundedLattice<powerset<8>>);
static_assert(BoundedLattice<powerset<64>>);

// =============================================================================
// Fixed-point iteration
// =============================================================================

/// Compute the least fixed point of a monotone function.
///
/// Tarski's theorem: if f is monotone on a complete lattice,
/// iterating f from bottom converges to the least fixed point.
/// We use Kleene iteration: x_{n+1} = join(x_n, f(x_n)), which
/// ascends the lattice until convergence.
///
/// For finite lattices, this always terminates. The max_iter
/// parameter guards against infinite ascending chains (e.g.,
/// when the lattice is not finite).
///
template<BoundedLattice L, typename F>
    requires std::invocable<F, L> &&
             std::convertible_to<std::invoke_result_t<F, L>, L>
L least_fixed_point(F f, std::size_t max_iter = 1000) {
    L x = bottom(L{});
    for (std::size_t i = 0; i < max_iter; ++i) {
        L next = join(x, f(x));
        if (next == x) return x;
        x = next;
    }
    return x;  // max iterations reached
}

/// Compute the greatest fixed point of a monotone function.
///
/// Dual of least_fixed_point: iterate from top, descending via meet.
///
template<BoundedLattice L, typename F>
    requires std::invocable<F, L> &&
             std::convertible_to<std::invoke_result_t<F, L>, L>
L greatest_fixed_point(F f, std::size_t max_iter = 1000) {
    L x = top(L{});
    for (std::size_t i = 0; i < max_iter; ++i) {
        L next = meet(x, f(x));
        if (next == x) return x;
        x = next;
    }
    return x;  // max iterations reached
}

// =============================================================================
// Abstract interpretation: sign analysis
// =============================================================================

/// Abstract evaluation of (x * y + z) in the sign domain.
///
/// This is a transfer function: it computes the abstract sign of an
/// expression given the abstract signs of its inputs. The same
/// structure works for any lattice-based abstract domain.
///
constexpr sign_lattice abstract_eval_mul_add(sign_lattice x,
                                             sign_lattice y,
                                             sign_lattice z) {
    return abstract_add(abstract_mul(x, y), z);
}

} // namespace stepanov
