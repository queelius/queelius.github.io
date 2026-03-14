#include <gtest/gtest.h>
#include "lattice.hpp"

using namespace stepanov;

// =============================================================================
// Concept Compliance
// =============================================================================

TEST(LatticeTest, ConceptsSatisfied) {
    static_assert(Lattice<sign_lattice>);
    static_assert(BoundedLattice<sign_lattice>);

    static_assert(Lattice<interval<int>>);
    static_assert(BoundedLattice<interval<int>>);
    static_assert(BoundedLattice<interval<double>>);

    static_assert(Lattice<divisor_lattice>);
    static_assert(BoundedLattice<divisor_lattice>);

    static_assert(Lattice<powerset<8>>);
    static_assert(BoundedLattice<powerset<8>>);
    static_assert(BoundedLattice<powerset<64>>);
}

// =============================================================================
// Lattice Laws: sign_lattice
// =============================================================================

class SignLatticeTest : public ::testing::Test {
protected:
    static constexpr sign_lattice all[] = {
        sign_lattice::bot, sign_lattice::negative,
        sign_lattice::zero, sign_lattice::positive,
        sign_lattice::top
    };
};

TEST_F(SignLatticeTest, Idempotent) {
    for (auto a : all) {
        EXPECT_EQ(meet(a, a), a);
        EXPECT_EQ(join(a, a), a);
    }
}

TEST_F(SignLatticeTest, Commutative) {
    for (auto a : all)
        for (auto b : all) {
            EXPECT_EQ(meet(a, b), meet(b, a));
            EXPECT_EQ(join(a, b), join(b, a));
        }
}

TEST_F(SignLatticeTest, Associative) {
    for (auto a : all)
        for (auto b : all)
            for (auto c : all) {
                EXPECT_EQ(meet(meet(a, b), c), meet(a, meet(b, c)));
                EXPECT_EQ(join(join(a, b), c), join(a, join(b, c)));
            }
}

TEST_F(SignLatticeTest, Absorption) {
    for (auto a : all)
        for (auto b : all) {
            EXPECT_EQ(meet(a, join(a, b)), a);
            EXPECT_EQ(join(a, meet(a, b)), a);
        }
}

TEST_F(SignLatticeTest, BottomTopIdentity) {
    for (auto a : all) {
        EXPECT_EQ(join(a, bottom(a)), a);
        EXPECT_EQ(meet(a, top(a)), a);
    }
}

// =============================================================================
// Sign Lattice: meet/join truth tables
// =============================================================================

TEST_F(SignLatticeTest, MeetTruthTable) {
    using S = sign_lattice;
    // meet with top returns the other element
    EXPECT_EQ(meet(S::top, S::negative), S::negative);
    EXPECT_EQ(meet(S::top, S::zero), S::zero);
    EXPECT_EQ(meet(S::top, S::positive), S::positive);
    // meet with bottom returns bottom
    EXPECT_EQ(meet(S::bot, S::negative), S::bot);
    EXPECT_EQ(meet(S::bot, S::top), S::bot);
    // meet of incomparable elements returns bottom
    EXPECT_EQ(meet(S::negative, S::zero), S::bot);
    EXPECT_EQ(meet(S::negative, S::positive), S::bot);
    EXPECT_EQ(meet(S::zero, S::positive), S::bot);
}

TEST_F(SignLatticeTest, JoinTruthTable) {
    using S = sign_lattice;
    // join with bottom returns the other element
    EXPECT_EQ(join(S::bot, S::negative), S::negative);
    EXPECT_EQ(join(S::bot, S::zero), S::zero);
    EXPECT_EQ(join(S::bot, S::positive), S::positive);
    // join with top returns top
    EXPECT_EQ(join(S::top, S::negative), S::top);
    // join of incomparable elements returns top
    EXPECT_EQ(join(S::negative, S::zero), S::top);
    EXPECT_EQ(join(S::negative, S::positive), S::top);
    EXPECT_EQ(join(S::zero, S::positive), S::top);
}

// =============================================================================
// Sign Lattice: abstract arithmetic
// =============================================================================

TEST_F(SignLatticeTest, AbstractAdd) {
    using S = sign_lattice;
    EXPECT_EQ(abstract_add(S::positive, S::positive), S::positive);
    EXPECT_EQ(abstract_add(S::negative, S::negative), S::negative);
    EXPECT_EQ(abstract_add(S::positive, S::zero), S::positive);
    EXPECT_EQ(abstract_add(S::zero, S::negative), S::negative);
    EXPECT_EQ(abstract_add(S::zero, S::zero), S::zero);
    // pos + neg could be anything
    EXPECT_EQ(abstract_add(S::positive, S::negative), S::top);
    EXPECT_EQ(abstract_add(S::negative, S::positive), S::top);
    // anything with bot is bot (unreachable)
    EXPECT_EQ(abstract_add(S::bot, S::positive), S::bot);
    // anything with top is top (unknown)
    EXPECT_EQ(abstract_add(S::top, S::zero), S::top);
}

TEST_F(SignLatticeTest, AbstractMul) {
    using S = sign_lattice;
    EXPECT_EQ(abstract_mul(S::positive, S::positive), S::positive);
    EXPECT_EQ(abstract_mul(S::negative, S::negative), S::positive);
    EXPECT_EQ(abstract_mul(S::positive, S::negative), S::negative);
    EXPECT_EQ(abstract_mul(S::negative, S::positive), S::negative);
    // zero absorbs
    EXPECT_EQ(abstract_mul(S::zero, S::positive), S::zero);
    EXPECT_EQ(abstract_mul(S::negative, S::zero), S::zero);
    EXPECT_EQ(abstract_mul(S::zero, S::zero), S::zero);
    // bot propagates
    EXPECT_EQ(abstract_mul(S::bot, S::positive), S::bot);
    // top propagates (when non-zero)
    EXPECT_EQ(abstract_mul(S::top, S::positive), S::top);
}

TEST_F(SignLatticeTest, AbstractEvalMulAdd) {
    using S = sign_lattice;
    // pos * pos + zero = pos
    EXPECT_EQ(abstract_eval_mul_add(S::positive, S::positive, S::zero),
              S::positive);
    // neg * neg + pos = pos + pos = pos
    EXPECT_EQ(abstract_eval_mul_add(S::negative, S::negative, S::positive),
              S::positive);
    // pos * neg + pos = neg + pos = top
    EXPECT_EQ(abstract_eval_mul_add(S::positive, S::negative, S::positive),
              S::top);
}

// =============================================================================
// Interval Lattice
// =============================================================================

TEST(IntervalTest, Construction) {
    interval<int> empty;
    EXPECT_TRUE(empty.is_empty());

    interval<int> a(1, 5);
    EXPECT_FALSE(a.is_empty());
    EXPECT_EQ(a.lo(), 1);
    EXPECT_EQ(a.hi(), 5);

    // Inverted bounds produce empty interval
    interval<int> bad(5, 1);
    EXPECT_TRUE(bad.is_empty());
}

TEST(IntervalTest, Contains) {
    interval<int> a(2, 8);
    EXPECT_TRUE(a.contains(2));
    EXPECT_TRUE(a.contains(5));
    EXPECT_TRUE(a.contains(8));
    EXPECT_FALSE(a.contains(1));
    EXPECT_FALSE(a.contains(9));

    interval<int> empty;
    EXPECT_FALSE(empty.contains(0));
}

TEST(IntervalTest, MeetIntersection) {
    interval<int> a(1, 5);
    interval<int> b(3, 8);
    auto m = meet(a, b);
    EXPECT_EQ(m, interval<int>(3, 5));

    // Disjoint intervals
    interval<int> c(1, 3);
    interval<int> d(5, 8);
    EXPECT_TRUE(meet(c, d).is_empty());

    // Meet with empty
    interval<int> empty;
    EXPECT_TRUE(meet(a, empty).is_empty());
}

TEST(IntervalTest, JoinEnclosing) {
    interval<int> a(1, 5);
    interval<int> b(3, 8);
    auto j = join(a, b);
    EXPECT_EQ(j, interval<int>(1, 8));

    // Join with empty
    interval<int> empty;
    EXPECT_EQ(join(a, empty), a);
    EXPECT_EQ(join(empty, a), a);
}

TEST(IntervalTest, Idempotent) {
    interval<int> a(2, 7);
    EXPECT_EQ(meet(a, a), a);
    EXPECT_EQ(join(a, a), a);
}

TEST(IntervalTest, Commutative) {
    interval<int> a(1, 5);
    interval<int> b(3, 8);
    EXPECT_EQ(meet(a, b), meet(b, a));
    EXPECT_EQ(join(a, b), join(b, a));
}

TEST(IntervalTest, Absorption) {
    interval<int> a(1, 5);
    interval<int> b(3, 8);
    EXPECT_EQ(meet(a, join(a, b)), a);
    EXPECT_EQ(join(a, meet(a, b)), a);
}

TEST(IntervalTest, BottomTopIdentity) {
    interval<int> a(2, 7);
    interval<int> bot = bottom(a);
    interval<int> t = top(a);

    EXPECT_EQ(join(a, bot), a);
    EXPECT_EQ(meet(a, t), a);
}

// =============================================================================
// Divisor Lattice
// =============================================================================

TEST(DivisorTest, MeetIsGcd) {
    EXPECT_EQ(meet(divisor_lattice{12}, divisor_lattice{8}).value(), 4u);
    EXPECT_EQ(meet(divisor_lattice{15}, divisor_lattice{10}).value(), 5u);
    EXPECT_EQ(meet(divisor_lattice{7}, divisor_lattice{13}).value(), 1u);
}

TEST(DivisorTest, JoinIsLcm) {
    EXPECT_EQ(join(divisor_lattice{4}, divisor_lattice{6}).value(), 12u);
    EXPECT_EQ(join(divisor_lattice{3}, divisor_lattice{5}).value(), 15u);
    EXPECT_EQ(join(divisor_lattice{12}, divisor_lattice{18}).value(), 36u);
}

TEST(DivisorTest, BottomIsOne) {
    divisor_lattice bot = bottom(divisor_lattice{});
    EXPECT_EQ(bot.value(), 1u);
    // 1 divides everything, so meet(1, x) = 1 and join(1, x) = x
    EXPECT_EQ(meet(bot, divisor_lattice{12}).value(), 1u);
    EXPECT_EQ(join(bot, divisor_lattice{12}).value(), 12u);
}

TEST(DivisorTest, TopIsZero) {
    divisor_lattice t = top(divisor_lattice{});
    EXPECT_EQ(t.value(), 0u);
    // Everything divides 0, so meet(0, x) = x and join(0, x) = 0
    EXPECT_EQ(meet(t, divisor_lattice{12}).value(), 12u);
    EXPECT_EQ(join(t, divisor_lattice{12}).value(), 0u);
}

TEST(DivisorTest, Idempotent) {
    divisor_lattice a{12};
    EXPECT_EQ(meet(a, a), a);
    EXPECT_EQ(join(a, a), a);
}

TEST(DivisorTest, Commutative) {
    divisor_lattice a{12}, b{18};
    EXPECT_EQ(meet(a, b), meet(b, a));
    EXPECT_EQ(join(a, b), join(b, a));
}

TEST(DivisorTest, Associative) {
    divisor_lattice a{12}, b{18}, c{8};
    EXPECT_EQ(meet(meet(a, b), c), meet(a, meet(b, c)));
    EXPECT_EQ(join(join(a, b), c), join(a, join(b, c)));
}

TEST(DivisorTest, Absorption) {
    divisor_lattice a{12}, b{18};
    EXPECT_EQ(meet(a, join(a, b)), a);
    EXPECT_EQ(join(a, meet(a, b)), a);
}

// =============================================================================
// Powerset Lattice
// =============================================================================

TEST(PowersetTest, MeetIsIntersection) {
    powerset<8> a{0b11001100};
    powerset<8> b{0b10101010};
    EXPECT_EQ(meet(a, b), powerset<8>{0b10001000});
}

TEST(PowersetTest, JoinIsUnion) {
    powerset<8> a{0b11001100};
    powerset<8> b{0b10101010};
    EXPECT_EQ(join(a, b), powerset<8>{0b11101110});
}

TEST(PowersetTest, BottomIsEmpty) {
    powerset<8> bot = bottom(powerset<8>{});
    EXPECT_EQ(bot.bits(), 0u);
    EXPECT_EQ(bot.size(), 0u);
}

TEST(PowersetTest, TopIsFull) {
    powerset<8> t = top(powerset<8>{});
    EXPECT_EQ(t.bits(), 0b11111111u);
    EXPECT_EQ(t.size(), 8u);
}

TEST(PowersetTest, Contains) {
    powerset<8> s;
    s.insert(2);
    s.insert(5);
    EXPECT_TRUE(s.contains(2));
    EXPECT_TRUE(s.contains(5));
    EXPECT_FALSE(s.contains(0));
    EXPECT_FALSE(s.contains(7));
}

TEST(PowersetTest, Idempotent) {
    powerset<8> a{0b10110};
    EXPECT_EQ(meet(a, a), a);
    EXPECT_EQ(join(a, a), a);
}

TEST(PowersetTest, Commutative) {
    powerset<8> a{0b11001100}, b{0b10101010};
    EXPECT_EQ(meet(a, b), meet(b, a));
    EXPECT_EQ(join(a, b), join(b, a));
}

TEST(PowersetTest, Associative) {
    powerset<8> a{0b11000000}, b{0b00110000}, c{0b00001100};
    EXPECT_EQ(meet(meet(a, b), c), meet(a, meet(b, c)));
    EXPECT_EQ(join(join(a, b), c), join(a, join(b, c)));
}

TEST(PowersetTest, Absorption) {
    powerset<8> a{0b11001100}, b{0b10101010};
    EXPECT_EQ(meet(a, join(a, b)), a);
    EXPECT_EQ(join(a, meet(a, b)), a);
}

TEST(PowersetTest, BottomTopIdentity) {
    powerset<8> a{0b10110};
    EXPECT_EQ(join(a, bottom(a)), a);
    EXPECT_EQ(meet(a, top(a)), a);
}

// =============================================================================
// Fixed-Point Iteration
// =============================================================================

TEST(FixedPointTest, IdentityFunctionBottomIsFixedPoint) {
    // f(x) = x: the least fixed point is bottom
    auto id = [](sign_lattice x) { return x; };
    auto fp = least_fixed_point<sign_lattice>(id);
    EXPECT_EQ(fp, sign_lattice::bot);
}

TEST(FixedPointTest, ConstantFunctionFixedPoint) {
    // f(x) = positive: starting from bot, join(bot, pos) = pos,
    // then join(pos, f(pos)) = join(pos, pos) = pos. Fixed point = positive.
    auto f = [](sign_lattice) { return sign_lattice::positive; };
    auto fp = least_fixed_point<sign_lattice>(f);
    EXPECT_EQ(fp, sign_lattice::positive);
}

TEST(FixedPointTest, TopFunction) {
    // f(x) = top: fixed point is top
    auto f = [](sign_lattice) { return sign_lattice::top; };
    auto fp = least_fixed_point<sign_lattice>(f);
    EXPECT_EQ(fp, sign_lattice::top);
}

TEST(FixedPointTest, PowersetReachability) {
    // Model a simple graph: 0 -> 1 -> 2, 0 -> 3
    // Transfer function: given reachable set S, add successors
    auto transfer = [](powerset<4> s) -> powerset<4> {
        powerset<4> result = s;
        if (s.contains(0)) { result.insert(1); result.insert(3); }
        if (s.contains(1)) { result.insert(2); }
        return result;
    };

    // Start from {0}
    auto reach = [&](powerset<4> x) -> powerset<4> {
        powerset<4> init;
        init.insert(0);
        return transfer(join(x, init));
    };

    auto fp = least_fixed_point<powerset<4>>(reach);
    EXPECT_TRUE(fp.contains(0));
    EXPECT_TRUE(fp.contains(1));
    EXPECT_TRUE(fp.contains(2));
    EXPECT_TRUE(fp.contains(3));
    EXPECT_EQ(fp.size(), 4u);
}

TEST(FixedPointTest, DivisorFixedPoint) {
    // f(x) = join(x, 6): starting from bottom=1,
    // join(1, 6) = lcm(1, 6) = 6,
    // join(6, 6) = 6. Fixed point = 6.
    auto f = [](divisor_lattice x) {
        return join(x, divisor_lattice{6});
    };
    auto fp = least_fixed_point<divisor_lattice>(f);
    EXPECT_EQ(fp.value(), 6u);
}

TEST(FixedPointTest, IntervalWithMaxIter) {
    // Widening: f(x) = join(x, [0, x.hi()+1])
    // This would grow without bound on integers, but max_iter stops it.
    auto f = [](interval<int> x) -> interval<int> {
        if (x.is_empty()) return interval<int>(0, 0);
        return join(x, interval<int>(0, x.hi() + 1));
    };
    auto fp = least_fixed_point<interval<int>>(f, 10);
    // After 10 iterations from bottom: [0,0], [0,1], ..., [0,9]
    EXPECT_FALSE(fp.is_empty());
    EXPECT_EQ(fp.lo(), 0);
    EXPECT_EQ(fp.hi(), 9);
}

TEST(FixedPointTest, GreatestFixedPoint) {
    // f(x) = x: greatest fixed point is top
    auto id = [](sign_lattice x) { return x; };
    auto fp = greatest_fixed_point<sign_lattice>(id);
    EXPECT_EQ(fp, sign_lattice::top);
}

TEST(FixedPointTest, GreatestFixedPointConstant) {
    // f(x) = negative: from top, meet(top, neg) = neg,
    // then meet(neg, neg) = neg. Fixed point = negative.
    auto f = [](sign_lattice) { return sign_lattice::negative; };
    auto fp = greatest_fixed_point<sign_lattice>(f);
    EXPECT_EQ(fp, sign_lattice::negative);
}

// =============================================================================
// Abstract Interpretation Mini-Example
// =============================================================================

TEST(AbstractInterpTest, SimpleExpression) {
    using S = sign_lattice;
    // x=pos, y=neg, z=pos: x*y + z = neg + pos = top
    EXPECT_EQ(abstract_eval_mul_add(S::positive, S::negative, S::positive),
              S::top);
    // x=pos, y=pos, z=zero: x*y + z = pos + zero = pos
    EXPECT_EQ(abstract_eval_mul_add(S::positive, S::positive, S::zero),
              S::positive);
    // x=neg, y=neg, z=neg: x*y + z = pos + neg = top
    EXPECT_EQ(abstract_eval_mul_add(S::negative, S::negative, S::negative),
              S::top);
}

TEST(AbstractInterpTest, LoopFixedPoint) {
    using S = sign_lattice;
    // Model a loop: x starts at zero, each iteration adds a positive.
    // Transfer: given abstract sign of x, compute sign of (x + positive).
    // x=bot -> bot (unreachable initially, but we join with zero)
    // The transfer function includes the initial condition.
    auto transfer = [](S x) -> S {
        S init = S::zero;
        S current = join(x, init);
        return abstract_add(current, S::positive);
    };

    // Trace: bot -> join(bot, transfer(bot))
    //   transfer(bot) = abstract_add(join(bot, zero), pos)
    //                  = abstract_add(zero, pos) = pos
    //   join(bot, pos) = pos
    // Then: transfer(pos) = abstract_add(join(pos, zero), pos)
    //                      = abstract_add(top, pos) = top
    //   join(pos, top) = top
    // Then: transfer(top) = abstract_add(join(top, zero), pos)
    //                      = abstract_add(top, pos) = top
    //   join(top, top) = top. Fixed point = top.
    auto fp = least_fixed_point<S>(transfer);
    EXPECT_EQ(fp, S::top);
}

TEST(AbstractInterpTest, SquareIsNonnegative) {
    using S = sign_lattice;
    // x * x is always non-negative
    EXPECT_EQ(abstract_mul(S::positive, S::positive), S::positive);
    EXPECT_EQ(abstract_mul(S::negative, S::negative), S::positive);
    EXPECT_EQ(abstract_mul(S::zero, S::zero), S::zero);
    // But if x is unknown, x*x is also unknown (the sign lattice
    // is too coarse to express "non-negative")
    EXPECT_EQ(abstract_mul(S::top, S::top), S::top);
}

// =============================================================================
// Constexpr Tests
// =============================================================================

TEST(LatticeTest, Constexpr) {
    constexpr auto sm = meet(sign_lattice::positive, sign_lattice::top);
    constexpr auto sj = join(sign_lattice::bot, sign_lattice::negative);
    static_assert(sm == sign_lattice::positive);
    static_assert(sj == sign_lattice::negative);

    constexpr auto im = meet(interval<int>(1, 5), interval<int>(3, 8));
    static_assert(im == interval<int>(3, 5));

    constexpr auto dm = meet(divisor_lattice{12}, divisor_lattice{8});
    static_assert(dm == divisor_lattice{4});

    constexpr auto pm = meet(powerset<8>{0b1100}, powerset<8>{0b1010});
    static_assert(pm == powerset<8>{0b1000});
}
