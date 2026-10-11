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
