#!/usr/bin/env python3
"""Benchmark: cdyn (C++ pybind11 core) vs dynamic.py (pure Python) — direct A/B.

Mirrors benchmarks/bench_dynamic.py exactly: same graph builder, same workload
generator, same seed, same config defaults (n=2000, m=4n, 500 updates, seed
12345). Both implementations run the IDENTICAL op sequence; churn and final
sizes must agree (A/B sanity), times are compared.

Run:
    python -m benchmarks.bench_cdyn                # n=2000 (same as bench_dynamic)
    python -m benchmarks.bench_cdyn --n 500
    python -m benchmarks.bench_cdyn --markdown
"""

import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cdyn import DynamicMatching as CDyn
from dynamic import DynamicMatching as PyDyn
from benchmarks.bench_dynamic import build_edges, make_workload


def time_ops_only(cls, n, edges, ops):
    """Core update cost: same ops, no per-op matching() churn capture."""
    dm = cls(n)
    for u, v in edges:
        dm.insert(u, v)
    t0 = time.perf_counter()
    for op, e in ops:
        if op == "i":
            dm.insert(*e)
        else:
            dm.delete(*e)
    return (time.perf_counter() - t0) / len(ops) * 1e6


def run_one(cls, n, edges, ops):
    dm = cls(n)
    t0 = time.perf_counter()
    for u, v in edges:
        dm.insert(u, v)
    build_time = time.perf_counter() - t0

    churn = []
    sizes = []
    t0 = time.perf_counter()
    s0 = dict(dm.stats)
    for op, e in ops:
        before = set(dm.matching())
        if op == "i":
            dm.insert(*e)
        else:
            dm.delete(*e)
        after = set(dm.matching())
        churn.append(len(before ^ after))
        sizes.append(dm.size())
    run_time = time.perf_counter() - t0
    s1 = dm.stats
    return {
        "build_time": build_time,
        "time": run_time,
        "us": run_time / len(ops) * 1e6,
        "build_us": build_time / max(len(edges), 1) * 1e6,
        "churn_mean": sum(churn) / len(churn),
        "churn_max": max(churn),
        "searches": s1["searches"] - s0["searches"],
        "augmentations": s1["augmentations"] - s0["augmentations"],
        "final_size": dm.size(),
        "sizes": sizes,
    }


def run_benchmark(n, m_target, updates, seed):
    rng = random.Random(seed)
    edges = build_edges(n, m_target, rng)
    ops = make_workload(n, edges, m_target, updates, rng)
    py = run_one(PyDyn, n, edges, ops)
    cc = run_one(CDyn, n, edges, ops)
    py_core_us = time_ops_only(PyDyn, n, edges, ops)
    cc_core_us = time_ops_only(CDyn, n, edges, ops)
    # A/B sanity: identical semantics on identical workload. The specific
    # matching can differ (maximum matching is not unique), but sizes and
    # per-op size-delta distribution (nu trajectory) must agree.
    assert py["final_size"] == cc["final_size"], "size divergence!"
    assert py["sizes"] == cc["sizes"], "per-op size trajectory divergence!"
    return {"n": n, "m": len(edges), "updates": len(ops), "seed": seed,
            "py": py, "cc": cc,
            "py_core_us": py_core_us, "cc_core_us": cc_core_us,
            "core_speedup": py_core_us / cc_core_us,
            "speedup": py["time"] / cc["time"]}


def print_text(r):
    print(f"graph: n={r['n']}, m={r['m']} (initial), {r['updates']} updates, "
          f"seed={r['seed']}  (identical ops for both)")
    for label, x in (("dynamic.py (pure Python)", r["py"]), ("cdyn (C++ core)", r["cc"])):
        print()
        print(f"--- {label} ---")
        print(f"initial build: {x['build_time']*1000:.0f} ms total "
              f"({x['build_us']:.0f} us/insert)")
        print(f"update phase:  {x['time']:.2f} s total, {x['us']:.0f} us/update")
        print(f"churn: {x['churn_mean']:.2f} mean, {x['churn_max']} max; "
              f"searches {x['searches']}, augmentations {x['augmentations']}")
        print(f"final matching size: {x['final_size']}")
    print()
    print(f"A/B speedup (update phase incl. churn capture, py/cc): {r['speedup']:.1f}x")
    print(f"core ops-only: py {r['py_core_us']:.0f} us/update vs "
          f"cdyn {r['cc_core_us']:.0f} us/update -> {r['core_speedup']:.1f}x")


def print_markdown(r):
    print(f"n={r['n']}, m={r['m']}, {r['updates']} updates, seed={r['seed']}")
    print()
    print("| implementation | build µs/insert | mean µs/update | total s | churn mean | searches |")
    print("|---|---|---|---|---|---|")
    for label, x in (("dynamic.py (pure Python)", r["py"]), ("cdyn (C++ core)", r["cc"])):
        print(f"| {label} | {x['build_us']:.0f} | {x['us']:.0f} | {x['time']:.2f} | "
              f"{x['churn_mean']:.2f} | {x['searches']} |")
    print()
    print(f"A/B speedup ≈ {r['speedup']:.1f}× (bench loop incl. churn capture); "
          f"core ops-only {r['core_speedup']:.1f}× "
          f"(py {r['py_core_us']:.0f} vs cdyn {r['cc_core_us']:.0f} us/update); "
          f"final size both = {r['cc']['final_size']}")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--n", type=int, default=2000, help="vertices (default 2000)")
    ap.add_argument("--m", type=int, default=None,
                    help="target edges (default 4*n)")
    ap.add_argument("--updates", type=int, default=500,
                    help="update operations (default 500)")
    ap.add_argument("--seed", type=int, default=12345)
    ap.add_argument("--markdown", action="store_true",
                    help="print results as a markdown table")
    args = ap.parse_args(argv)

    m = args.m if args.m is not None else 4 * args.n
    r = run_benchmark(args.n, m, args.updates, args.seed)
    if args.markdown:
        print_markdown(r)
    else:
        print_text(r)


if __name__ == "__main__":
    main()
