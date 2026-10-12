#!/usr/bin/env python3
"""Generate cross-language golden fixtures for the C++ port (M1 gate).

For each seed: build a deterministic random insert/delete workload with
python's random.Random(seed), record every op together with the matching size
after the op as computed by BOTH dynamic.py and networkx (asserting they agree
— this is the Python-side differential). The C++ test replays the ops and must
reproduce the identical size after every op (cross-language parity).

Output: cpp/tests/golden/seed<k>.txt, one op per line:
    i <u> <v> <size_after>      or      d <u> <v> <size_after>

Run from repo root:  python3 cpp/tests/gen_golden.py
"""

import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

import networkx as nx

from dynamic import DynamicMatching

N = 30
STEPS = 300
SEEDS = 5


def gen(seed):
    rng = random.Random(seed)
    dm = DynamicMatching(N)
    g = nx.Graph()
    g.add_nodes_from(range(N))
    present = set()
    lines = []
    for _ in range(STEPS):
        p_insert = 0.5 if not present else min(0.9, max(0.1, 1.0 - len(present) / (0.2 * N * N)))
        if rng.random() < p_insert:
            for _ in range(50):
                u, v = rng.randrange(N), rng.randrange(N)
                if u != v and (min(u, v), max(u, v)) not in present:
                    break
            else:
                continue
            e = (min(u, v), max(u, v))
            dm.insert(*e)
            g.add_edge(*e)
            present.add(e)
        else:
            if not present:
                continue
            e = rng.choice(sorted(present))
            dm.delete(*e)
            g.remove_edge(*e)
            present.discard(e)
        oracle = len(nx.max_weight_matching(g, maxcardinality=True))
        assert oracle == dm.size(), f"seed {seed}: python/oracle mismatch {dm.size()} != {oracle}"
        lines.append(f"{'i' if e in present else 'd'} {e[0]} {e[1]} {oracle}")
    return lines


def main():
    outdir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "golden")
    os.makedirs(outdir, exist_ok=True)
    for seed in range(SEEDS):
        lines = gen(seed)
        path = os.path.join(outdir, f"seed{seed}.txt")
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")
        print(f"{path}: {len(lines)} ops")


if __name__ == "__main__":
    main()
