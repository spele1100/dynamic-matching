// dynmatch.cpp — DynamicMatching API implementation (port of dynamic.py).
#include "dynmatch/dynmatch.h"

#include <algorithm>
#include <stdexcept>

#include "dynmatch/blossom.h"

namespace dm {

DynamicMatching::DynamicMatching(int32_t n)
    : g_(n < 0 ? throw std::invalid_argument("n must be >= 0") : n),
      mate_(static_cast<size_t>(n < 0 ? 0 : n), -1) {
    fs_.reset_for(g_.n());
    roots_.reserve(n);
}

void DynamicMatching::check(int32_t u, int32_t v) const {
    if (u == v) throw std::invalid_argument("self-loop rejected");
    if (u < 0 || u >= g_.n() || v < 0 || v >= g_.n())
        throw std::out_of_range("vertex out of range");
}

RepairReport DynamicMatching::insert(int32_t u, int32_t v) {
    check(u, v);
    if (u > v) std::swap(u, v);
    RepairReport rep;
    rep.u = u;
    rep.v = v;
    rep.size = size_;
    if (g_.has_edge(u, v)) {
        rep.op = "noop";
        return rep;
    }
    g_.insert_edge(u, v);
    ++stats.updates;
    rep.op = "insert";
    const int32_t mu = mate_[u], mv = mate_[v];
    if (mu == -1 && mv == -1) {
        // both free: matching the new edge is always optimal (Lemma A). No search.
        mate_[u] = v;
        mate_[v] = u;
        ++size_;
        rep.augmented = true;
        rep.size = size_;
        return rep;
    }
    if (mu == -1 || mv == -1) {
        // one free endpoint: every augmenting path must start there.
        const int32_t root = (mu == -1) ? u : v;
        rep.searched = true;
        rep.work = search(&root, 1);
        rep.augmented = size_ > rep.size;
        rep.size = size_;
        return rep;
    }
    // both matched: one multi-root search over all free vertices
    roots_.clear();
    for (int32_t r = 0; r < g_.n(); ++r)
        if (mate_[r] == -1) roots_.push_back(r);
    rep.searched = true;
    rep.work = search(roots_.data(), static_cast<int32_t>(roots_.size()));
    rep.augmented = size_ > rep.size;
    rep.size = size_;
    return rep;
}

RepairReport DynamicMatching::remove(int32_t u, int32_t v) {
    check(u, v);
    if (u > v) std::swap(u, v);
    RepairReport rep;
    rep.u = u;
    rep.v = v;
    rep.size = size_;
    if (!g_.has_edge(u, v)) {
        rep.op = "noop";
        return rep;
    }
    g_.delete_edge(u, v);
    ++stats.updates;
    rep.op = "delete";
    if (mate_[u] != v) {
        // unmatched edge: zero work, still maximum (Lemma A)
        return rep;
    }
    // matched edge: free both endpoints; every augmenting path ends at u or v
    mate_[u] = -1;
    mate_[v] = -1;
    --size_;
    const int32_t roots[2] = {u, v};
    rep.searched = true;
    rep.work = search(roots, 2);
    rep.augmented = size_ > rep.size - 1;
    rep.nu_dropped = (size_ == rep.size - 1);
    rep.size = size_;
    return rep;
}

void DynamicMatching::insert_many(
    const std::vector<std::pair<int32_t, int32_t>>& edges) {
    for (const auto& e : edges) insert(e.first, e.second);
}

void DynamicMatching::delete_many(
    const std::vector<std::pair<int32_t, int32_t>>& edges) {
    for (const auto& e : edges) remove(e.first, e.second);
}

std::vector<std::pair<int32_t, int32_t>> DynamicMatching::matching() const {
    std::vector<std::pair<int32_t, int32_t>> out;
    for (int32_t v = 0; v < g_.n(); ++v) {
        const int32_t w = mate_[v];
        if (w != -1 && v < w) out.emplace_back(v, w);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool DynamicMatching::is_valid() const {
    for (int32_t v = 0; v < g_.n(); ++v) {
        const int32_t w = mate_[v];
        if (w == -1) continue;
        if (w < 0 || w >= g_.n() || mate_[w] != v || !g_.has_edge(v, w))
            return false;
    }
    return true;
}

std::vector<int32_t> DynamicMatching::free_vertices() const {
    std::vector<int32_t> out;
    for (int32_t v = 0; v < g_.n(); ++v)
        if (mate_[v] == -1) out.push_back(v);
    return out;
}

int64_t DynamicMatching::search(const int32_t* roots, int32_t nroots) {
    ++stats.searches;
    const int64_t work =
        blossom_search(g_, mate_.data(), roots, nroots, &size_, fs_);
    stats.work += work;
    return work;
}

}  // namespace dm
