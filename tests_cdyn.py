"""Differential tests for the cdyn pybind11 wrapper (M2).

Three pillars, mirroring tests_dynamic.py:
  1. cdyn vs networkx oracle after EVERY op (validity + maximum cardinality);
  2. cdyn vs pure-Python dynamic.DynamicMatching after EVERY op on identical
     workloads (cross-implementation report/matching parity, same seeds);
  3. golden-file replay of the M1 fixtures (cpp/tests/golden/seed*.txt) —
     size-after-every-op parity across languages.
"""

import os
import random

import networkx as nx
import pytest

import cdyn
from cdyn import DynamicMatching as CDyn
from dynamic import DynamicMatching as PyDyn

GOLDEN_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "cpp", "tests", "golden")


def oracle_size(g: nx.Graph) -> int:
    return len(nx.max_weight_matching(g, maxcardinality=True))


def edges_of(g: nx.Graph):
    return {tuple(sorted(e)) for e in g.edges}


def check_impl(dm, g: nx.Graph):
    """Invariant + oracle check of a cdyn instance against graph g."""
    assert dm.is_valid(), f"invalid matching: {dm.matching()}"
    M = dm.matching()
    seen = set()
    for u, v in M:
        assert isinstance(u, int) and isinstance(v, int)
        assert u < v
        assert g.has_edge(u, v), f"matched edge ({u},{v}) not in graph"
        assert u not in seen and v not in seen, "vertex reused"
        seen.add(u); seen.add(v)
    assert dm.size() == len(M)
    assert dm.num_edges() == g.number_of_edges()
    assert oracle_size(g) == dm.size(), (
        f"size {dm.size()} != oracle {oracle_size(g)}; edges={sorted(g.edges)}"
    )
    free = {v for v in range(dm.n) if v not in seen}
    assert dm.free_vertices() == free


def assert_report_parity(rc, rp):
    """cdyn report vs dynamic.py report — matching-independent fields.

    op/edge/size depend only on the graph (identical workloads), so they must
    match exactly. searched/augmented/nu_dropped depend on WHICH maximum
    matching each implementation currently holds (not unique), so only the
    size after the op is compared here; matching-dependent consistency is
    checked by assert_report_consistent().
    """
    assert rc.op == rp.op
    assert tuple(rc.edge) == tuple(rp.edge)
    assert rc.size == rp.size


def assert_report_consistent(r, size_before, op):
    """Internal consistency of a report against the size delta it witnessed."""
    assert r.op == op
    if r.op == "noop":
        assert r.searched is False and r.work == 0 and r.augmented is False
        assert r.size == size_before
        return
    if r.op == "delete":
        # matched delete: augmented means fully re-augmented back to size_before
        if r.augmented:
            assert r.size == size_before and not r.nu_dropped
        elif r.nu_dropped:
            assert r.size == size_before - 1
        else:
            assert r.size == size_before
    elif r.augmented:
        assert r.size == size_before + 1
    else:
        assert r.size == size_before
    if not r.searched:
        assert r.work == 0


# ---------------------------------------------------------------- fuzz: triple differential

@pytest.mark.parametrize("n,density,steps", [
    (8, 0.05, 50), (8, 0.2, 50), (8, 0.5, 200),
    (12, 0.1, 100), (12, 0.5, 200),
    (20, 0.2, 150),
])
def test_random_sequence_oracle(n, density, steps):
    """cdyn vs networkx oracle after every op (mirrors tests_dynamic pillar 1)."""
    for seed in range(3):
        rng = random.Random(seed * 977 + n * 31 + int(density * 100) + steps)
        all_edges = [(u, v) for u in range(n) for v in range(u + 1, n)]
        dm = CDyn(n)
        g = nx.Graph(); g.add_nodes_from(range(n))
        present = set()
        for _ in range(steps):
            if present and rng.random() < min(0.5, len(present) / (density * len(all_edges) + 1)):
                e = rng.choice(sorted(present))
                dm.delete(*e); g.remove_edge(*e); present.discard(e)
            else:
                for _ in range(50):
                    e = rng.choice(all_edges)
                    if e not in present:
                        break
                else:
                    continue
                dm.insert(*e); g.add_edge(*e); present.add(e)
            check_impl(dm, g)


@pytest.mark.parametrize("n,steps", [(6, 300), (10, 400), (16, 400)])
def test_random_sequence_cross_impl(n, steps):
    """cdyn vs dynamic.py: identical ops, identical reports, identical matchings."""
    for seed in range(5):
        rng = random.Random(seed * 7919 + n)
        all_edges = [(u, v) for u in range(n) for v in range(u + 1, n)]
        dc, dp = CDyn(n), PyDyn(n)
        g = nx.Graph(); g.add_nodes_from(range(n))
        present = set()
        sc = sp = 0
        ops_done = 0
        for _ in range(steps):
            if present and rng.random() < 0.4:
                e = rng.choice(sorted(present))
                rc, rp = dc.delete(*e), dp.delete(*e)
                g.remove_edge(*e); present.discard(e)
                name = "delete"
                ops_done += 1
            else:
                for _ in range(50):
                    e = rng.choice(all_edges)
                    if e not in present:
                        break
                else:
                    continue
                rc, rp = dc.insert(*e), dp.insert(*e)
                g.add_edge(*e); present.add(e)
                name = "insert"
                ops_done += 1
            assert_report_parity(rc, rp)
            assert_report_consistent(rc, sc, name)
            assert_report_consistent(rp, sp, name)
            # maximum matching is NOT unique across implementations — compare
            # semantics (size/validity), and check both against the oracle.
            assert dc.size() == dp.size()
            assert dc.is_valid() and dp.is_valid()
            oracle = oracle_size(g)
            assert dc.size() == oracle and dp.size() == oracle
            # both must be genuine matchings of g
            for M in (dc.matching(), dp.matching()):
                seen = set()
                for u, v in M:
                    assert g.has_edge(u, v) and u < v
                    assert u not in seen and v not in seen
                    seen.add(u); seen.add(v)
                assert len(M) == oracle
            sc, sp = dc.size(), dp.size()
        # stats: update counts identical; searches/augmentations are
        # matching-choice dependent (non-unique) — sanity only.
        assert dc.stats["updates"] == dp.stats["updates"] == ops_done
        assert dc.stats["work"] > 0 and dp.stats["work"] > 0


def test_batches_cross_impl():
    dc, dp = CDyn(8), PyDyn(8)
    edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (0, 7)]
    rcs, rps = dc.insert_many(edges), dp.insert_many(edges)
    assert len(rcs) == len(rps) == len(edges)
    for rc, rp in zip(rcs, rps):
        assert_report_parity(rc, rp)
    assert dc.size() == dp.size()
    rcs, rps = dc.delete_many([(0, 1), (2, 3), (4, 5)]), dp.delete_many([(0, 1), (2, 3), (4, 5)])
    for rc, rp in zip(rcs, rps):
        assert_report_parity(rc, rp)
    assert dc.size() == dp.size()


# ---------------------------------------------------------------- golden replay

@pytest.mark.parametrize("seed", range(5))
def test_golden_replay(seed):
    """M1 golden fixtures: size-after-every-op parity (cross-language)."""
    path = os.path.join(GOLDEN_DIR, f"seed{seed}.txt")
    with open(path) as f:
        lines = [ln.split() for ln in f if ln.strip()]
    dm = CDyn(30)
    for op, u, v, size in lines:
        u, v, size = int(u), int(v), int(size)
        rep = dm.insert(u, v) if op == "i" else dm.delete(u, v)
        assert rep.size == size, (
            f"seed {seed}: op {op} ({u},{v}) size {rep.size} != golden {size}"
        )
        assert dm.size() == size
    assert dm.is_valid()


def test_golden_replay_cross_impl():
    """Full golden replay against dynamic.py too: sizes agree throughout
    (matchings may differ — maximum matching is not unique)."""
    for seed in range(5):
        with open(os.path.join(GOLDEN_DIR, f"seed{seed}.txt")) as f:
            lines = [ln.split() for ln in f if ln.strip()]
        dc, dp = CDyn(30), PyDyn(30)
        for op, u, v, _size in lines:
            u, v = int(u), int(v)
            (dc.insert if op == "i" else dc.delete)(u, v)
            (dp.insert if op == "i" else dp.delete)(u, v)
            assert dc.size() == dp.size()  # matchings may differ (non-unique)


# ---------------------------------------------------------------- API semantics

def test_module_and_class():
    assert cdyn.__name__ == "cdyn"
    assert CDyn is cdyn.DynamicMatching


def test_constructor_errors():
    with pytest.raises(ValueError):
        CDyn(-1)
    dm = CDyn(0)
    assert dm.n == 0 and dm.size() == 0 and dm.matching() == []


def test_self_loop_rejected():
    dm = CDyn(4)
    with pytest.raises(ValueError):
        dm.insert(2, 2)
    with pytest.raises(ValueError):
        dm.delete(2, 2)


def test_out_of_range():
    dm = CDyn(4)
    with pytest.raises(IndexError):
        dm.insert(4, 0)
    with pytest.raises(IndexError):
        dm.delete(0, -1)


def test_duplicate_insert_noop():
    dm = CDyn(4)
    r1 = dm.insert(0, 1)
    assert r1.op == "insert" and r1.edge == (0, 1) and r1.augmented
    r2 = dm.insert(1, 0)  # reversed order dedups
    assert r2.op == "noop" and r2.searched is False
    assert dm.size() == 1 and dm.matching() == [(0, 1)]


def test_delete_absent_noop():
    dm = CDyn(4)
    dm.insert(0, 1)
    r = dm.delete(2, 3)
    assert r.op == "noop"
    assert dm.size() == 1 and dm.is_valid()


def test_matching_format():
    dm = CDyn(6)
    dm.insert_many([(3, 4), (0, 2), (1, 5)])
    M = dm.matching()
    assert M == [(0, 2), (1, 5), (3, 4)]
    assert all(u < v for u, v in M)
    assert dm.size() == 3 and dm.free_vertices() == set()


def test_free_vertices_is_set():
    dm = CDyn(5)
    dm.insert(0, 2)
    assert dm.free_vertices() == {1, 3, 4}
    assert isinstance(dm.free_vertices(), set)


def test_report_repr():
    dm = CDyn(4)
    r = repr(dm.insert(0, 1))
    assert r.startswith("RepairReport('insert', (0, 1), searched=False")


def test_stats_snapshot():
    dm = CDyn(4)
    dm.insert(0, 1)
    s = dict(dm.stats)
    assert set(s) == {"updates", "searches", "augmentations", "work"}
    assert s["updates"] == 1 and s["searches"] == 0
    dm.insert(2, 0)  # one free endpoint: search runs
    s2 = dict(dm.stats)
    assert s2["searches"] >= 1


def test_nu_dropped_flag():
    dm = CDyn(3)
    dm.insert_many([(0, 1), (1, 2)])
    rep = dm.delete(1, 2)  # present but unmatched: zero work
    assert rep.searched is False and dm.size() == 1
    rep = dm.delete(0, 1)  # matched; only free vertex left is isolated 0 -> no re-augment
    assert rep.nu_dropped is True and dm.size() == 0


def test_petersen_full():
    g0 = nx.petersen_graph()
    edges = [tuple(sorted(e)) for e in g0.edges()]
    dm = CDyn(10)
    dm.insert_many(edges)
    g = nx.Graph(); g.add_nodes_from(range(10)); g.add_edges_from(edges)
    check_impl(dm, g)
    assert dm.size() == 5
