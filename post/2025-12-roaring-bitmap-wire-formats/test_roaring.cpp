#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "roaring_bitmap.hpp"

using namespace roaring;

// ---- ArrayContainer tests ---------------------------------------------------

TEST(ArrayContainerTest, EmptyOnConstruct) {
    ArrayContainer c;
    EXPECT_EQ(c.cardinality(), 0u);
    EXPECT_FALSE(c.contains(0));
    EXPECT_FALSE(c.contains(65535));
}

TEST(ArrayContainerTest, AddAndContains) {
    ArrayContainer c;
    c.add(100);
    c.add(200);
    c.add(50);
    EXPECT_EQ(c.cardinality(), 3u);
    EXPECT_TRUE(c.contains(50));
    EXPECT_TRUE(c.contains(100));
    EXPECT_TRUE(c.contains(200));
    EXPECT_FALSE(c.contains(99));
    EXPECT_FALSE(c.contains(101));
}

TEST(ArrayContainerTest, DuplicateAddNoChange) {
    ArrayContainer c;
    c.add(42);
    c.add(42);
    EXPECT_EQ(c.cardinality(), 1u);
}

TEST(ArrayContainerTest, AddMaintainsSortedOrder) {
    ArrayContainer c;
    c.add(300);
    c.add(10);
    c.add(100);
    // Internally sorted: contains should work via binary search.
    EXPECT_TRUE(c.contains(10));
    EXPECT_TRUE(c.contains(100));
    EXPECT_TRUE(c.contains(300));
    EXPECT_FALSE(c.contains(200));
}

TEST(ArrayContainerTest, CardinalityGrows) {
    ArrayContainer c;
    for (uint16_t v = 0; v < 100; ++v) c.add(v);
    EXPECT_EQ(c.cardinality(), 100u);
}

// ---- BitmapContainer tests --------------------------------------------------

TEST(BitmapContainerTest, EmptyOnConstruct) {
    BitmapContainer c;
    EXPECT_EQ(c.cardinality(), 0u);
    EXPECT_FALSE(c.contains(0));
    EXPECT_FALSE(c.contains(65535));
}

TEST(BitmapContainerTest, AddAndContains) {
    BitmapContainer c;
    c.add(0);
    c.add(65535);
    c.add(1000);
    EXPECT_EQ(c.cardinality(), 3u);
    EXPECT_TRUE(c.contains(0));
    EXPECT_TRUE(c.contains(65535));
    EXPECT_TRUE(c.contains(1000));
    EXPECT_FALSE(c.contains(1));
    EXPECT_FALSE(c.contains(999));
}

TEST(BitmapContainerTest, DuplicateAddNoChange) {
    BitmapContainer c;
    c.add(500);
    c.add(500);
    EXPECT_EQ(c.cardinality(), 1u);
}

TEST(BitmapContainerTest, CardinalityAfterManyAdds) {
    BitmapContainer c;
    for (uint16_t v = 0; v < 1000; ++v) c.add(v);
    EXPECT_EQ(c.cardinality(), 1000u);
}

TEST(BitmapContainerTest, WordBoundaryBits) {
    BitmapContainer c;
    c.add(63);   // Last bit of word 0.
    c.add(64);   // First bit of word 1.
    EXPECT_TRUE(c.contains(63));
    EXPECT_TRUE(c.contains(64));
    EXPECT_FALSE(c.contains(62));
    EXPECT_FALSE(c.contains(65));
}

// ---- RunContainer tests -----------------------------------------------------

TEST(RunContainerTest, EmptyOnConstruct) {
    RunContainer c;
    EXPECT_EQ(c.cardinality(), 0u);
    EXPECT_FALSE(c.contains(0));
}

TEST(RunContainerTest, SingleElement) {
    RunContainer c;
    c.add(42);
    EXPECT_EQ(c.cardinality(), 1u);
    EXPECT_TRUE(c.contains(42));
    EXPECT_FALSE(c.contains(41));
    EXPECT_FALSE(c.contains(43));
}

TEST(RunContainerTest, ConsecutiveAddsMergeRun) {
    RunContainer c;
    c.add(10);
    c.add(11);
    c.add(12);
    EXPECT_EQ(c.cardinality(), 3u);
    EXPECT_EQ(c.num_runs(), 1u);  // All merged into one run [10,12].
    EXPECT_TRUE(c.contains(10));
    EXPECT_TRUE(c.contains(11));
    EXPECT_TRUE(c.contains(12));
    EXPECT_FALSE(c.contains(9));
    EXPECT_FALSE(c.contains(13));
}

TEST(RunContainerTest, NonConsecutiveAddsTwoRuns) {
    RunContainer c;
    c.add(5);
    c.add(6);
    c.add(10);
    c.add(11);
    EXPECT_EQ(c.cardinality(), 4u);
    EXPECT_EQ(c.num_runs(), 2u);
}

TEST(RunContainerTest, AddAtBoundaryExtendsPriorRun) {
    RunContainer c;
    c.add(100);
    c.add(101);
    c.add(102);
    c.add(103);  // Should extend [100,102] to [100,103].
    EXPECT_EQ(c.num_runs(), 1u);
    EXPECT_EQ(c.cardinality(), 4u);
}

TEST(RunContainerTest, DuplicateAddNoChange) {
    RunContainer c;
    c.add(7);
    c.add(8);
    c.add(7);  // Duplicate.
    EXPECT_EQ(c.cardinality(), 2u);
    EXPECT_EQ(c.num_runs(), 1u);
}

// ---- Container conversion tests ---------------------------------------------

// array_to_bitmap: converts ArrayContainer contents into a BitmapContainer.
TEST(ConversionTest, ArrayToBitmap) {
    ArrayContainer a;
    for (uint16_t v = 0; v < 100; ++v) a.add(v);
    BitmapContainer b = array_to_bitmap(a);
    EXPECT_EQ(b.cardinality(), 100u);
    for (uint16_t v = 0; v < 100; ++v) EXPECT_TRUE(b.contains(v));
    EXPECT_FALSE(b.contains(100));
}

// bitmap_to_array: converts BitmapContainer contents into an ArrayContainer.
TEST(ConversionTest, BitmapToArray) {
    BitmapContainer b;
    for (uint16_t v = 0; v < 50; ++v) b.add(v);
    ArrayContainer a = bitmap_to_array(b);
    EXPECT_EQ(a.cardinality(), 50u);
    for (uint16_t v = 0; v < 50; ++v) EXPECT_TRUE(a.contains(v));
    EXPECT_FALSE(a.contains(50));
}

// array_to_run: converts ArrayContainer into a RunContainer.
TEST(ConversionTest, ArrayToRun) {
    ArrayContainer a;
    for (uint16_t v = 10; v < 20; ++v) a.add(v);  // One run: [10, 19].
    RunContainer r = array_to_run(a);
    EXPECT_EQ(r.cardinality(), 10u);
    EXPECT_EQ(r.num_runs(), 1u);
    for (uint16_t v = 10; v < 20; ++v) EXPECT_TRUE(r.contains(v));
}

// Round-trip: array -> bitmap -> array preserves cardinality.
TEST(ConversionTest, ArrayBitmapArrayRoundTrip) {
    ArrayContainer a_orig;
    for (uint16_t v = 0; v < 200; v += 3) a_orig.add(v);
    BitmapContainer b = array_to_bitmap(a_orig);
    ArrayContainer a_back = bitmap_to_array(b);
    EXPECT_EQ(a_back.cardinality(), a_orig.cardinality());
    for (const auto& val : a_orig.elements()) {
        EXPECT_TRUE(a_back.contains(val));
    }
}

// ---- RoaringBitmap tests ----------------------------------------------------

TEST(RoaringBitmapTest, EmptyOnConstruct) {
    RoaringBitmap rb;
    EXPECT_EQ(rb.cardinality(), 0u);
    EXPECT_FALSE(rb.contains(0));
    EXPECT_FALSE(rb.contains(0xFFFFFFFF));
}

TEST(RoaringBitmapTest, AddAndContains) {
    RoaringBitmap rb;
    rb.add(0);
    rb.add(65535);             // Same chunk as 0 (high bits = 0).
    rb.add(65536);             // Different chunk (high bits = 1).
    rb.add(0xFFFFFFFF);        // Highest possible value.
    EXPECT_TRUE(rb.contains(0));
    EXPECT_TRUE(rb.contains(65535));
    EXPECT_TRUE(rb.contains(65536));
    EXPECT_TRUE(rb.contains(0xFFFFFFFF));
    EXPECT_FALSE(rb.contains(1));
    EXPECT_FALSE(rb.contains(65534));
}

TEST(RoaringBitmapTest, CardinalityAcrossChunks) {
    RoaringBitmap rb;
    for (uint32_t i = 0; i < 10; ++i) rb.add(i);         // Chunk 0.
    for (uint32_t i = 65536; i < 65540; ++i) rb.add(i);  // Chunk 1.
    EXPECT_EQ(rb.cardinality(), 14u);
}

TEST(RoaringBitmapTest, DuplicateAddNoChange) {
    RoaringBitmap rb;
    rb.add(100);
    rb.add(100);
    EXPECT_EQ(rb.cardinality(), 1u);
}

TEST(RoaringBitmapTest, AutoConvertArrayToBitmap) {
    RoaringBitmap rb;
    // Adding ARRAY_MAX + 1 distinct values to chunk 0 forces array -> bitmap.
    for (uint32_t i = 0; i <= 4096; ++i) rb.add(i);
    EXPECT_EQ(rb.cardinality(), 4097u);
    // All values must still be accessible.
    for (uint32_t i = 0; i <= 4096; ++i) EXPECT_TRUE(rb.contains(i));
    EXPECT_FALSE(rb.contains(4097));
}

// ---- Set operation tests ----------------------------------------------------

TEST(RoaringSetOpsTest, UnionDisjoint) {
    RoaringBitmap a, b;
    a.add(1); a.add(2);
    b.add(3); b.add(4);
    RoaringBitmap u = a.union_with(b);
    EXPECT_EQ(u.cardinality(), 4u);
    for (uint32_t v : {1u, 2u, 3u, 4u}) EXPECT_TRUE(u.contains(v));
}

TEST(RoaringSetOpsTest, UnionOverlapping) {
    RoaringBitmap a, b;
    a.add(10); a.add(20);
    b.add(20); b.add(30);
    RoaringBitmap u = a.union_with(b);
    EXPECT_EQ(u.cardinality(), 3u);
    EXPECT_TRUE(u.contains(10));
    EXPECT_TRUE(u.contains(20));
    EXPECT_TRUE(u.contains(30));
}

TEST(RoaringSetOpsTest, IntersectionOverlapping) {
    RoaringBitmap a, b;
    a.add(5); a.add(10); a.add(15);
    b.add(10); b.add(15); b.add(20);
    RoaringBitmap inter = a.intersection_with(b);
    EXPECT_EQ(inter.cardinality(), 2u);
    EXPECT_TRUE(inter.contains(10));
    EXPECT_TRUE(inter.contains(15));
    EXPECT_FALSE(inter.contains(5));
    EXPECT_FALSE(inter.contains(20));
}

TEST(RoaringSetOpsTest, IntersectionDisjoint) {
    RoaringBitmap a, b;
    a.add(1); a.add(2);
    b.add(3); b.add(4);
    RoaringBitmap inter = a.intersection_with(b);
    EXPECT_EQ(inter.cardinality(), 0u);
}

TEST(RoaringSetOpsTest, DifferenceAMinusB) {
    RoaringBitmap a, b;
    a.add(1); a.add(2); a.add(3);
    b.add(2); b.add(4);
    RoaringBitmap diff = a.difference(b);
    EXPECT_EQ(diff.cardinality(), 2u);
    EXPECT_TRUE(diff.contains(1));
    EXPECT_TRUE(diff.contains(3));
    EXPECT_FALSE(diff.contains(2));
}

TEST(RoaringSetOpsTest, SetOpsAcrossChunks) {
    RoaringBitmap a, b;
    a.add(10);               // Chunk 0.
    a.add(65536 + 5);        // Chunk 1.
    b.add(10);               // Chunk 0.
    b.add(65536 + 10);       // Chunk 1, different value.
    RoaringBitmap u = a.union_with(b);
    EXPECT_EQ(u.cardinality(), 3u);
    RoaringBitmap inter = a.intersection_with(b);
    EXPECT_EQ(inter.cardinality(), 1u);
    EXPECT_TRUE(inter.contains(10));
}

// ---- Round-trip and space-efficiency tests ----------------------------------

// Very sparse: 100 random-ish values in [0, 2^32). Should stay as ArrayContainers.
TEST(RoaringSpaceTest, SparseSetsUseFewChunks) {
    RoaringBitmap rb;
    for (uint32_t i = 0; i < 100; ++i) {
        rb.add(i * 65536u + (i * 13u % 65536u));  // One value per chunk.
    }
    EXPECT_EQ(rb.cardinality(), 100u);
    for (uint32_t i = 0; i < 100; ++i) {
        EXPECT_TRUE(rb.contains(i * 65536u + (i * 13u % 65536u)));
    }
}

// Moderately dense: 5000 values in chunk 0. Should trigger array -> bitmap.
TEST(RoaringSpaceTest, ModeratelDenseTriggersBitmapConversion) {
    RoaringBitmap rb;
    for (uint32_t i = 0; i < 5000; ++i) rb.add(i);
    EXPECT_EQ(rb.cardinality(), 5000u);
    // All 5000 values must still be present after conversion.
    for (uint32_t i = 0; i < 5000; ++i) EXPECT_TRUE(rb.contains(i));
    EXPECT_FALSE(rb.contains(5000));
}

// Clustered: large run of consecutive values. optimize() yields RunContainer.
TEST(RoaringSpaceTest, ClusteredRunOptimizesToRunContainer) {
    RoaringBitmap rb;
    for (uint32_t i = 1000; i < 2000; ++i) rb.add(i);  // 1000 consecutive values.
    EXPECT_EQ(rb.cardinality(), 1000u);
    rb.optimize();  // Should convert the array into a RunContainer.
    // Cardinality and membership must be unchanged after optimize().
    EXPECT_EQ(rb.cardinality(), 1000u);
    for (uint32_t i = 1000; i < 2000; ++i) EXPECT_TRUE(rb.contains(i));
    EXPECT_FALSE(rb.contains(999));
    EXPECT_FALSE(rb.contains(2000));
}

// Dense: full chunk (all 65536 values in chunk 0).
TEST(RoaringSpaceTest, FullChunkAllValuesPresent) {
    RoaringBitmap rb;
    for (uint32_t i = 0; i < 65536; ++i) rb.add(i);
    EXPECT_EQ(rb.cardinality(), 65536u);
    EXPECT_TRUE(rb.contains(0));
    EXPECT_TRUE(rb.contains(65535));
    EXPECT_FALSE(rb.contains(65536));
}

// Union preserves cardinality: |A union B| = |A| + |B| - |A intersect B|.
TEST(RoaringSpaceTest, UnionCardinalityFormula) {
    RoaringBitmap a, b;
    for (uint32_t i = 0; i < 100; ++i) a.add(i);
    for (uint32_t i = 50; i < 150; ++i) b.add(i);
    RoaringBitmap u     = a.union_with(b);
    RoaringBitmap inter = a.intersection_with(b);
    EXPECT_EQ(u.cardinality(),
              a.cardinality() + b.cardinality() - inter.cardinality());
}
