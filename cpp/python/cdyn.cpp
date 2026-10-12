// cdyn.cpp — pybind11 wrapper around dm::DynamicMatching (M2).
//
// Mirrors dynamic.py semantics 1:1:
//   * insert/delete return a RepairReport-like object (cdyn.RepairReport) with
//     the same attribute names as dynamic.RepairReport;
//   * insert_many/delete_many return lists of reports;
//   * matching() -> sorted list of (u, v) tuples with u < v;
//   * free_vertices() -> set of ints;  stats -> snapshot dict
//     {"updates", "searches", "augmentations", "work"};
//   * exceptions: std::invalid_argument -> ValueError (self-loop, n < 0),
//     std::out_of_range -> IndexError (vertex out of range) — both are
//     pybind11's built-in translations, matching dynamic.py exactly.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <string>
#include <utility>

#include "dynmatch/dynmatch.h"

namespace py = pybind11;
using dm::DynamicMatching;

namespace {

py::object make_edge(const dm::RepairReport& r) {
    return py::make_tuple(r.u, r.v);
}

py::object report_repr(const dm::RepairReport& r) {
    std::string s = "RepairReport('" + std::string(r.op) + "', (" +
                    std::to_string(r.u) + ", " + std::to_string(r.v) +
                    "), searched=" + (r.searched ? "True" : "False") +
                    ", augmented=" + (r.augmented ? "True" : "False") +
                    ", nu_dropped=" + (r.nu_dropped ? "True" : "False") +
                    ", work=" + std::to_string(r.work) +
                    ", size=" + std::to_string(r.size) + ")";
    return py::str(s);
}

// Batch helpers that return the report list (mirrors dynamic.py).
std::vector<dm::RepairReport> insert_many(DynamicMatching& dm,
                                          const std::vector<std::pair<int32_t, int32_t>>& edges) {
    std::vector<dm::RepairReport> out;
    out.reserve(edges.size());
    for (const auto& [u, v] : edges) out.push_back(dm.insert(u, v));
    return out;
}

std::vector<dm::RepairReport> delete_many(DynamicMatching& dm,
                                          const std::vector<std::pair<int32_t, int32_t>>& edges) {
    std::vector<dm::RepairReport> out;
    out.reserve(edges.size());
    for (const auto& [u, v] : edges) out.push_back(dm.remove(u, v));
    return out;
}

std::vector<std::pair<int32_t, int32_t>> matching_sorted(const DynamicMatching& dm) {
    auto m = dm.matching();
    std::sort(m.begin(), m.end());
    return m;
}

py::object free_vertices_set(const DynamicMatching& dm) {
    py::set s = py::set();
    for (int32_t v : dm.free_vertices()) s.add(py::int_(v));
    return std::move(s);
}

py::dict stats_dict(const DynamicMatching& dm) {
    py::dict d;
    d["updates"] = dm.stats.updates;
    d["searches"] = dm.stats.searches;
    d["augmentations"] = dm.stats.augmentations;
    d["work"] = dm.stats.work;
    return d;
}

}  // namespace

PYBIND11_MODULE(cdyn, m) {
    m.doc() = "DynamicMatching C++ core (pybind11) — exact dynamic maximum-cardinality "
              "matching for general graphs. Semantics 1:1 with dynamic.py.";

    py::class_<dm::RepairReport>(m, "RepairReport",
                                 "Summary of the work done by a single update (mirrors "
                                 "dynamic.RepairReport).")
        .def_readonly("op", &dm::RepairReport::op)
        .def_property_readonly("edge", &make_edge)
        .def_readonly("searched", &dm::RepairReport::searched)
        .def_readonly("augmented", &dm::RepairReport::augmented)
        .def_readonly("nu_dropped", &dm::RepairReport::nu_dropped)
        .def_readonly("work", &dm::RepairReport::work)
        .def_readonly("size", &dm::RepairReport::size)
        .def("__repr__", &report_repr);

    py::class_<DynamicMatching>(m, "DynamicMatching",
                                "Exact dynamic maximum-cardinality matching on n fixed "
                                "vertices (C++ core).")
        .def(py::init<int32_t>(), py::arg("n"))
        .def_property_readonly("n", &DynamicMatching::n)
        .def("insert", &DynamicMatching::insert, py::arg("u"), py::arg("v"),
             "Insert edge (u, v); no-op if already present.")
        .def("delete", &DynamicMatching::remove, py::arg("u"), py::arg("v"),
             "Delete edge (u, v); no-op if absent.")
        .def("insert_many", &insert_many, py::arg("edges"),
             "Apply a sequence of inserts; returns the RepairReports.")
        .def("delete_many", &delete_many, py::arg("edges"),
             "Apply a sequence of deletes; returns the RepairReports.")
        .def("matching", &matching_sorted,
             "Current maximum matching as a sorted list of (u, v) pairs, u < v.")
        .def("size", &DynamicMatching::size, "Number of matched edges (|M|).")
        .def("num_edges", &DynamicMatching::num_edges,
             "Number of logical (deduplicated) edges currently present.")
        .def("is_valid", &DynamicMatching::is_valid,
             "Invariant check: mate symmetric, every matched pair is a real edge.")
        .def("free_vertices", &free_vertices_set,
             "Set of currently unmatched vertices.")
        .def_property_readonly("stats", &stats_dict,
                               "Snapshot dict: updates, searches, augmentations, work.");
}
