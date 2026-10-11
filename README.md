# Dynamic-Matching: Exact Maximum Matching Under Edge Updates

**Maintain an exact maximum-cardinality matching of a general (non-bipartite)
graph under edge insertions and deletions — without recomputing from scratch.**

If your graph *changes over time*, every mainstream library makes you re-solve
the whole problem per update: NetworkX, LEMON and Boost.Graph all re-run a full
blossom computation per call, and the closest research code (DynMatch) settles
for a 2-approximation. As far as we know, no practical open-source library
offers an *exact* dynamic maximum matcher for general graphs. This one does —
one blossom search per update instead of a full recompute, and the matching
stays provably maximum after **every** operation.

## Origin

This library grew out of [reverse-matching](https://github.com/spele1100/reverse-matching),
a correctness-only reference implementation reverse-engineered from OpenAI's
`/math` release — a collection of 719 research-level mathematics manuscripts
(including an almost-linear-time maximum-matching algorithm) published with no
accompanying code. While working with that codebase, the need for a practical
dynamic matcher exposed the gap above, and this tool is the result: the blossom
search at its core is battle-tested e-maxx lineage, extended to a multi-root
alternating forest so a single update needs exactly one search.

## Install & Quickstart

Pure Python, standard library only (networkx is needed only for tests and
benchmarks). Python 3.10+.

```python
from dynamic import DynamicMatching

dm = DynamicMatching(n=6)
dm.insert(0, 1)          # both free -> matched instantly, no search
dm.insert(1, 2)          # one free endpoint: one rooted search if needed
dm.insert(2, 4)          # arbitrary: one blossom search over free roots
dm.delete(0, 1)          # unmatched delete: zero work; matched delete: repair search
dm.size()                # |M| now
dm.matching()            # sorted list of (u, v) pairs — always a valid matching
dm.is_valid()            # invariant self-check (mate symmetric, edges exist)
dm.oracle_check()        # test-only: differential check vs networkx
```

## Guarantees

Proofs in the `dynamic.py` module docstring.

- **Maximum after every update** — eager and exact, never approximate.
- Deleting an unmatched edge costs **O(1)** (the matching stays maximum).
- Every other update performs **at most one blossom search**, O(m) worst case,
  versus O(n·m) for a full Edmonds recompute — an O(n) factor.
- **Stability**: repairs flip one short augmenting path. Full recomputation can
  rewrite the entire matching every update (see churn below) — if downstream
  consumers depend on assignment stability, that churn is a cost no amount of
  raw speed fixes.
- Insert of a present edge / delete of an absent edge are no-ops; self-loops
  raise `ValueError`; parallel edges are deduplicated.

The repair correctness is the classical pair: ν changes by at most 1 per single
edge update, and by Berge's theorem one augmenting-path search restores
maximality — a matched-edge deletion may legitimately shrink ν when no
re-augmentation exists; the search decides.

## Design

The search kernel is a faithful Python port of the battle-tested e-maxx /
cp-algorithms blossom implementation, extended to a **multi-root alternating
forest**:

- each insert performs exactly one dispatch decision: match directly, one
  rooted search, or one multi-root search over all free vertices;
- a matched-edge delete frees two endpoints and runs one two-root search;
- an edge joining even vertices of *different* trees closes an augmenting path
  (concatenated alternating paths through both roots); even–odd cross-tree
  edges are provably inert and skipped.

**One search per update** is the whole trick — everything else is bookkeeping.

## Correctness certification

`tests_dynamic.py` is a differential harness: after *every* operation of
hundreds of randomized update sequences (n up to 200, densities 0.05–0.5, plus
glued-triangle / K₂ₜ₊₁-minus-matching / Petersen blossom gauntlets and
adversarial churn workloads), the matching size must equal NetworkX's oracle
and the matching must be valid. 39 tests, per-operation oracle checks.

```bash
python -m pytest tests_dynamic.py -q
```

## Benchmarks

`python -m benchmarks.bench_dynamic` — pure Python vs pure Python, seed 12345.
The baseline harness re-solves before *and* after each update (to measure churn
on both sides), so per-update speedup against a *single* recompute is ≈ 2× the
figure shown.

| n | m | approach | mean µs/update | churn mean | churn max |
|---|---|---|---|---|---|
| 2000 | 8000 | **DynamicMatching (ours)** | **973** | **0.46** | 12 |
| 2000 | 8000 | networkx full recompute | 3,707,425 | 236.02 | 884 |
| 500 | 2000 | **DynamicMatching (ours)** | **1037** | **0.47** | 14 |
| 500 | 2000 | networkx full recompute | 269,097 | 60.12 | 292 |

- Speedup ≈ **3,810× per update** at n=2000 (≈ **1,900×** against a single
  recompute), ≈ **260×** at n=500.
- Search cost: 0.58 blossom searches per update on average.
- **Churn is the headline**: recompute rewrites ~236 of ~1,000 matched edges per
  update on average (up to 884); repair touches fewer than 1 on average.

### C++ core (`cdyn`) — M2/M3

**Install (branch `cpp-port` only):** `pip install .` builds the `cdyn`
extension via CMake + scikit-build-core (needs a C++17 compiler and CMake;
`dynamic.py` remains importable pure-Python from the repo root regardless).

**API parity:** `cdyn.DynamicMatching` is a drop-in for `dynamic.DynamicMatching`
— same constructor, same method surface (`insert`, `delete`, `size`, `matching`,
`is_valid`, `free_vertices`, `num_edges`, `oracle_check`), same semantics,
including no-op duplicate inserts, `ValueError` on self-loops, and edge
deduplication.

**Benchmarks:** `python -m benchmarks.bench_cdyn` — cdyn (pybind11 C++17 core)
vs `dynamic.py`, identical op sequence (n=2000, m=8000, 500 updates, seed
12345). Core ops-only = update ops timed without per-op churn capture (the
honest algorithm cost).

| platform | implementation | core µs/update | notes |
|---|---|---|---|
| x86-64 (VPS) | dynamic.py | 98 | M2 reference host |
| x86-64 (VPS) | cdyn (SSE2 auto-vec) | 3 | 33× vs Python |
| RK3588 A76 @ ~2.4GHz | dynamic.py | 153 | big cores pinned (`taskset -c 4-7`) |
| RK3588 A76 @ ~2.4GHz | cdyn (NEON) | 3.4–3.7 | 40× vs Python; ≈ x86 cdyn |

- ~3 µs/update core-ops on both platforms: the C++ core is essentially *free*
  relative to Python; RK3588 lands within ~15% of the x86 host despite the
  power envelope. In end-to-end A/B harnesses (which capture per-op matchings)
  the honest frame is **28–40×** depending on platform — the harness, not the
  core, sets the ceiling.
- **NEON verdict (M3, RK3588, measured)**: end-to-end update cost with NEON
  paths compiled out (`-DDM_NO_NEON`) is indistinguishable from noise (±8%
  run-to-run) — the blossom kernel dominates, not the shim. Per-op:
  `classify_parity` NEON is ~20× faster, `scan_index_i32` ~2.9×, but the
  zero-fill reset is *faster as memset* — M3 tuning replaced the hand-NEON
  fill with `memset` for the dominant zero case (2× at the op level,
  end-to-end unchanged within noise).

**Testing (cdyn, 3 pillars — `tests_cdyn.py`):** a bitmask C++ unit oracle
(`cpp/tests/`, differential vs a brute-force matcher), 5-seed golden-file
replay (`cpp/tests/golden/`) asserting size-after-every-op parity across
languages, and Python-side suites checking cdyn against the networkx oracle
after **every** operation *and* cross-implementation parity with
`dynamic.DynamicMatching` on identical workloads. Same seeds and gauntlets as
the pure-Python suite; 67/67 green on x86-64 and RK3588.

**Known gaps:** `matching()` snapshot materialization is comparatively more
expensive in cdyn than in Python under full per-op-capture A/B harnesses (it
was tuned away from the core path); `classify_parity` (the strongest NEON win,
~20×) sits on a currently near-dead code path — kept for future weighted
variants. The C++ port is on branch `cpp-port`; merging to `main` is a pending
decision (see `docs/cpp-port-memo.md`).

## Honest caveats

- Worst case is still **O(m) per update** (one blossom search); pathological
  workloads approach that, random workloads stay ~1 ms at n=2000.
- **Cold build is unoptimized**: n inserts run n searches (~16 ms/insert at
  n=2000). Use this for workloads that *change over time*, not as a static
  solver — for one-shot matching, run any blossom implementation once.
- **Incremental-only use case**: weights are not supported; this is
  maximum-cardinality matching on unweighted general graphs.

## License

MIT — see [LICENSE](LICENSE).
