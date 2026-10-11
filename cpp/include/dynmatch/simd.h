// simd.h — portable SIMD shim for the dynamic-matching C++ port.
//
// M1 scope: structure + hot-loop hygiene, not forced intrinsics everywhere.
// The two genuinely batch-oriented hot operations are covered:
//   1. batch mark/blossom-array reset (fill)           -> fill_u8 / reset_marks
//   2. adjacency-block scan (membership test)          -> scan_index_i32
//   3. parity classification of forest flags           -> classify_parity
// Plain-loop implementations are the portable fallback; NEON paths are provided
// under __ARM_NEON (on aarch64 NEON is baseline under -march=armv8-a, so a
// compile-time guard suffices — no runtime dispatch needed on ARM64).
// On x86-64 the plain loops auto-vectorize under SSE2 (baseline), which the
// compiler does by default; we do not add x86 intrinsics in M1.
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#define DM_HAS_NEON 1
#endif

namespace dm::simd {

// Fill n bytes with value v (used to batch-reset mark/blossom/used scratch
// arrays between searches). Contiguous, aliasing-free, SIMD-friendly.
inline void fill_u8(uint8_t* a, size_t n, uint8_t v) {
#if DM_HAS_NEON
    if (n >= 16) {
        const uint8x16_t vv = vdupq_n_u8(v);
        uint8_t* p = a;
        size_t rem = n;
        while (rem >= 16) {
            vst1q_u8(p, vv);
            p += 16;
            rem -= 16;
        }
        n = rem;
        a = p;
    }
#endif
    for (size_t i = 0; i < n; ++i) a[i] = v;
}

// Batch-reset a mark array (the `for i in range(n): blossom[i] = False` step
// of every blossom contraction, and the per-search scratch reset).
inline void reset_marks(uint8_t* a, size_t n) { fill_u8(a, n, 0); }

// Linear scan of a contiguous int32 block for `key`; returns index or -1.
// Adjacency blocks are kept sorted by vertex id, so this is also a
// lower-bound scan; SIMD compares check 4 ids per instruction.
inline int32_t scan_index_i32(const int32_t* a, int32_t n, int32_t key) {
#if DM_HAS_NEON
    if (n >= 4) {
        const int32x4_t kk = vdupq_n_s32(key);
        int32_t i = 0;
        for (; i + 4 <= n; i += 4) {
            uint32x4_t eq = vceqq_s32(vld1q_s32(a + i), kk);
            if (vmaxvq_u32(eq) != 0) {
                // block contains a hit: scalar finish inside this block
                for (int32_t j = i; j < i + 4; ++j)
                    if (a[j] == key) return j;
            }
        }
        // tail
        for (int32_t j = i; j < n; ++j)
            if (a[j] == key) return j;
        return -1;
    }
#endif
    for (int32_t i = 0; i < n; ++i)
        if (a[i] == key) return i;
    return -1;
}

// Parity classifier: given the forest `even` flags (1 = even/outer vertex),
// emit `out[i] = 1` where vertex i is an even forest member, 0 otherwise.
// Vectorizable in blocks of 16; kept as a demonstration/template for the
// M2+ parity-bucket workloads (odd-vertex lists, alternating parity checks).
inline void classify_parity(const uint8_t* even_flags, uint8_t* out, size_t n) {
#if DM_HAS_NEON
    if (n >= 16) {
        const uint8x16_t zero = vdupq_n_u8(0);
        uint8_t const* s = even_flags;
        uint8_t* d = out;
        size_t rem = n;
        while (rem >= 16) {
            uint8x16_t f = vld1q_u8(s);
            vst1q_u8(d, vcgtq_u8(f, zero));
            s += 16; d += 16; rem -= 16;
        }
        n = rem; even_flags = s; out = d;
    }
#endif
    for (size_t i = 0; i < n; ++i) out[i] = even_flags[i] ? 1 : 0;
}

}  // namespace dm::simd
