// graph.h — flat CSR-style adjacency for the dynamic-matching C++ port.
//
// Design (M1 requirement: SIMD-friendly layout, zero malloc in the hot path):
//   * All neighbor ids live in ONE contiguous int32 arena (SoA-friendly,
//     cache-friendly, SIMD-scannable in blocks).
//   * Vertex v owns the block arena_[offset_[v] .. offset_[v] + deg_[v]).
//     Blocks are kept SORTED by vertex id, so membership scans and duplicate
//     detection are lower-bound scans (see simd::scan_index_i32).
//   * Edge insert/delete rebuilds only the two touched blocks by appending
//     fresh copies at the arena tail; the old blocks become dead space.
//     The arena is compacted when dead space exceeds live size (amortized,
//     happens only between searches — never inside one).
//   * Searches only read through neighbors(v)/degree(v): no allocation at all
//     in the search hot path.
//
// This mirrors dynamic.py's per-vertex dict (deduped logical edges) with
// iteration order = sorted (Python's was insertion order; iteration order can
// differ, but only affects WHICH maximum matching is found, never its size).
#pragma once

#include <cstdint>
#include <vector>

#include "dynmatch/simd.h"

namespace dm {

class Graph {
public:
    explicit Graph(int32_t n) : n_(n), offset_(static_cast<size_t>(n), 0),
                                deg_(static_cast<size_t>(n), 0) {}

    int32_t n() const { return n_; }
    int32_t degree(int32_t v) const { return deg_[v]; }
    int64_t num_edges() const { return m_; }

    const int32_t* neighbors(int32_t v) const {
        return arena_.data() + offset_[v];
    }

    bool has_edge(int32_t u, int32_t v) const {
        return simd::scan_index_i32(neighbors(u), deg_[u], v) >= 0;
    }

    // Returns true if the edge was inserted (was absent), false if duplicate.
    bool insert_edge(int32_t u, int32_t v) {
        if (has_edge(u, v)) return false;
        mutate_block(u, v, /*adding=*/true);
        mutate_block(v, u, /*adding=*/true);
        ++m_;
        return true;
    }

    // Returns true if the edge was deleted (was present), false if absent.
    bool delete_edge(int32_t u, int32_t v) {
        if (!has_edge(u, v)) return false;
        mutate_block(u, v, /*adding=*/false);
        mutate_block(v, u, /*adding=*/false);
        --m_;
        return true;
    }

    // Test/debug hooks.
    size_t arena_live() const { return static_cast<size_t>(live_); }
    size_t arena_size() const { return arena_.size(); }

private:
    // Append a fresh sorted copy of v's block, plus (adding) or minus
    // (not adding) the other endpoint `x`, then repoint offset_[v].
    void mutate_block(int32_t v, int32_t x, bool adding) {
        const int32_t d = deg_[v];
        const int32_t nd = d + (adding ? 1 : -1);

        ensure_capacity(static_cast<size_t>(nd));
        const int32_t* src = arena_.data() + offset_[v];  // after ensure (no realloc)
        size_t old_size = arena_.size();
        arena_.resize(old_size + static_cast<size_t>(nd));
        int32_t* dst = arena_.data() + old_size;

        int32_t i = 0, j = 0;
        bool placed = false;
        if (adding) {
            while (i < d || !placed) {
                if (!placed && (i >= d || x < src[i])) {
                    dst[j++] = x;
                    placed = true;
                    continue;
                }
                dst[j++] = src[i++];
            }
        } else {
            for (int32_t k = 0; k < d; ++k)
                if (src[k] != x) dst[j++] = src[k];
        }
        (void)j;  // j == nd
        offset_[v] = static_cast<int32_t>(old_size);
        deg_[v] = nd;
        live_ += (adding ? 1 : -1);
        maybe_compact();
    }

    void ensure_capacity(size_t extra) {
        if (arena_.size() + extra > arena_.capacity())
            arena_.reserve((arena_.size() + extra) * 2 + 64);
    }

    // Rebuild the arena contiguously when dead space dominates.
    void maybe_compact() {
        if (arena_.size() <= 2 * static_cast<size_t>(live_) + 4096) return;
        std::vector<int32_t> fresh;
        fresh.reserve(static_cast<size_t>(live_) + 64);
        std::vector<int32_t> new_off(offset_.size());
        for (int32_t v = 0; v < n_; ++v) {
            new_off[v] = static_cast<int32_t>(fresh.size());
            const int32_t* src = arena_.data() + offset_[v];
            fresh.insert(fresh.end(), src, src + deg_[v]);
        }
        arena_.swap(fresh);
        offset_.swap(new_off);
    }

    int32_t n_;
    int64_t m_ = 0;
    int64_t live_ = 0;  // total live slots = sum(deg)
    std::vector<int32_t> arena_;
    std::vector<int32_t> offset_;  // int32 offsets: compaction bounds growth
    std::vector<int32_t> deg_;
};

}  // namespace dm
