# Merge decision memo: `cpp-port` → `main`

**Status: DECISION PENDING — Samuel's call. Nothing is merged. This memo
presents the options and a recommendation only.**
Date: 2026-10-12 · Branch head: `fde74b2` (M4 docs commit follows this memo)
Chain: M1 C++ core → M2 pybind11 `cdyn` + packaging → M3 RK3588 native verify
→ M4 this package.

## What is on the branch

- `cpp/` — C++17 core: CSR arena graph, faithful e-maxx blossom kernel
  (multi-root alternating forest), NEON-ready SIMD shim (memset-tuned reset).
- `cdyn` — pybind11 module with drop-in API parity with `dynamic.DynamicMatching`.
- Packaging via scikit-build-core (`pip install .` builds the extension through
  CMake; `dynamic.py` ships in the same wheel).
- Tests: C++ bitmask oracle unit suite + 5-seed golden fixtures; Python-side
  `tests_cdyn.py` with per-op networkx oracle checks and cross-implementation
  parity. **67/67 green on x86-64 and RK3588.**

## Benchmarks (honest frame)

- Core update ops: cdyn ~3 µs vs Python 973 µs on x86 (~300× update-phase on
  matched harness); full A/B harness end-to-end **28–40×** depending on
  platform/harness.
- RK3588: 3.4–3.7 µs vs 153 µs Python (40×); NEON end-to-end delta ≈ noise —
  the blossom kernel dominates, not the SIMD shim.

## What merging means

Merging as-is makes **CMake + a C++17 compiler a hard build dependency for
every `pip install`** (source installs; sdist builds locally). The package
currently has zero runtime dependencies — this is the single biggest change
to the user story. Wheels (manylinux/macOS) would remove the compiler
requirement for most users, but need CI to build them.

## User-visible changes if merged

- New import `cdyn` alongside `dynamic` (drop-in semantics, same methods).
- `dynamic.py` remains available unchanged — pure-Python path never goes away
  unless we choose option B below and deprecate it.
- Docs gain the C++ section, platform table, NEON verdict, testing story.

## Risks

- **Platform build failures in the wild**: old GCC/Clang, missing CMake,
  exotic Pythons (PyPy), Windows MSVC quirks. Every one of these becomes a
  failed install for a package that used to "just work" (stdlib only).
- Maintenance surface doubles: two implementations to keep in behavioral
  lockstep (mitigated by the differential + golden suites, which fail loudly
  on divergence).
- Wheel CI is not set up on `main` yet; without it, source-only distribution
  amplifies the build-dependency risk.

## Options

**A. Merge with graceful fallback (recommended)**
Keep `dynamic.py` the default import; attempt the `cdyn` build at install
time and fall back to pure-Python if compilation fails (try/except import,
documented). Users with a toolchain get ~30–300×; users without get today's
behavior. Cost: a small amount of packaging cleverness (build-optional
extension) and slightly fuzzier "what am I running" ergonomics (expose
`dm.backend` to keep it explicit).

**B. Merge hard (C++ required)**
`cdyn` becomes the implementation; build failure = install failure. Cleanest
story, one code path to test, best performance guarantee — but breaks the
zero-dependency promise for source installs and makes the repo hostile to
constrained environments.

**C. Keep the branch; publish C++ separately**
`main` stays pure-Python; release `cdyn` as an optional extra
(`pip install dynamic-matching[cpp]`) or a separate wheel package. Zero risk
to existing users; cost is discoverability and two release pipelines.

## Recommendation

**A**, with **C** as the low-effort fallback if the build-optional packaging
turns out to be fragile. Rationale: the differential test infrastructure
makes dual-implementation maintenance cheap, the performance win is real and
measured on both x86 and ARM, and fallback preserves the library's
"stdlib-only, just works" character for every existing user.

*No action taken. Merge/publish decision belongs to Samuel.*
