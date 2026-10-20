#pragma once

/**
 * @file accumulator.hpp
 * @brief Online statistics as monoids
 *
 * An accumulator processes a stream of values, maintaining state that
 * can be queried at any time. The key observation: accumulators form
 * monoids.
 *
 *     Identity:   default construction (empty accumulator)
 *     Operation:  combination via += (merge two accumulators' states)
 *
 * This is the same algebraic structure from peasant.hpp—but instead of
 * computing x^n, we compute streaming statistics. And the same monoid
 * structure enables the same kind of generic algorithm: fold().
 *
 * Parallel composition gives us the product monoid: compute many
 * statistics in a single pass, with arbitrary nesting.
 *
 * Reference: Stepanov & Rose, "From Mathematics to Generic Programming"
 */

#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>

namespace stepanov {

// =============================================================================
// The Accumulator concept
// =============================================================================

/// An Accumulator is a monoid that also processes individual values.
///
///   - Default construction = identity element
///   - operator+=(Acc) = associative binary operation (combine)
///   - operator+=(value_type) = process one observation
///   - eval() = extract current result
///
template<typename A>
concept Accumulator = std::semiregular<A> &&
    requires(A a, A b, typename A::value_type v) {
        typename A::value_type;
        { a += v } -> std::same_as<A&>;   // process one value
        { a += b } -> std::same_as<A&>;   // combine two accumulators
        { a.eval() };                       // extract result
    };

// =============================================================================
// kbn_sum: compensated summation
// =============================================================================

/// Kahan-Babuska-Neumaier summation.
///
/// Naive floating-point summation accumulates O(n) rounding error.
/// KBN tracks the lost low-order bits in a compensation term,
/// achieving O(1) error—the same accuracy regardless of how many
/// values you add.
///
/// Monoid: (kbn_sum, +=, kbn_sum{})
///   Identity = kbn_sum{} (sum=0, compensation=0)
///   Operation = combine sums (just add the totals)
///
template<std::floating_point T>
class kbn_sum {
    T sum_ = T(0);
    T comp_ = T(0);

    static constexpr T abs_(T x) { return x < T(0) ? -x : x; }

public:
    using value_type = T;

    constexpr kbn_sum() = default;
    constexpr explicit kbn_sum(T v) : sum_(v) {}

    constexpr kbn_sum& operator+=(const T& v) {
        T t = sum_ + v;
        // When |sum| >= |v|, the low bits of v are lost in t.
        // When |v| > |sum|, the low bits of sum are lost in t.
        // Either way, we recover them into the compensation term.
        comp_ += abs_(sum_) >= abs_(v) ? (sum_ - t) + v
                                       : (v - t) + sum_;
        sum_ = t;
        return *this;
    }

    constexpr kbn_sum& operator+=(const kbn_sum& other) {
        return *this += other.eval();
    }

    constexpr T eval() const { return sum_ + comp_; }
    constexpr explicit operator T() const { return eval(); }
};

static_assert(Accumulator<kbn_sum<double>>);
static_assert(Accumulator<kbn_sum<float>>);

// =============================================================================
// welford: online mean and variance
// =============================================================================

/// Welford's algorithm for streaming mean and variance.
///
/// Computes both statistics in a single pass without storing all
/// values. The combination formula merges two independent samples—
/// this is what makes parallel computation possible.
///
/// Monoid: (welford, +=, welford{})
///   Identity = welford{} (n=0, mean=0, m2=0)
///   Operation = parallel merge of two sample sets
///
template<std::floating_point T>
class welford {
    std::size_t n_ = 0;
    T mean_ = T(0);
    T m2_ = T(0);   // sum of squared deviations from mean

public:
    using value_type = T;

    welford() = default;

    welford& operator+=(const T& v) {
        ++n_;
        T delta = v - mean_;
        mean_ += delta / static_cast<T>(n_);
        T delta2 = v - mean_;   // note: uses *updated* mean
        m2_ += delta * delta2;
        return *this;
    }

    /// Parallel combination: merge two independent samples.
    /// This is Chan et al.'s formula for combining partial results.
    welford& operator+=(const welford& other) {
        if (other.n_ == 0) return *this;
        if (n_ == 0) { *this = other; return *this; }

        auto total = n_ + other.n_;
        T delta = other.mean_ - mean_;

        mean_ = (static_cast<T>(n_) * mean_
               + static_cast<T>(other.n_) * other.mean_)
              / static_cast<T>(total);

        m2_ += other.m2_
             + delta * delta * static_cast<T>(n_)
               * static_cast<T>(other.n_) / static_cast<T>(total);

        n_ = total;
        return *this;
    }

    T eval() const { return mean(); }
    T mean() const { return n_ > 0 ? mean_ : T(0); }
    T variance() const { return n_ > 0 ? m2_ / static_cast<T>(n_) : T(0); }
    T sample_variance() const {
        return n_ > 1 ? m2_ / static_cast<T>(n_ - 1) : T(0);
    }
    std::size_t size() const { return n_; }

    explicit operator T() const { return mean(); }
};

static_assert(Accumulator<welford<double>>);

// =============================================================================
// min_accumulator, max_accumulator, count_accumulator
// =============================================================================

/// Tracks the minimum value seen.
/// Monoid: (min_accumulator, +=, min_accumulator{})
///   Identity = infinity (no values seen)
///   Operation = take the smaller minimum
template<typename T>
class min_accumulator {
    T val_ = std::numeric_limits<T>::max();
    bool has_ = false;

public:
    using value_type = T;

    constexpr min_accumulator() = default;
    constexpr explicit min_accumulator(T v) : val_(v), has_(true) {}

    constexpr min_accumulator& operator+=(const T& v) {
        if (!has_ || v < val_) { val_ = v; has_ = true; }
        return *this;
    }

    constexpr min_accumulator& operator+=(const min_accumulator& other) {
        if (other.has_) *this += other.val_;
        return *this;
    }

    constexpr T eval() const { return val_; }
    constexpr explicit operator T() const { return eval(); }
    constexpr bool empty() const { return !has_; }
};

/// Tracks the maximum value seen.
/// Monoid: (max_accumulator, +=, max_accumulator{})
///   Identity = -infinity (no values seen)
///   Operation = take the larger maximum
template<typename T>
class max_accumulator {
    T val_ = std::numeric_limits<T>::lowest();
    bool has_ = false;

public:
    using value_type = T;

    constexpr max_accumulator() = default;
    constexpr explicit max_accumulator(T v) : val_(v), has_(true) {}

    constexpr max_accumulator& operator+=(const T& v) {
        if (!has_ || v > val_) { val_ = v; has_ = true; }
        return *this;
    }

    constexpr max_accumulator& operator+=(const max_accumulator& other) {
        if (other.has_) *this += other.val_;
        return *this;
    }

    constexpr T eval() const { return val_; }
    constexpr explicit operator T() const { return eval(); }
    constexpr bool empty() const { return !has_; }
};

/// Counts values processed.
/// Monoid: (count_accumulator, +=, count_accumulator{})
///   Identity = 0
///   Operation = add counts
///
/// Parameterised on T so it shares the value_type of its siblings
/// in a parallel composition.
template<typename T>
class count_accumulator {
    std::size_t n_ = 0;

public:
    using value_type = T;

    constexpr count_accumulator() = default;

    constexpr count_accumulator& operator+=(const T&) {
        ++n_;
        return *this;
    }

    constexpr count_accumulator& operator+=(const count_accumulator& other) {
        n_ += other.n_;
        return *this;
    }

    constexpr std::size_t eval() const { return n_; }
    constexpr explicit operator std::size_t() const { return n_; }
};

static_assert(Accumulator<min_accumulator<double>>);
static_assert(Accumulator<min_accumulator<int>>);
static_assert(Accumulator<max_accumulator<double>>);
static_assert(Accumulator<count_accumulator<double>>);

// =============================================================================
// Parallel composition: the product monoid
// =============================================================================

/// If A and B are accumulators over the same value type, then
/// parallel<A, B> is also an accumulator: both process the same
/// data stream, each maintaining its own state independently.
///
/// This is the product monoid—and it nests:
///   parallel<parallel<A, B>, C>
/// computes three statistics in one pass. The algebraic structure
/// composes without limit.
///
template<Accumulator A, Accumulator B>
    requires std::same_as<typename A::value_type, typename B::value_type>
class parallel {
    A a_;
    B b_;

public:
    using value_type = typename A::value_type;

    parallel() = default;
    parallel(A a, B b) : a_(std::move(a)), b_(std::move(b)) {}

    parallel& operator+=(const value_type& v) {
        a_ += v;
        b_ += v;
        return *this;
    }

    parallel& operator+=(const parallel& other) {
        a_ += other.a_;
        b_ += other.b_;
        return *this;
    }

    auto eval() const { return std::make_tuple(a_.eval(), b_.eval()); }

    const A& first() const { return a_; }
    const B& second() const { return b_; }
};

static_assert(Accumulator<parallel<kbn_sum<double>, min_accumulator<double>>>);

/// Convenience: kbn_sum<double>{} | welford<double>{} creates a parallel.
template<Accumulator A, Accumulator B>
    requires std::same_as<typename A::value_type, typename B::value_type>
auto operator|(A a, B b) {
    return parallel<A, B>(std::move(a), std::move(b));
}

// =============================================================================
// fold: the generic algorithm that monoid structure enables
// =============================================================================

/// Process a range of values through an accumulator.
///
/// This is the algorithm that monoid structure makes possible—
/// the same pattern as std::accumulate, but the accumulator's
/// algebraic properties (identity, associativity) mean we could
/// split the range, fold each piece independently, and combine
/// the results. That's parallelism for free.
///
template<Accumulator Acc, typename It>
Acc fold(It first, It last) {
    Acc acc{};    // identity element
    for (; first != last; ++first)
        acc += *first;
    return acc;
}

} // namespace stepanov
