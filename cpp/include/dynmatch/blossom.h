// blossom.h — multi-root Edmonds blossom search (kernel of dynamic.py).
//
// Faithful C++ port of dynamic.py::DynamicMatching._search: the classical
// e-maxx single-root blossom kernel extended to multiple free roots (tree ids,
// cross-tree even-even augmenting flips). Algorithm semantics preserved 1:1 —
// see blossom.cpp for the annotated correspondences.
#pragma once

#include <cstdint>

#include "dynmatch/forest.h"
#include "dynmatch/graph.h"

namespace dm {

// One multi-root blossom search over free vertices `roots`.
// On success, augments `mate` along the found path (*size += 1).
// Returns the number of adjacency slots scanned ("work").
// No allocation happens inside this function (all scratch is preallocated).
int64_t blossom_search(const Graph& g, int32_t* mate, const int32_t* roots,
                       int32_t nroots, int32_t* size, ForestScratch& fs);

}  // namespace dm
