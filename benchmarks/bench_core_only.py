#!/usr/bin/env python3
"""Core-ops-only A/B for cdyn (no churn capture) — used for the NEON on/off
delta and per-platform cdyn core numbers. Same workload as bench_cdyn."""
import os, sys, random, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cdyn import DynamicMatching as CDyn
from benchmarks.bench_dynamic import build_edges, make_workload

def core_us(n, edges, ops, reps=5):
    best = None
    for _ in range(reps):
        dm = CDyn(n)
        for u, v in edges:
            dm.insert(u, v)
        t0 = time.perf_counter()
        for op, e in ops:
            if op == "i":
                dm.insert(*e)
            else:
                dm.delete(*e)
        dt = (time.perf_counter() - t0) / len(ops) * 1e6
        best = dt if best is None else min(best, dt)
    return best

n = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
rng = random.Random(12345)
edges = build_edges(n, 4 * n, rng)
ops = make_workload(n, edges, 4 * n, 500, rng)
us = core_us(n, edges, ops)
print(f"cdyn core ops: {us:.2f} us/update  (n={n}, best of 5, {__import__('platform').machine()})")
