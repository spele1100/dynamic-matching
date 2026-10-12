// forest.h — multi-root alternating-forest scratch for the blossom search.
//
// Mirrors the per-search arrays of dynamic.py::_search, but allocated ONCE on
// the DynamicMatching object and batch-reset between searches (fill-based,
// SIMD-friendly — see simd.h). One tree id per vertex, positional parity is
// tracked by the search itself (even = `used`, odd = discovered-but-not-used).
#pragma once

#include <cstdint>
#include <numeric>
#include <vector>

namespace dm {

struct ForestScratch {
    void reset_for(int32_t n) {
        p.assign(n, -1);
        base.resize(n);
        used.assign(n, 0);
        tree.assign(n, -1);
        blossom.assign(n, 0);
        lca_mark.assign(n, -1);
        q.clear();
        q.reserve(n);
        path_a.clear();
        path_b.clear();
        full.clear();
        lca_stamp = -1;
    }

    void begin_search(int32_t n) {
        // batch reset (SoA fills; these are the reset hot spots the SIMD shim
        // targets — std::fill/vector::assign compile to the same vectorized
        // memset on both x86-64 and aarch64, and simd::reset_marks covers the
        // per-contraction blossom reset inside the search)
        p.assign(n, -1);
        std::iota(base.begin(), base.end(), 0);
        used.assign(n, 0);
        tree.assign(n, -1);
        blossom.assign(n, 0);
        q.clear();
    }

    // Forest state (per vertex):
    std::vector<int32_t> p;        // parent (unmatched forest edge), -1 = none
    std::vector<int32_t> base;     // blossom base labels
    std::vector<uint8_t> used;     // 1 = even/outer forest member
    std::vector<int32_t> tree;     // root id of the tree owning this vertex
    std::vector<uint8_t> blossom;  // scratch marks for contraction
    std::vector<int32_t> lca_mark; // version-stamped marks (avoids O(n) alloc/clear per LCA)
    int32_t lca_stamp = -1;

    // BFS queue (plain vector + head cursor; no deque, no alloc in loop)
    std::vector<int32_t> q;

    // Path scratch for cross-tree pair-flip
    std::vector<int32_t> path_a, path_b, full;
};

}  // namespace dm
