#include "tcxVoronoiSlice.h"

#include <earcut/earcut.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>

using namespace std;
using namespace tc;

namespace tcx::voronoi {

// -----------------------------------------------------------------------------
// toSliceMesh - weld vertices by quantized position so triangles share edges.
// -----------------------------------------------------------------------------
SliceMesh toSliceMesh(const Mesh& m, float weldEps) {
    SliceMesh out;
    const vector<Vec3>& srcVerts = m.getVertices();
    const vector<unsigned int>& srcIdx = m.getIndices();

    const float inv = 1.0f / weldEps;
    auto quant = [&](const Vec3& p) {
        return make_tuple(static_cast<long long>(llround(p.x * inv)),
                          static_cast<long long>(llround(p.y * inv)),
                          static_cast<long long>(llround(p.z * inv)));
    };

    map<tuple<long long, long long, long long>, unsigned int> weld;
    // Map every source vertex index to a welded index.
    vector<unsigned int> remap(srcVerts.size());
    for (size_t i = 0; i < srcVerts.size(); ++i) {
        auto key = quant(srcVerts[i]);
        auto it = weld.find(key);
        if (it == weld.end()) {
            unsigned int id = static_cast<unsigned int>(out.verts.size());
            out.verts.push_back(srcVerts[i]);
            weld.emplace(key, id);
            remap[i] = id;
        } else {
            remap[i] = it->second;
        }
    }

    auto pushTri = [&](unsigned int a, unsigned int b, unsigned int c) {
        // Skip degenerate triangles that collapse after welding.
        if (a == b || b == c || a == c) return;
        out.tris.push_back(a);
        out.tris.push_back(b);
        out.tris.push_back(c);
    };

    if (!srcIdx.empty()) {
        for (size_t i = 0; i + 2 < srcIdx.size(); i += 3) {
            pushTri(remap[srcIdx[i]], remap[srcIdx[i + 1]], remap[srcIdx[i + 2]]);
        }
    } else {
        // Unindexed: treat consecutive triples as triangles.
        for (size_t i = 0; i + 2 < srcVerts.size(); i += 3) {
            pushTri(remap[i], remap[i + 1], remap[i + 2]);
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// capTriangulate - triangulate the cut cross-section (one or more coplanar
// loops) and append the cap triangles to 'out'.
//
// Handles concave cross-sections and holes: loops are projected to the plane's
// 2D basis, classified outer/hole by even-odd containment depth, and each filled
// loop (with its immediate holes) is triangulated with earcut. Each output
// triangle is oriented so its normal points toward +plane.normal (outward of the
// kept solid, which lives on the -normal side).
// -----------------------------------------------------------------------------
static void capTriangulate(SliceMesh& out,
                           const vector<vector<unsigned int>>& loops,
                           const Vec3& n) {
    const size_t L = loops.size();
    if (L == 0) return;

    // Plane basis (u, v) spanning the cut plane.
    Vec3 u = (fabs(n.x) > 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    u = (u - n * u.dot(n)).normalized();
    Vec3 v = n.cross(u);

    auto project = [&](unsigned int idx) -> array<double, 2> {
        const Vec3& p = out.verts[idx];
        return {static_cast<double>(p.dot(u)), static_cast<double>(p.dot(v))};
    };

    // 2D projection of every loop.
    vector<vector<array<double, 2>>> loop2d(L);
    for (size_t i = 0; i < L; ++i) {
        loop2d[i].reserve(loops[i].size());
        for (unsigned int idx : loops[i]) loop2d[i].push_back(project(idx));
    }

    // Point-in-polygon (ray cast) test in 2D.
    auto inside = [](const array<double, 2>& pt, const vector<array<double, 2>>& poly) -> bool {
        bool c = false;
        size_t m = poly.size();
        for (size_t i = 0, j = m - 1; i < m; j = i++) {
            double xi = poly[i][0], yi = poly[i][1];
            double xj = poly[j][0], yj = poly[j][1];
            if (((yi > pt[1]) != (yj > pt[1])) &&
                (pt[0] < (xj - xi) * (pt[1] - yi) / (yj - yi) + xi)) {
                c = !c;
            }
        }
        return c;
    };

    // contains[a][b] = loop a strictly contains loop b (test b's first point).
    vector<int> depth(L, 0);
    vector<vector<bool>> contains(L, vector<bool>(L, false));
    for (size_t a = 0; a < L; ++a) {
        for (size_t b = 0; b < L; ++b) {
            if (a == b || loop2d[b].empty()) continue;
            if (inside(loop2d[b][0], loop2d[a])) {
                contains[a][b] = true;
                depth[b]++;
            }
        }
    }

    // Even depth -> filled (outer); odd depth -> hole. Each hole attaches to the
    // innermost filled loop that contains it (depth == hole.depth - 1).
    for (size_t f = 0; f < L; ++f) {
        if (depth[f] % 2 != 0) continue;  // holes are emitted with their parent

        vector<size_t> rings;
        rings.push_back(f);
        for (size_t h = 0; h < L; ++h) {
            if (depth[h] == depth[f] + 1 && contains[f][h]) rings.push_back(h);
        }

        // Assemble the earcut polygon and a flat index map (ring order).
        vector<vector<array<double, 2>>> poly;
        vector<unsigned int> flat;
        for (size_t r : rings) {
            poly.push_back(loop2d[r]);
            for (unsigned int idx : loops[r]) flat.push_back(idx);
        }

        vector<uint32_t> tri = mapbox::earcut<uint32_t>(poly);
        for (size_t k = 0; k + 2 < tri.size(); k += 3) {
            unsigned int a = flat[tri[k]], b = flat[tri[k + 1]], c = flat[tri[k + 2]];
            Vec3 fn = (out.verts[b] - out.verts[a]).cross(out.verts[c] - out.verts[a]);
            if (fn.dot(n) >= 0.0f) {
                out.tris.push_back(a); out.tris.push_back(b); out.tris.push_back(c);
            } else {
                out.tris.push_back(a); out.tris.push_back(c); out.tris.push_back(b);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// capResidualHoles - watertightness safety net.
//
// earcut can fail to fully triangulate a jagged, self-touching, or otherwise
// non-simple cross-section (common when the cut plane meets a blocky/voxel
// surface), leaving the cell with an open cap. This closes any edge of `out`
// that is still a boundary (used by exactly one triangle) AND lies on the cut
// plane, by stitching the residual boundary into loop(s) and centroid-fanning
// each -- watertight for ANY closed loop, regardless of convexity. Fan triangles
// are oriented toward +plane.normal to match the earcut caps.
//
// On clean meshes earcut leaves no residual, so this finds nothing and the
// earcut cap is used unchanged (no behaviour change for existing meshes). The
// plane filter keeps it from touching any hole inherited from an earlier slice.
// -----------------------------------------------------------------------------
static void capResidualHoles(SliceMesh& out, const Plane& plane, float eps) {
    map<pair<unsigned int, unsigned int>, int> use;
    auto bump = [&](unsigned int a, unsigned int b) {
        if (a == b) return;
        use[a < b ? make_pair(a, b) : make_pair(b, a)]++;
    };
    for (size_t t = 0; t + 2 < out.tris.size(); t += 3) {
        bump(out.tris[t], out.tris[t + 1]);
        bump(out.tris[t + 1], out.tris[t + 2]);
        bump(out.tris[t + 2], out.tris[t]);
    }

    const float tol = std::max(eps * 4.0f, 1e-4f);
    vector<pair<unsigned int, unsigned int>> boundary;
    for (auto& e : use) {
        if (e.second != 1) continue;
        if (fabs(plane.signedDistance(out.verts[e.first.first]))  <= tol &&
            fabs(plane.signedDistance(out.verts[e.first.second])) <= tol)
            boundary.push_back(e.first);
    }
    if (boundary.empty()) return;

    struct HalfEdge { unsigned int to; size_t seg; };
    unordered_map<unsigned int, vector<HalfEdge>> adj;
    adj.reserve(boundary.size() * 2);
    for (size_t s = 0; s < boundary.size(); ++s) {
        adj[boundary[s].first].push_back({boundary[s].second, s});
        adj[boundary[s].second].push_back({boundary[s].first, s});
    }
    vector<char> segUsed(boundary.size(), 0);

    for (size_t s0 = 0; s0 < boundary.size(); ++s0) {
        if (segUsed[s0]) continue;
        unsigned int start = boundary[s0].first;
        unsigned int cur = start;
        vector<unsigned int> loop;
        while (true) {
            loop.push_back(cur);
            size_t pick = numeric_limits<size_t>::max();
            unsigned int next = 0;
            for (const HalfEdge& he : adj[cur]) {
                if (!segUsed[he.seg]) { pick = he.seg; next = he.to; break; }
            }
            if (pick == numeric_limits<size_t>::max()) break;
            segUsed[pick] = 1;
            cur = next;
            if (cur == start) break;
        }
        if (loop.size() < 3) continue;

        Vec3 ctr(0, 0, 0);
        for (unsigned int id : loop) ctr += out.verts[id];
        ctr = ctr * (1.0f / static_cast<float>(loop.size()));
        unsigned int cid = static_cast<unsigned int>(out.verts.size());
        out.verts.push_back(ctr);
        for (size_t i = 0; i < loop.size(); ++i) {
            unsigned int a = loop[i], b = loop[(i + 1) % loop.size()];
            Vec3 fn = (out.verts[a] - ctr).cross(out.verts[b] - ctr);
            out.tris.push_back(cid);
            if (fn.dot(plane.normal) >= 0.0f) { out.tris.push_back(a); out.tris.push_back(b); }
            else                              { out.tris.push_back(b); out.tris.push_back(a); }
        }
    }
}

// -----------------------------------------------------------------------------
// sliceKeepInside
// -----------------------------------------------------------------------------
SliceMesh sliceKeepInside(const SliceMesh& in, const Plane& plane, float eps) {
    SliceMesh out;
    if (in.empty()) return out;

    const size_t nv = in.verts.size();
    vector<float> dist(nv);
    for (size_t i = 0; i < nv; ++i) dist[i] = plane.signedDistance(in.verts[i]);

    // Map source (kept) vertices into the output lazily.
    vector<int> srcRemap(nv, -1);
    auto keepVertex = [&](unsigned int i) -> unsigned int {
        if (srcRemap[i] < 0) {
            srcRemap[i] = static_cast<int>(out.verts.size());
            out.verts.push_back(in.verts[i]);
        }
        return static_cast<unsigned int>(srcRemap[i]);
    };

    // Cut vertices are identified by the undirected source edge they split, so
    // that adjacent triangles sharing that edge produce the SAME welded cut
    // vertex (exact match, no epsilon stitching needed).
    map<pair<unsigned int, unsigned int>, unsigned int> cutVerts;
    auto cutOnEdge = [&](unsigned int a, unsigned int b) -> unsigned int {
        pair<unsigned int, unsigned int> key = a < b ? make_pair(a, b) : make_pair(b, a);
        auto it = cutVerts.find(key);
        if (it != cutVerts.end()) return it->second;
        float da = dist[a], db = dist[b];
        float t = da / (da - db);  // da and db straddle 0
        Vec3 p = in.verts[a].lerp(in.verts[b], t);
        unsigned int id = static_cast<unsigned int>(out.verts.size());
        out.verts.push_back(p);
        cutVerts.emplace(key, id);
        return id;
    };

    const unsigned int* tri = in.tris.data();
    const size_t nt = in.tris.size();
    bool anyOutside = false;
    for (size_t t = 0; t < nt; t += 3) {
        unsigned int idx[3] = {tri[t], tri[t + 1], tri[t + 2]};

        // Sutherland-Hodgman clip of the triangle against the half-space
        // (keep signedDistance <= eps), fan-triangulating the kept polygon.
        unsigned int poly[4];
        int polyN = 0;

        for (int e = 0; e < 3; ++e) {
            unsigned int cur = idx[e];
            unsigned int nxt = idx[(e + 1) % 3];
            float dCur = dist[cur];
            float dNxt = dist[nxt];
            if (dCur > eps) anyOutside = true;
            bool curIn = dCur <= eps;

            if (curIn && polyN < 4) poly[polyN++] = keepVertex(cur);

            // Genuine crossing (strictly opposite sides beyond eps) -> cut vertex.
            bool crossing = (dCur > eps && dNxt < -eps) || (dCur < -eps && dNxt > eps);
            if (crossing && polyN < 4) poly[polyN++] = cutOnEdge(cur, nxt);
        }

        if (polyN >= 3) {
            // Fan triangulate the kept polygon.
            for (int k = 1; k + 1 < polyN; ++k) {
                out.tris.push_back(poly[0]);
                out.tris.push_back(poly[k]);
                out.tris.push_back(poly[k + 1]);
            }
        }
    }

    if (!anyOutside) return out;  // plane removed nothing: no cut, no cap

    // -------------------------------------------------------------------------
    // Cap the cut cross-section. Because the input is a closed surface, every
    // boundary edge of the clipped side geometry (an edge used by exactly ONE
    // triangle) lies on the cut plane. Deriving the cap loops from those
    // boundary edges makes the cap match the side geometry EXACTLY -> the cell
    // is watertight by construction, even where the plane passes through
    // existing vertices (blocky/voxel meshes leave no cut vertex there) or the
    // cross-section is concave or non-manifold. Interior edges (shared by two
    // kept triangles, incl. fan diagonals) are used twice and skipped.
    // -------------------------------------------------------------------------
    map<pair<unsigned int, unsigned int>, int> edgeUse;
    {
        const unsigned int* ot = out.tris.data();
        const size_t ont = out.tris.size();
        auto bump = [&](unsigned int a, unsigned int b) {
            if (a == b) return;
            edgeUse[a < b ? make_pair(a, b) : make_pair(b, a)]++;
        };
        for (size_t t = 0; t < ont; t += 3) {
            bump(ot[t], ot[t + 1]); bump(ot[t + 1], ot[t + 2]); bump(ot[t + 2], ot[t]);
        }
    }

    vector<pair<unsigned int, unsigned int>> boundary;
    for (auto& e : edgeUse) if (e.second == 1) boundary.push_back(e.first);
    if (boundary.empty()) return out;

    // Stitch boundary edges into closed loop(s) by CONSUMING edges (not marking
    // vertices), so a shared vertex where two loops meet (degree > 2) splits
    // into separate loops instead of terminating the walk and dropping a cap.
    // Multiple disjoint loops + holes (concave cross-sections) are fine;
    // capTriangulate classifies outer vs holes.
    struct HalfEdge { unsigned int to; size_t seg; };
    unordered_map<unsigned int, vector<HalfEdge>> adj;
    adj.reserve(boundary.size() * 2);
    for (size_t s = 0; s < boundary.size(); ++s) {
        adj[boundary[s].first].push_back({boundary[s].second, s});
        adj[boundary[s].second].push_back({boundary[s].first, s});
    }
    vector<char> segUsed(boundary.size(), 0);

    vector<vector<unsigned int>> loops;
    for (size_t s0 = 0; s0 < boundary.size(); ++s0) {
        if (segUsed[s0]) continue;
        unsigned int start = boundary[s0].first;
        unsigned int cur = start;
        vector<unsigned int> loop;
        while (true) {
            loop.push_back(cur);
            size_t pick = numeric_limits<size_t>::max();
            unsigned int next = 0;
            for (const HalfEdge& he : adj[cur]) {
                if (!segUsed[he.seg]) { pick = he.seg; next = he.to; break; }
            }
            if (pick == numeric_limits<size_t>::max()) break;  // open chain: stop
            segUsed[pick] = 1;
            cur = next;
            if (cur == start) break;                           // closed loop
        }
        if (loop.size() >= 3) loops.push_back(std::move(loop));
    }

    capTriangulate(out, loops, plane.normal);

    // Close anything earcut left open on this plane (jagged/blocky cross-sections).
    capResidualHoles(out, plane, eps);

    return out;
}

// -----------------------------------------------------------------------------
// toRenderMesh - flat-shaded faceted mesh.
// -----------------------------------------------------------------------------
Mesh toRenderMesh(const SliceMesh& sm) {
    Mesh m;
    const unsigned int* tri = sm.tris.data();
    const size_t nt = sm.tris.size();
    for (size_t t = 0; t < nt; t += 3) {
        const Vec3& a = sm.verts[tri[t]];
        const Vec3& b = sm.verts[tri[t + 1]];
        const Vec3& c = sm.verts[tri[t + 2]];
        Vec3 nrm = (b - a).cross(c - a);
        float len = nrm.length();
        nrm = len > 1e-12f ? nrm * (1.0f / len) : Vec3(0, 1, 0);

        unsigned int base = static_cast<unsigned int>(m.getVertices().size());
        m.addVertex(a); m.addNormal(nrm);
        m.addVertex(b); m.addNormal(nrm);
        m.addVertex(c); m.addNormal(nrm);
        m.addTriangle(base, base + 1, base + 2);
    }
    return m;
}

// -----------------------------------------------------------------------------
// extrudePath
// -----------------------------------------------------------------------------
Mesh extrudePath(const Path& region, float thickness) {
    Mesh m;

    // Outer boundary polygon (first subpath), as 2D points.
    vector<array<double, 2>> poly2d;
    vector<Vec2> poly;
    const vector<Vec3>& rv = region.getVertices();
    if (region.getNumSubpaths() > 0) {
        auto rng = region.getSubpathRange(0);
        for (size_t i = rng.first; i < rng.second; ++i) {
            poly.push_back(Vec2(rv[i].x, rv[i].y));
            poly2d.push_back({static_cast<double>(rv[i].x), static_cast<double>(rv[i].y)});
        }
    } else {
        for (const Vec3& v : rv) {
            poly.push_back(Vec2(v.x, v.y));
            poly2d.push_back({static_cast<double>(v.x), static_cast<double>(v.y)});
        }
    }
    const size_t np = poly.size();
    if (np < 3) return m;

    vector<vector<array<double, 2>>> rings{poly2d};
    vector<uint32_t> capTri = mapbox::earcut<uint32_t>(rings);

    // Front cap at z=0 (normal -Z), back cap at z=thickness (normal +Z).
    unsigned int frontBase = 0;
    for (const Vec2& p : poly) { m.addVertex(Vec3(p.x, p.y, 0.0f)); m.addNormal(Vec3(0, 0, -1)); }
    unsigned int backBase = static_cast<unsigned int>(m.getVertices().size());
    for (const Vec2& p : poly) { m.addVertex(Vec3(p.x, p.y, thickness)); m.addNormal(Vec3(0, 0, 1)); }

    for (size_t k = 0; k + 2 < capTri.size(); k += 3) {
        // Front faces -Z: wind so the normal points to -Z.
        m.addTriangle(frontBase + capTri[k], frontBase + capTri[k + 2], frontBase + capTri[k + 1]);
        // Back faces +Z.
        m.addTriangle(backBase + capTri[k], backBase + capTri[k + 1], backBase + capTri[k + 2]);
    }

    // Side walls: a quad per boundary edge, outward normal.
    for (size_t i = 0; i < np; ++i) {
        size_t j = (i + 1) % np;
        Vec3 a0(poly[i].x, poly[i].y, 0.0f);
        Vec3 b0(poly[j].x, poly[j].y, 0.0f);
        Vec3 a1(poly[i].x, poly[i].y, thickness);
        Vec3 b1(poly[j].x, poly[j].y, thickness);
        Vec3 edge = b0 - a0;
        Vec3 nrm = edge.cross(Vec3(0, 0, 1));  // perpendicular, in-plane
        float len = nrm.length();
        nrm = len > 1e-12f ? nrm * (1.0f / len) : Vec3(1, 0, 0);

        unsigned int base = static_cast<unsigned int>(m.getVertices().size());
        m.addVertex(a0); m.addNormal(nrm);
        m.addVertex(b0); m.addNormal(nrm);
        m.addVertex(b1); m.addNormal(nrm);
        m.addVertex(a1); m.addNormal(nrm);
        m.addTriangle(base, base + 1, base + 2);
        m.addTriangle(base, base + 2, base + 3);
    }

    return m;
}

// -----------------------------------------------------------------------------
tc::Vec3 meshCentroid(const SliceMesh& sm) {
    if (sm.verts.empty()) return Vec3(0, 0, 0);
    Vec3 sum(0, 0, 0);
    for (const Vec3& v : sm.verts) sum += v;
    return sum * (1.0f / static_cast<float>(sm.verts.size()));
}

} // namespace tcx::voronoi
