#pragma once

#include <TrussC.h>

#include "tcxVoronoiTypes.h"

#include <vector>

namespace tcx {

// =============================================================================
// SliceMesh - lightweight positions-only indexed triangle mesh.
//
// This is the working representation used while cascading plane slices to
// build a Voronoi cell. Normals/uv are intentionally dropped here: they would
// only get in the way of topology operations, and are recomputed once at the
// end via toRenderMesh(). Vertices are welded (shared) so that edges are
// shared between adjacent triangles, which the cut-loop stitching relies on.
// =============================================================================
struct SliceMesh {
    std::vector<tc::Vec3> verts;
    std::vector<unsigned int> tris;  // 3 indices per triangle

    bool empty() const { return tris.empty(); }
    size_t numTris() const { return tris.size() / 3; }
};

// Build a welded SliceMesh from a tc::Mesh (assumed PrimitiveMode::Triangles).
// Duplicate vertex positions are merged so the result is edge-connected.
SliceMesh toSliceMesh(const tc::Mesh& m, float weldEps = 1e-4f);

// Slice a closed SliceMesh by a plane, keeping the inside (signedDistance < 0)
// half and capping the cut so the result stays closed.
//   - Triangles fully inside are kept; fully outside are dropped.
//   - Triangles straddling the plane are clipped (Sutherland-Hodgman).
//   - The cut cross-section is stitched into loop(s) and triangulated as a cap.
//
// Phase 1 caps assume convex cross-sections (single convex loop) and fan
// triangulate. Concave meshes (multiple / non-convex loops) are a Phase 2
// hardening step.
SliceMesh sliceKeepInside(const SliceMesh& in, const Plane& plane, float eps = 1e-5f);

// Convert a finished cell SliceMesh into a renderable tc::Mesh. Produces a
// flat-shaded mesh (per-face normals, duplicated vertices) for a faceted shard
// look that reads well for fracture.
tc::Mesh toRenderMesh(const SliceMesh& sm);

// Average of all vertices (cheap representative center).
tc::Vec3 meshCentroid(const SliceMesh& sm);

} // namespace tcx
