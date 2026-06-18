#include "tcxVoronoiSlice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>

using namespace std;
using namespace tc;

namespace tcx {

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

    // Cut segments along the plane, collected to stitch into cap loop(s).
    vector<pair<unsigned int, unsigned int>> cutSegs;

    const unsigned int* tri = in.tris.data();
    const size_t nt = in.tris.size();
    for (size_t t = 0; t < nt; t += 3) {
        unsigned int idx[3] = {tri[t], tri[t + 1], tri[t + 2]};

        // Sutherland-Hodgman clip of the triangle against the half-space
        // (keep signedDistance <= eps). Output polygon (<= 4 verts), plus the
        // pair of cut vertices that form this triangle's cut segment.
        unsigned int poly[4];
        int polyN = 0;
        unsigned int cut[2];
        int cutN = 0;

        for (int e = 0; e < 3; ++e) {
            unsigned int cur = idx[e];
            unsigned int nxt = idx[(e + 1) % 3];
            float dCur = dist[cur];
            float dNxt = dist[nxt];
            bool curIn = dCur <= eps;
            bool nxtIn = dNxt <= eps;

            if (curIn) poly[polyN++] = keepVertex(cur);

            // Genuine crossing (strictly opposite sides beyond eps).
            bool crossing = (dCur > eps && dNxt < -eps) || (dCur < -eps && dNxt > eps);
            if (crossing) {
                unsigned int c = cutOnEdge(cur, nxt);
                if (polyN < 4) poly[polyN++] = c;
                if (cutN < 2) cut[cutN++] = c;
            }
        }

        if (polyN >= 3) {
            // Fan triangulate the kept polygon.
            for (int k = 1; k + 1 < polyN; ++k) {
                out.tris.push_back(poly[0]);
                out.tris.push_back(poly[k]);
                out.tris.push_back(poly[k + 1]);
            }
        }
        if (cutN == 2) {
            cutSegs.emplace_back(cut[0], cut[1]);
        }
    }

    if (cutSegs.empty()) return out;  // plane did not actually cut: no cap

    // -------------------------------------------------------------------------
    // Build cap(s): stitch cut segments into closed loop(s), then triangulate.
    // -------------------------------------------------------------------------
    unordered_map<unsigned int, vector<unsigned int>> adj;
    for (auto& s : cutSegs) {
        adj[s.first].push_back(s.second);
        adj[s.second].push_back(s.first);
    }

    unordered_map<unsigned int, bool> used;
    const Vec3 n = plane.normal;

    for (auto& kv : adj) {
        unsigned int start = kv.first;
        if (used[start]) continue;

        // Walk the (degree-2) cycle starting at 'start'.
        vector<unsigned int> loop;
        unsigned int cur = start;
        unsigned int prev = numeric_limits<unsigned int>::max();
        while (true) {
            loop.push_back(cur);
            used[cur] = true;
            unsigned int next = numeric_limits<unsigned int>::max();
            for (unsigned int w : adj[cur]) {
                if (w != prev) { next = w; break; }
            }
            if (next == numeric_limits<unsigned int>::max()) break;
            if (next == start) break;       // closed
            if (used[next]) break;          // ran into an already-walked vertex
            prev = cur;
            cur = next;
        }

        if (loop.size() < 3) continue;

        // Fan triangulate the loop (Phase 1: convex caps). Orient each triangle
        // so its normal points toward +plane.normal (outward of the kept solid).
        for (size_t k = 1; k + 1 < loop.size(); ++k) {
            unsigned int a = loop[0], b = loop[k], c = loop[k + 1];
            Vec3 fn = (out.verts[b] - out.verts[a]).cross(out.verts[c] - out.verts[a]);
            if (fn.dot(n) >= 0.0f) {
                out.tris.push_back(a);
                out.tris.push_back(b);
                out.tris.push_back(c);
            } else {
                out.tris.push_back(a);
                out.tris.push_back(c);
                out.tris.push_back(b);
            }
        }
    }

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
tc::Vec3 meshCentroid(const SliceMesh& sm) {
    if (sm.verts.empty()) return Vec3(0, 0, 0);
    Vec3 sum(0, 0, 0);
    for (const Vec3& v : sm.verts) sum += v;
    return sum * (1.0f / static_cast<float>(sm.verts.size()));
}

} // namespace tcx
