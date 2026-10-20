#include <gtest/gtest.h>
#include <cmath>
#include <cstddef>
#include <vector>
#include "huffman.hpp"
#include "../2022-01-priors-wire-formats/priors.hpp"

using namespace huffman;

// Minimal BitSink + BitSource backed by a std::vector<bool>.
// Used only in round-trip tests.
struct BitVector {
    std::vector<bool> bits;
    std::size_t read_pos = 0;

    void write(bool b) { bits.push_back(b); }
    bool read() {
        assert(read_pos < bits.size());
        return bits[read_pos++];
    }
    void reset() { read_pos = 0; }
};

// Helper: count the total number of leaves reachable from a node.
static int count_leaves(const Node* node) {
    if (!node) return 0;
    if (node->symbol >= 0) return 1;  // leaf
    return count_leaves(node->left.get()) + count_leaves(node->right.get());
}

// Helper: count the total number of nodes (leaves + internal).
static int count_nodes(const Node* node) {
    if (!node) return 0;
    return 1 + count_nodes(node->left.get()) + count_nodes(node->right.get());
}

// For a 4-symbol distribution, the Huffman tree must have exactly 4 leaves
// and (4 - 1) = 3 internal nodes, so 7 nodes total.
TEST(HuffmanTest, TreeHasCorrectLeafCount) {
    std::vector<double> freqs = {0.4, 0.3, 0.2, 0.1};
    auto root = build_huffman_tree(freqs);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(count_leaves(root.get()), 4);
    EXPECT_EQ(count_nodes(root.get()), 7);  // 4 leaves + 3 internal
}

// The root's frequency must equal the sum of all input frequencies.
TEST(HuffmanTest, RootFrequencyIsSum) {
    std::vector<double> freqs = {0.4, 0.3, 0.2, 0.1};
    auto root = build_huffman_tree(freqs);
    ASSERT_NE(root, nullptr);
    double total = 0.0;
    for (double f : freqs) total += f;
    EXPECT_NEAR(root->freq, total, 1e-12);
}

// Single-symbol distribution: the root is itself a leaf.
TEST(HuffmanTest, SingleSymbolTreeIsLeaf) {
    std::vector<double> freqs = {1.0};
    auto root = build_huffman_tree(freqs);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->symbol, 0);
    EXPECT_EQ(root->left, nullptr);
    EXPECT_EQ(root->right, nullptr);
}

// Two-symbol distribution: root has two leaf children.
TEST(HuffmanTest, TwoSymbolTree) {
    std::vector<double> freqs = {0.6, 0.4};
    auto root = build_huffman_tree(freqs);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->symbol, -1);  // internal node
    ASSERT_NE(root->left, nullptr);
    ASSERT_NE(root->right, nullptr);
    EXPECT_GE(root->left->symbol, 0);   // leaf
    EXPECT_GE(root->right->symbol, 0);  // leaf
}

// Leaf symbols: each symbol index 0..n-1 must appear exactly once.
TEST(HuffmanTest, AllSymbolsPresent) {
    std::vector<double> freqs = {0.25, 0.25, 0.25, 0.25};
    auto root = build_huffman_tree(freqs);
    ASSERT_NE(root, nullptr);
    // Collect leaf symbols.
    std::vector<int> found;
    std::function<void(const Node*)> collect = [&](const Node* n) {
        if (!n) return;
        if (n->symbol >= 0) found.push_back(n->symbol);
        collect(n->left.get());
        collect(n->right.get());
    };
    collect(root.get());
    std::sort(found.begin(), found.end());
    ASSERT_EQ(found.size(), 4u);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(found[i], i);
}

// For a two-symbol distribution, both codewords must be exactly 1 bit ("0" or "1").
TEST(HuffmanTest, TwoSymbolCodewordLengthIs1) {
    std::vector<double> freqs = {0.6, 0.4};
    auto root = build_huffman_tree(freqs);
    auto cb = tree_to_codebook(root.get());
    ASSERT_EQ(cb.size(), 2u);
    for (const auto& [sym, cw] : cb) {
        EXPECT_EQ(cw.size(), 1u) << "symbol " << sym << " has codeword length " << cw.size();
    }
}

// For a uniform 4-symbol distribution, all codewords should be 2 bits
// (Huffman reduces to fixed-width for uniform power-of-2 alphabets).
TEST(HuffmanTest, UniformFourSymbolAllTwoBits) {
    std::vector<double> freqs = {0.25, 0.25, 0.25, 0.25};
    auto root = build_huffman_tree(freqs);
    auto cb = tree_to_codebook(root.get());
    ASSERT_EQ(cb.size(), 4u);
    for (const auto& [sym, cw] : cb) {
        EXPECT_EQ(cw.size(), 2u) << "symbol " << sym;
    }
}

// The most frequent symbol must get the shortest codeword (or tied-shortest).
// Distribution: {0.5, 0.25, 0.125, 0.125}.
// Symbol 0 (freq 0.5) should get a 1-bit codeword.
TEST(HuffmanTest, MostFrequentSymbolShortestCode) {
    std::vector<double> freqs = {0.5, 0.25, 0.125, 0.125};
    auto root = build_huffman_tree(freqs);
    auto cb = tree_to_codebook(root.get());
    ASSERT_EQ(cb.size(), 4u);
    std::size_t len0 = cb.at(0).size();
    for (const auto& [sym, cw] : cb) {
        EXPECT_GE(cw.size(), len0)
            << "symbol " << sym << " has shorter code than symbol 0";
    }
}

// Codebook must be prefix-free: no codeword is a prefix of another.
TEST(HuffmanTest, CodebookIsPrefixFree) {
    std::vector<double> freqs = {0.4, 0.3, 0.2, 0.1};
    auto root = build_huffman_tree(freqs);
    auto cb = tree_to_codebook(root.get());
    // For every pair (a, b), a is not a prefix of b.
    for (const auto& [sa, ca] : cb) {
        for (const auto& [sb, cb2] : cb) {
            if (sa == sb) continue;
            bool a_prefix_of_b = (cb2.size() >= ca.size()) &&
                                 (cb2.substr(0, ca.size()) == ca);
            EXPECT_FALSE(a_prefix_of_b)
                << "codeword \"" << ca << "\" (sym " << sa
                << ") is a prefix of \"" << cb2 << "\" (sym " << sb << ")";
        }
    }
}

// Single-symbol distribution: codebook has one entry.
// The codeword is conventionally "0" (assign at least 1 bit).
TEST(HuffmanTest, SingleSymbolCodebook) {
    std::vector<double> freqs = {1.0};
    auto root = build_huffman_tree(freqs);
    auto cb = tree_to_codebook(root.get());
    ASSERT_EQ(cb.size(), 1u);
    EXPECT_FALSE(cb.at(0).empty());
}

// Round-trip single symbol: encode then decode must recover the symbol.
TEST(HuffmanTest, RoundTripSingleSymbol) {
    std::vector<double> freqs = {1.0};
    auto root   = build_huffman_tree(freqs);
    auto cb     = tree_to_codebook(root.get());
    BitVector bv;
    encode(0, cb, bv);
    bv.reset();
    int sym = decode(root.get(), bv);
    EXPECT_EQ(sym, 0);
}

// Round-trip for each symbol of a 4-symbol distribution.
TEST(HuffmanTest, RoundTripFourSymbols) {
    std::vector<double> freqs = {0.4, 0.3, 0.2, 0.1};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    for (int s = 0; s < 4; ++s) {
        BitVector bv;
        encode(s, cb, bv);
        bv.reset();
        int decoded = decode(root.get(), bv);
        EXPECT_EQ(decoded, s) << "symbol " << s << " did not round-trip";
    }
}

// Round-trip for a longer sequence of symbols.
// Encodes [0, 1, 2, 3, 0, 2, 1, 3] and decodes them back in order.
TEST(HuffmanTest, RoundTripSequence) {
    std::vector<double> freqs = {0.4, 0.3, 0.2, 0.1};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    std::vector<int> input = {0, 1, 2, 3, 0, 2, 1, 3};
    BitVector bv;
    for (int s : input) encode(s, cb, bv);
    bv.reset();
    for (int expected : input) {
        int got = decode(root.get(), bv);
        EXPECT_EQ(got, expected);
    }
}

// Round-trip: uniform binary source (2 symbols).
TEST(HuffmanTest, RoundTripBinaryUniform) {
    std::vector<double> freqs = {0.5, 0.5};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    std::vector<int> input = {0, 1, 0, 0, 1, 1, 0, 1};
    BitVector bv;
    for (int s : input) encode(s, cb, bv);
    bv.reset();
    for (int expected : input) {
        EXPECT_EQ(decode(root.get(), bv), expected);
    }
}

// Helper: extract codeword lengths from a codebook, indexed by symbol.
static std::vector<std::size_t> codebook_lengths(
        const std::map<int, std::string>& cb, std::size_t n) {
    std::vector<std::size_t> lens(n);
    for (const auto& [sym, cw] : cb) {
        assert(static_cast<std::size_t>(sym) < n);
        lens[static_cast<std::size_t>(sym)] = cw.size();
    }
    return lens;
}

// Optimality test: uniform distribution over 4 symbols.
// Entropy = 2 bits. Expected length = 2 bits (Huffman gives fixed-width code).
// Redundancy = 0.
TEST(HuffmanTest, OptimalityUniform4) {
    std::vector<double> freqs = {0.25, 0.25, 0.25, 0.25};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto lens = codebook_lengths(cb, freqs.size());
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_NEAR(H, 2.0, 1e-12);
    EXPECT_NEAR(L, 2.0, 1e-12);  // Huffman achieves entropy exactly here.
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}

// Optimality test: dyadic distribution {0.5, 0.25, 0.125, 0.125}.
// Entropy = 1.75 bits. Huffman achieves exactly 1.75 bits.
TEST(HuffmanTest, OptimalityDyadic) {
    std::vector<double> freqs = {0.5, 0.25, 0.125, 0.125};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto lens = codebook_lengths(cb, freqs.size());
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_NEAR(H, 1.75, 1e-12);
    EXPECT_NEAR(L, H, 1e-9);  // Dyadic: Huffman achieves entropy exactly.
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}

// Optimality test: geometric(1/2) truncated to 8 symbols.
// H < L <= H + 1.
TEST(HuffmanTest, OptimalityGeometric) {
    const std::size_t K = 8;
    std::vector<double> freqs(K);
    double z = 0.0;
    for (std::size_t i = 0; i < K; ++i) {
        freqs[i] = std::ldexp(1.0, -static_cast<int>(i + 1));
        z += freqs[i];
    }
    for (double& f : freqs) f /= z;
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto lens = codebook_lengths(cb, freqs.size());
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}

// Optimality test: Zipf distribution over 8 symbols (p_i = C/i).
TEST(HuffmanTest, OptimalityZipf) {
    const std::size_t N = 8;
    std::vector<double> freqs(N);
    double z = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        freqs[i] = 1.0 / static_cast<double>(i + 1);
        z += freqs[i];
    }
    for (double& f : freqs) f /= z;
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto lens = codebook_lengths(cb, freqs.size());
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}

// Optimality test: highly skewed binary source (p0 = 0.99, p1 = 0.01).
// Entropy ~ 0.081 bits. Huffman cannot compress below 1 bit/symbol
// (the integer-length constraint). L = 1.0 > H.
TEST(HuffmanTest, OptimalityHighlySkewedBinary) {
    std::vector<double> freqs = {0.99, 0.01};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto lens = codebook_lengths(cb, freqs.size());
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_LT(H, 0.15);        // Entropy is very low.
    EXPECT_NEAR(L, 1.0, 1e-9); // Huffman is stuck at 1 bit/symbol.
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}

// Uniform 8-symbol distribution: Huffman produces 3-bit fixed-width code.
// (8 = 2^3; entropy = 3 bits; Huffman achieves entropy exactly.)
TEST(HuffmanTest, UniformEightSymbolFixedWidth) {
    const std::size_t N = 8;
    std::vector<double> freqs(N, 1.0 / static_cast<double>(N));
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    for (const auto& [sym, cw] : cb) {
        EXPECT_EQ(cw.size(), 3u) << "symbol " << sym;
    }
}

// Huffman beats gamma on a known distribution.
// On a power-law(2) source with 16 symbols, Huffman's expected length
// should be <= gamma's expected length (knowing the distribution helps).
TEST(HuffmanTest, HuffmanBeatsGammaOnPowerLaw2) {
    const std::size_t N = 16;
    std::vector<double> freqs(N);
    double z = 0.0;
    for (std::size_t i = 0; i < N; ++i) {
        double n = static_cast<double>(i + 1);
        freqs[i] = 1.0 / (n * n);
        z += freqs[i];
    }
    for (double& f : freqs) f /= z;

    // Huffman expected length.
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    auto huff_lens = codebook_lengths(cb, freqs.size());
    double L_huff = priors::expected_length(freqs, huff_lens);

    // Gamma expected length: l_n = 2*floor(log2(n)) + 1 for n = 1..N.
    std::vector<std::size_t> gamma_lens(N);
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t n = i + 1, k = 0, tmp = n;
        while (tmp > 1) { tmp >>= 1; ++k; }
        gamma_lens[i] = 2 * k + 1;
    }
    double L_gamma = priors::expected_length(freqs, gamma_lens);

    EXPECT_LE(L_huff, L_gamma + 1e-9)
        << "Huffman (" << L_huff << " bits) should be no worse than gamma (" << L_gamma << " bits)";
}

// Two-symbol distribution: codeword lengths are always exactly 1 bit each.
// This holds regardless of the probabilities (as long as both > 0).
TEST(HuffmanTest, TwoSymbolAlwaysOneBit) {
    for (double p : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        std::vector<double> freqs = {p, 1.0 - p};
        auto root = build_huffman_tree(freqs);
        auto cb   = tree_to_codebook(root.get());
        EXPECT_EQ(cb.at(0).size(), 1u) << "p = " << p;
        EXPECT_EQ(cb.at(1).size(), 1u) << "p = " << p;
    }
}

// Longer integer sequence round-trip (100 symbols drawn from a 6-symbol
// alphabet, all symbols present, all must decode correctly).
TEST(HuffmanTest, RoundTripLongSequence) {
    std::vector<double> freqs = {0.4, 0.2, 0.15, 0.1, 0.1, 0.05};
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());
    // Deterministic sequence: cycle through symbols weighted by freq bucket.
    std::vector<int> input;
    for (int rep = 0; rep < 20; ++rep) {
        for (int s : {0, 0, 1, 0, 2, 1, 0, 3, 4, 5}) input.push_back(s);
    }
    BitVector bv;
    for (int s : input) encode(s, cb, bv);
    bv.reset();
    for (int expected : input) {
        EXPECT_EQ(decode(root.get(), bv), expected);
    }
}

// Two-pass round-trip: learn frequencies from a training sequence, then
// build Huffman, encode the same sequence, decode and verify.
TEST(HuffmanTest, RoundTripLearnedDistribution) {
    // Training sequence: 5-symbol alphabet with skewed frequency.
    std::vector<int> train = {
        0, 0, 0, 1, 0, 0, 2, 0, 1, 0,
        0, 3, 0, 0, 1, 0, 0, 4, 0, 0,
        1, 0, 0, 0, 2, 0, 1, 0, 0, 0
    };
    const std::size_t alphabet_size = 5;

    // Count frequencies.
    std::vector<double> freqs(alphabet_size, 0.0);
    for (int s : train) {
        assert(s >= 0 && static_cast<std::size_t>(s) < alphabet_size);
        freqs[static_cast<std::size_t>(s)] += 1.0;
    }
    // Normalize.
    double total = 0.0;
    for (double f : freqs) total += f;
    for (double& f : freqs) f /= total;

    // Build codec.
    auto root = build_huffman_tree(freqs);
    auto cb   = tree_to_codebook(root.get());

    // Encode and decode.
    BitVector bv;
    for (int s : train) encode(s, cb, bv);
    bv.reset();
    std::vector<int> decoded;
    for (std::size_t i = 0; i < train.size(); ++i) {
        decoded.push_back(decode(root.get(), bv));
    }

    EXPECT_EQ(decoded, train);

    // Verify expected length is within 1 bit of entropy.
    auto lens = codebook_lengths(cb, alphabet_size);
    double H  = priors::entropy(freqs);
    double L  = priors::expected_length(freqs, lens);
    EXPECT_GE(L, H - 1e-9);
    EXPECT_LE(L, H + 1.0 + 1e-9);
}
