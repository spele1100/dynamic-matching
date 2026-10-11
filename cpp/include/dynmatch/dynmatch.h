// dynmatch.h — DynamicMatching public API (C++ port of dynamic.py).
//
// Semantics 1:1 with dynamic.py:
//   * insert of a present edge: no-op (zero-work report)
//   * delete of an absent edge: no-op
//   * insert(u, u): throws std::invalid_argument (self-loop rejected)
//   * out-of-range vertices: throw std::out_of_range
//   * parallel edges deduplicated; eager exact repair after every update
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "dynmatch/forest.h"
#include "dynmatch/graph.h"

namespace dm {

struct RepairReport {
    const char* op = "noop";     // "insert" | "delete" | "noop"
    int32_t u = -1, v = -1;      // normalized u < v
    bool searched = false;       // any blossom search was run
    bool augmented = false;      // |M| increased by this update
    bool nu_dropped = false;     // delete: freed edge could not be re-augmented
    int64_t work = 0;            // adjacency slots scanned by searches
    int32_t size = 0;            // |M| after the update
};

struct Stats {
    int64_t updates = 0;
    int64_t searches = 0;
    int64_t augmentations = 0;
    int64_t work = 0;
};

class DynamicMatching {
public:
    explicit DynamicMatching(int32_t n);

    RepairReport insert(int32_t u, int32_t v);
    RepairReport remove(int32_t u, int32_t v);  // `delete` is a C++ keyword

    void insert_many(const std::vector<std::pair<int32_t, int32_t>>& edges);
    void delete_many(const std::vector<std::pair<int32_t, int32_t>>& edges);

    std::vector<std::pair<int32_t, int32_t>> matching() const;
    int32_t size() const { return size_; }
    int64_t num_edges() const { return g_.num_edges(); }
    bool is_valid() const;
    std::vector<int32_t> free_vertices() const;

    const int32_t* mate_array() const { return mate_.data(); }
    int32_t n() const { return g_.n(); }
    Stats stats;

private:
    void check(int32_t u, int32_t v) const;
    int64_t search(const int32_t* roots, int32_t nroots);

    Graph g_;
    std::vector<int32_t> mate_;
    int32_t size_ = 0;
    ForestScratch fs_;
    std::vector<int32_t> roots_;  // reused free-vertex buffer (no alloc in loop)
};

}  // namespace dm
