#pragma once

#include <TrussC.h>

#include <vector>

namespace tcx {

// =============================================================================
// Plane
// signedDistance(p) = normal.dot(p) + d.
// Convention used throughout tcxVoronoi: the "inside" (kept) half is
// signedDistance < 0.
//
// NOTE: this lives in tcxVoronoi for now. It is a small, generally useful
// type and is a candidate to promote into core (tc::Plane) once the slicer
// that depends on it has proven stable.
// =============================================================================
struct Plane {
    tc::Vec3 normal{0.0f, 1.0f, 0.0f};
    float d = 0.0f;

    Plane() = default;
    Plane(const tc::Vec3& n, float dd) : normal(n), d(dd) {}

    // Plane passing through p with the given normal.
    static Plane fromPointNormal(const tc::Vec3& p, const tc::Vec3& n) {
        tc::Vec3 nn = n.normalized();
        return Plane(nn, -nn.dot(p));
    }

    // Perpendicular bisector of segment a-b. The normal points from a toward b,
    // so signedDistance(p) < 0 on a's side (the half closer to a).
    static Plane bisector(const tc::Vec3& a, const tc::Vec3& b) {
        tc::Vec3 n = (b - a).normalized();
        tc::Vec3 m = (a + b) * 0.5f;
        return Plane(n, -n.dot(m));
    }

    float signedDistance(const tc::Vec3& p) const { return normal.dot(p) + d; }
};

// =============================================================================
// VoronoiCell - a single 3D fragment
// =============================================================================
struct VoronoiCell {
    tc::Mesh mesh;                 // the fragment (closed)
    tc::Vec3 seed;                 // the Voronoi site this cell was grown from
    tc::Vec3 centroid;             // representative center of the fragment
    std::vector<int> neighbors;    // indices of adjacent cells (face-sharing)
};

// =============================================================================
// Interface - the shared face boundary between two cells
// Face-level only for now; edge/vertex correspondence is a planned extension.
// =============================================================================
struct Interface {
    int cellA = -1;
    int cellB = -1;
    tc::Vec3 point;   // representative contact point (centroid of the shared face)
    tc::Vec3 normal;  // cut-plane normal, oriented from cellA toward cellB
};

// =============================================================================
// FractureResult - 3D fracture output
// =============================================================================
struct FractureResult {
    std::vector<VoronoiCell> cells;
    std::vector<Interface> interfaces;

    std::vector<int> neighborsOf(int cell) const {
        if (cell < 0 || cell >= static_cast<int>(cells.size())) return {};
        return cells[cell].neighbors;
    }

    const Interface* interfaceBetween(int a, int b) const {
        for (const Interface& it : interfaces) {
            if ((it.cellA == a && it.cellB == b) ||
                (it.cellA == b && it.cellB == a)) {
                return &it;
            }
        }
        return nullptr;
    }
};

} // namespace tcx
