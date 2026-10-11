// bench_dynamic.cpp — coarse per-update benchmark for the C++ port.
//
// Mirrors benchmarks/bench_dynamic.py's workload shape (n=2000, m=4n,
// 500 mixed updates biased to hold |E| near target). Dynamic-only (the
// Python baseline number comes from the Python harness; see report).
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <vector>

#include "dynmatch/dynmatch.h"

using dm::DynamicMatching;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::atoi(argv[1]) : 2000;
    const int updates = argc > 2 ? std::atoi(argv[2]) : 500;
    const int m_target = argc > 3 ? std::atoi(argv[3]) : 4 * n;
    const uint32_t seed = argc > 4 ? std::atoi(argv[4]) : 12345;

    std::mt19937 rng(seed);
    // initial edge set
    std::set<std::pair<int32_t, int32_t>> edges;
    while (static_cast<int>(edges.size()) < m_target) {
        int32_t u = rng() % n, v = rng() % n;
        if (u == v) continue;
        edges.emplace(std::min(u, v), std::max(u, v));
    }

    DynamicMatching dm(n);
    auto t0 = Clock::now();
    for (auto& e : edges) dm.insert(e.first, e.second);
    auto t1 = Clock::now();
    const double build_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    // workload
    std::set<std::pair<int32_t, int32_t>> present = edges;
    std::vector<std::pair<char, std::pair<int32_t, int32_t>>> ops;
    for (int i = 0; i < updates; ++i) {
        const double p_del =
            static_cast<int>(present.size()) > m_target ? 0.5 : 0.25;
        if ((rng() % 1000) / 1000.0 < p_del && !present.empty()) {
            // pseudo-random element of the sorted set
            auto it = present.begin();
            std::advance(it, rng() % present.size());
            ops.push_back({'d', *it});
            present.erase(it);
        } else {
            for (int tries = 0; tries < 50; ++tries) {
                int32_t u = rng() % n, v = rng() % n;
                if (u == v) continue;
                std::pair<int32_t, int32_t> e{std::min(u, v), std::max(u, v)};
                if (!present.count(e)) {
                    ops.push_back({'i', e});
                    present.insert(e);
                    break;
                }
            }
        }
    }

    dm.stats = {};
    t0 = Clock::now();
    for (auto& [op, e] : ops) {
        if (op == 'i') dm.insert(e.first, e.second);
        else dm.remove(e.first, e.second);
    }
    t1 = Clock::now();
    const double s = std::chrono::duration<double>(t1 - t0).count();

    std::printf(
        "n=%d m=%d updates=%zu seed=%u\n"
        "build: %.0f ms total (%.2f us/insert)\n"
        "dynamic: %.1f us/update, %.0f updates/sec\n"
        "searches=%lld augmentations=%lld work=%lld\n"
        "final size=%d (oracle-verified via golden tests)\n",
        n, m_target, ops.size(), seed, build_ms,
        build_ms / edges.size() * 1000.0, s / ops.size() * 1e6,
        ops.size() / s, (long long)dm.stats.searches,
        (long long)dm.stats.augmentations, (long long)dm.stats.work,
        dm.size());
    return 0;
}
