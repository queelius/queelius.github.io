// huffman.hpp
// Pedagogical implementation for the post "Huffman Coding" in the
// "Algebra over Wire Formats" series. For the production version, see PFC:
// https://github.com/queelius/pfc (huffman.hpp)
//
// Gotcha: build_huffman_tree uses const_cast on pq.top() to enable moving
// out of a std::priority_queue. std::priority_queue::top() returns const&
// because the queue must not be mutated through the reference (that would
// break the heap invariant). Once we call pq.pop() immediately after, the
// invariant is maintained; the const_cast is safe here. Do not remove it or
// replace the std::move with a copy -- the tree uses std::unique_ptr and
// cannot be copied.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace huffman {

// BitSink and BitSource concepts (minimal local definitions for self-contained
// pedagogical use; production code uses pfc/core.hpp).

template<typename S>
concept BitSink = requires(S& s, bool b) {
    { s.write(b) } -> std::same_as<void>;
};

template<typename S>
concept BitSource = requires(S& s) {
    { s.read() } -> std::same_as<bool>;
};

// ---- Node -- tree node for the Huffman construction -------------------------
//
// Leaf nodes have symbol >= 0 (the symbol index) and null children.
// Internal nodes have symbol = -1 and non-null left and right children.

struct Node {
    double freq;
    int symbol = -1;  // -1 for internal nodes
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

// ---- build_huffman_tree -- greedy bottom-up tree construction ---------------
//
// Algorithm:
//   1. Push one leaf node per symbol into a min-priority-queue (ordered by
//      frequency, lowest first).
//   2. While more than one node remains: extract the two lowest-frequency
//      nodes, combine into a new internal node whose frequency is their sum,
//      push the internal node back.
//   3. The remaining node is the root.
//
// Complexity: O(n log n) where n = freqs.size().
//
// GOTCHA: std::priority_queue::top() returns const& (to prevent callers from
// mutating the top element and breaking the heap invariant). Here we call
// pq.pop() immediately after the const_cast + std::move, so the heap
// invariant is maintained. This is the standard workaround for moving out of
// a std::priority_queue. Do not change std::move to a copy -- Node owns
// std::unique_ptr children and is not copyable.

inline std::unique_ptr<Node> build_huffman_tree(const std::vector<double>& freqs) {
    assert(!freqs.empty() && "build_huffman_tree requires at least one symbol");

    auto cmp = [](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
        return a->freq > b->freq;  // min-heap: lowest frequency on top
    };
    std::priority_queue<std::unique_ptr<Node>,
                        std::vector<std::unique_ptr<Node>>,
                        decltype(cmp)> pq(cmp);

    for (std::size_t i = 0; i < freqs.size(); ++i) {
        auto node = std::make_unique<Node>();
        node->freq   = freqs[i];
        node->symbol = static_cast<int>(i);
        pq.push(std::move(node));
    }

    while (pq.size() > 1) {
        // Extract the two lowest-frequency nodes.
        auto a = std::move(const_cast<std::unique_ptr<Node>&>(pq.top())); pq.pop();
        auto b = std::move(const_cast<std::unique_ptr<Node>&>(pq.top())); pq.pop();

        // Combine into a new internal node.
        auto parent      = std::make_unique<Node>();
        parent->freq     = a->freq + b->freq;
        // parent->symbol remains -1 (internal node default).
        parent->left     = std::move(a);
        parent->right    = std::move(b);
        pq.push(std::move(parent));
    }

    return std::move(const_cast<std::unique_ptr<Node>&>(pq.top()));
}

// ---- tree_to_codebook -- recursive DFS to extract symbol-to-codeword map ---
//
// Walks the tree, accumulating a path string ("0" for left, "1" for right).
// At each leaf, emits the symbol-to-path mapping into the result map.
//
// For a single-symbol tree (the root is itself a leaf), the function returns
// {"0"} for that symbol. A single-bit codeword is the minimum meaningful
// codeword; it is not prefix-free with any other code, but for a one-symbol
// alphabet there is nothing to be prefix-free against.
//
// Returns: std::map<int, std::string> mapping symbol index -> binary codeword.

inline std::map<int, std::string> tree_to_codebook(const Node* root,
                                                    std::string prefix = "") {
    assert(root != nullptr && "tree_to_codebook called with null root");

    // Special case: single-symbol alphabet (root is a leaf).
    if (root->symbol >= 0 && root->left == nullptr && root->right == nullptr) {
        // A single-symbol code needs at least 1 bit. Assign "0".
        return {{root->symbol, prefix.empty() ? "0" : prefix}};
    }

    std::map<int, std::string> result;

    // Recursive DFS: left child gets "0", right child gets "1".
    if (root->left) {
        auto left_codes = tree_to_codebook(root->left.get(), prefix + "0");
        result.insert(left_codes.begin(), left_codes.end());
    }
    if (root->right) {
        auto right_codes = tree_to_codebook(root->right.get(), prefix + "1");
        result.insert(right_codes.begin(), right_codes.end());
    }

    return result;
}

// ---- encode -- write a symbol's Huffman codeword into a BitSink -------------
//
// Looks up the symbol in the codebook and writes each bit ('0' -> false,
// '1' -> true) into sink. Asserts that the symbol is present in the codebook.

template<BitSink S>
inline void encode(int symbol,
                   const std::map<int, std::string>& codebook,
                   S& sink) {
    auto it = codebook.find(symbol);
    assert(it != codebook.end() && "encode: symbol not in codebook");
    for (char c : it->second) {
        sink.write(c == '1');
    }
}

// ---- decode -- walk the Huffman tree and return the symbol at the leaf -------
//
// Reads bits from source, going left on 0 and right on 1, until a leaf is
// reached. Returns the leaf's symbol index.
// Asserts that the tree is well-formed (every internal node has two children).

template<BitSource S>
inline int decode(const Node* root, S& source) {
    assert(root != nullptr && "decode called with null root");
    const Node* cur = root;
    while (cur->symbol < 0) {
        // Internal node: consume one bit.
        bool bit = source.read();
        if (bit) {
            assert(cur->right && "decode: null right child on internal node");
            cur = cur->right.get();
        } else {
            assert(cur->left && "decode: null left child on internal node");
            cur = cur->left.get();
        }
    }
    return cur->symbol;
}

}  // namespace huffman
