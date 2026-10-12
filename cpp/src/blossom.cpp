// blossom.cpp — faithful port of dynamic.py::DynamicMatching._search.
//
// Line-by-line correspondence with the Python (kept in comments). The
// single-root kernel structure is the classical e-maxx implementation; the
// multi-root extension (tree ids, cross-tree even-even pair-flip) follows
// dynamic.py exactly. Any behavioral deviation here is a bug.
#include "dynmatch/blossom.h"

#include <cstring>

#include "dynmatch/simd.h"

namespace dm {

int64_t blossom_search(const Graph& g, int32_t* mate, const int32_t* roots,
                       int32_t nroots, int32_t* size, ForestScratch& fs) {
    const int32_t n = g.n();
    auto& p = fs.p;
    auto& base = fs.base;
    auto& used = fs.used;
    auto& tree = fs.tree;
    auto& blossom = fs.blossom;

    fs.begin_search(n);
    auto& q = fs.q;
    int32_t head = 0;
    for (int32_t i = 0; i < nroots; ++i) {
        const int32_t r = roots[i];
        used[r] = 1;
        tree[r] = r;
        q.push_back(r);
    }
    int64_t work = 0;

    // lca(a, b): a and b are guaranteed to be in the same tree.
    // Version-stamped marks replace Python's per-call `[False]*n` allocation
    // (same semantics: marks visible only within this LCA call).
    auto lca = [&](int32_t a, int32_t b) -> int32_t {
        const int32_t stamp = ++fs.lca_stamp;
        while (true) {
            a = base[a];
            fs.lca_mark[a] = stamp;
            if (mate[a] < 0) break;
            a = p[mate[a]];
        }
        while (true) {
            b = base[b];
            if (fs.lca_mark[b] == stamp) return b;
            b = p[mate[b]];
        }
    };

    auto mark_path = [&](int32_t v, int32_t b, int32_t x) {
        while (base[v] != b) {
            blossom[base[v]] = 1;
            blossom[base[mate[v]]] = 1;
            p[v] = x;
            x = mate[v];
            v = p[x];
        }
    };

    // walk_up(x): vertex path from even forest vertex x to its free root.
    // Positional parity (even->mate, odd->p), NOT used[].
    auto walk_up = [&](int32_t x, std::vector<int32_t>& path) {
        path.clear();
        path.push_back(x);
        int32_t cur = x;
        bool even = true;
        while (true) {
            if (even) {
                if (mate[cur] == -1) return;  // cur is the free root
                cur = mate[cur];
            } else {
                cur = p[cur];
            }
            path.push_back(cur);
            even = !even;
        }
    };

    // flip_walk(u): classical e-maxx augmenting flip from free odd vertex u.
    auto flip_walk = [&](int32_t u) {
        int32_t cur = u;
        while (cur != -1) {
            const int32_t prev = p[cur];
            const int32_t nxt = prev >= 0 ? mate[prev] : -1;
            mate[cur] = prev;
            if (prev >= 0) mate[prev] = cur;
            cur = nxt;
        }
    };

    // cross_flip(v, u): even-even edge across trees: augment rootA..v-u..rootB.
    auto cross_flip = [&](int32_t v, int32_t u) {
        walk_up(v, fs.path_a);   // v ... rootA
        walk_up(u, fs.path_b);   // u ... rootB
        fs.full.clear();
        for (size_t i = fs.path_a.size(); i-- > 0;)  // reversed: rootA ... v
            fs.full.push_back(fs.path_a[i]);
        fs.full.insert(fs.full.end(), fs.path_b.begin(), fs.path_b.end());
        for (size_t i = 0; i + 1 < fs.full.size(); i += 2) {
            const int32_t a = fs.full[i], b = fs.full[i + 1];
            mate[a] = b;
            mate[b] = a;
        }
    };

    auto contract = [&](int32_t v, int32_t u) {
        const int32_t curbase = lca(v, u);
        simd::reset_marks(blossom.data(), static_cast<size_t>(n));  // for i in range(n): blossom[i] = False
        mark_path(v, curbase, u);
        mark_path(u, curbase, v);
        for (int32_t i = 0; i < n; ++i) {
            if (blossom[base[i]]) {
                base[i] = curbase;
                if (!used[i]) {
                    used[i] = 1;
                    q.push_back(i);
                }
            }
        }
    };

    while (head < static_cast<int32_t>(q.size())) {
        const int32_t v = q[head++];
        const int32_t tv = tree[v];
        const int32_t dv = g.degree(v);
        const int32_t* nbr = g.neighbors(v);
        for (int32_t i = 0; i < dv; ++i) {
            const int32_t u = nbr[i];
            ++work;
            if (base[v] == base[u] || mate[v] == u) continue;
            if (!used[u] && p[u] < 0) {
                // not in forest: discover as odd child of v
                p[u] = v;
                tree[u] = tv;
                if (mate[u] < 0) {
                    flip_walk(u);               // single-tree augmenting path
                    ++*size;
                    return work;
                }
                used[mate[u]] = 1;
                tree[mate[u]] = tv;
                q.push_back(mate[u]);
            } else if (used[u]) {
                // u is even in some tree
                if (tree[u] != tv) {
                    cross_flip(v, u);           // augmenting path across trees
                    ++*size;
                    return work;
                }
                if (mate[u] == -1 || p[mate[u]] >= 0) {
                    // same tree, even u: blossom (u==root case included)
                    contract(v, u);
                }
            } else {
                // u is odd in some tree (p[u] >= 0)
                if (tree[u] == tv && mate[u] >= 0 && p[mate[u]] >= 0) {
                    // same tree, repointed mate: blossom (e-maxx rule)
                    contract(v, u);
                }
                // otherwise inert: even-odd edge (same or different tree)
            }
        }
    }
    return work;
}

}  // namespace dm
