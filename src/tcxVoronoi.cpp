#include "tcxVoronoi.h"

#include <algorithm>
#include <cmath>
#include <map>

using namespace std;
using namespace tc;

namespace tcx::voronoi {

namespace {

struct Bounds {
    Vec3 mn{0, 0, 0};
    Vec3 mx{0, 0, 0};
    Vec3 size() const { return mx - mn; }
    float diagonal() const { return (mx - mn).length(); }
};

Bounds computeBounds(const Mesh& mesh) {
    Bounds b;
    const vector<Vec3>& v = mesh.getVertices();
    if (v.empty()) return b;
    b.mn = b.mx = v[0];
    for (const Vec3& p : v) {
        b.mn.x = min(b.mn.x, p.x); b.mn.y = min(b.mn.y, p.y); b.mn.z = min(b.mn.z, p.z);
        b.mx.x = max(b.mx.x, p.x); b.mx.y = max(b.mx.y, p.y); b.mx.z = max(b.mx.z, p.z);
    }
    return b;
}

Vec3 projectOntoPlane(const Vec3& p, const Plane& plane) {
    return p - plane.normal * plane.signedDistance(p);
}

} // namespace

// -----------------------------------------------------------------------------
vector<Vec3> Voronoi::resolveSeeds(const Mesh& mesh) {
    vector<Vec3> result = seeds_;  // explicit points come first, always kept

    // With a seed plane, all seeds (explicit + generated) must be coplanar so
    // the resulting bisector cuts are perpendicular to it (prismatic).
    if (hasSeedPlane_) {
        for (Vec3& p : result) p = projectOntoPlane(p, seedPlane_);
    }
    if (explicitSeeds_) return result;

    int target = max(seedCount_, static_cast<int>(result.size()));
    if (target < 1) target = 1;

    if (hasRandomSeed_) tc::randomSeed(randomSeed_);

    if (hasSeedPlane_) {
        // Distribute generated seeds in 2D on the plane, within the mesh's
        // projected bounds.
        const Vec3 n = seedPlane_.normal;
        const Vec3 origin = n * (-seedPlane_.d);
        Vec3 u = (fabs(n.x) > 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        u = (u - n * u.dot(n)).normalized();
        Vec3 v = n.cross(u);

        float smn = 0, smx = 0, tmn = 0, tmx = 0;
        bool first = true;
        for (const Vec3& q : mesh.getVertices()) {
            Vec3 dq = q - origin;
            float s = dq.dot(u), tt = dq.dot(v);
            if (first) { smn = smx = s; tmn = tmx = tt; first = false; }
            else {
                smn = min(smn, s); smx = max(smx, s);
                tmn = min(tmn, tt); tmx = max(tmx, tt);
            }
        }
        auto place = [&](float s, float tt) { result.push_back(origin + u * s + v * tt); };

        if (distribution_ == Distribution::Grid) {
            int gn = max(1, static_cast<int>(ceil(sqrt(static_cast<double>(target)))));
            float cs = (smx - smn) / gn, ct = (tmx - tmn) / gn;
            for (int x = 0; x < gn && static_cast<int>(result.size()) < target; ++x)
            for (int y = 0; y < gn && static_cast<int>(result.size()) < target; ++y) {
                place(smn + (x + tc::random()) * cs, tmn + (y + tc::random()) * ct);
            }
        }
        while (static_cast<int>(result.size()) < target) {
            place(tc::random(smn, smx), tc::random(tmn, tmx));
        }
        return result;
    }

    Bounds b = computeBounds(mesh);

    if (distribution_ == Distribution::Grid) {
        // Jittered grid: roughly cube-root cells, jitter inside each cell.
        Vec3 sz = b.size();
        int gn = max(1, static_cast<int>(ceil(cbrt(static_cast<double>(target)))));
        Vec3 cell(sz.x / gn, sz.y / gn, sz.z / gn);
        for (int x = 0; x < gn && static_cast<int>(result.size()) < target; ++x)
        for (int y = 0; y < gn && static_cast<int>(result.size()) < target; ++y)
        for (int z = 0; z < gn && static_cast<int>(result.size()) < target; ++z) {
            result.push_back(Vec3(
                b.mn.x + (x + tc::random()) * cell.x,
                b.mn.y + (y + tc::random()) * cell.y,
                b.mn.z + (z + tc::random()) * cell.z));
        }
    }

    // Uniform fill (also tops up Grid if it under-shot).
    while (static_cast<int>(result.size()) < target) {
        result.push_back(Vec3(
            tc::random(b.mn.x, b.mx.x),
            tc::random(b.mn.y, b.mx.y),
            tc::random(b.mn.z, b.mx.z)));
    }
    return result;
}

// -----------------------------------------------------------------------------
FractureResult Voronoi::fracture(const Mesh& mesh) {
    FractureResult result;

    vector<Vec3> seeds = resolveSeeds(mesh);
    const int n = static_cast<int>(seeds.size());
    if (n == 0) return result;

    SliceMesh base = toSliceMesh(mesh);
    float eps = max(1e-5f, computeBounds(mesh).diagonal() * 1e-6f);
    float interfaceEps = computeBounds(mesh).diagonal() * 1e-3f;

    // Accumulator for shared interfaces, keyed by ordered cell pair.
    struct Acc { Vec3 sum{0, 0, 0}; int count = 0; Vec3 normal{0, 0, 0}; };
    map<pair<int, int>, Acc> ifaceAcc;

    // First pass: build each cell's geometry by cascading plane slices.
    vector<SliceMesh> cellMeshes(n);
    for (int i = 0; i < n; ++i) {
        SliceMesh cell = base;
        for (int j = 0; j < n && !cell.empty(); ++j) {
            if (j == i) continue;
            cell = sliceKeepInside(cell, Plane::bisector(seeds[i], seeds[j]), eps);
        }
        cellMeshes[i] = std::move(cell);
    }

    // Map seed index -> emitted cell index (empty cells are dropped, so the two
    // index spaces differ; neighbors/interfaces are stored per-seed and remapped
    // to cell indices at the end).
    vector<int> seedToCell(n, -1);
    int cellCount = 0;
    for (int i = 0; i < n; ++i) {
        if (!cellMeshes[i].empty()) seedToCell[i] = cellCount++;
    }

    // Per-seed neighbor sets (seed indices).
    vector<vector<bool>> neighborOf(n, vector<bool>());

    // Second pass: classify faces to recover accurate neighbors + interfaces.
    // A face is the (i,j) interface when its centroid is equidistant from seed i
    // and the nearest other seed j.
    for (int i = 0; i < n; ++i) {
        SliceMesh& cell = cellMeshes[i];
        if (cell.empty()) continue;

        vector<bool>& isNeighbor = neighborOf[i];
        isNeighbor.assign(n, false);
        const unsigned int* tri = cell.tris.data();
        const size_t nt = cell.tris.size();
        for (size_t t = 0; t < nt; t += 3) {
            Vec3 c = (cell.verts[tri[t]] + cell.verts[tri[t + 1]] + cell.verts[tri[t + 2]])
                     * (1.0f / 3.0f);
            float di = c.distance(seeds[i]);
            int bestJ = -1;
            float bestErr = interfaceEps;
            for (int j = 0; j < n; ++j) {
                if (j == i || seedToCell[j] < 0) continue;
                float err = fabs(c.distance(seeds[j]) - di);
                if (err < bestErr) { bestErr = err; bestJ = j; }
            }
            if (bestJ >= 0) {
                isNeighbor[bestJ] = true;
                int a = min(i, bestJ), bb = max(i, bestJ);
                Acc& acc = ifaceAcc[{a, bb}];
                acc.sum += c;
                acc.count++;
                acc.normal = Plane::bisector(seeds[a], seeds[bb]).normal;
            }
        }
    }

    // Emit cells in seed order, remapping neighbor seed indices to cell indices.
    result.cells.reserve(cellCount);
    for (int i = 0; i < n; ++i) {
        if (seedToCell[i] < 0) continue;
        VoronoiCell vc;
        vc.mesh = toRenderMesh(cellMeshes[i]);
        vc.seed = seeds[i];
        vc.centroid = meshCentroid(cellMeshes[i]);
        for (int j = 0; j < n; ++j) {
            if (j < static_cast<int>(neighborOf[i].size()) && neighborOf[i][j] && seedToCell[j] >= 0) {
                vc.neighbors.push_back(seedToCell[j]);
            }
        }
        result.cells.push_back(std::move(vc));
    }

    // Emit deduped interfaces (remapped to cell indices).
    for (auto& kv : ifaceAcc) {
        int a = seedToCell[kv.first.first];
        int b = seedToCell[kv.first.second];
        if (a < 0 || b < 0) continue;
        Interface it;
        it.cellA = a;
        it.cellB = b;
        it.point = kv.second.count > 0
                       ? kv.second.sum * (1.0f / static_cast<float>(kv.second.count))
                       : Vec3(0, 0, 0);
        it.normal = kv.second.normal;
        result.interfaces.push_back(it);
    }

    return result;
}

// -----------------------------------------------------------------------------
FractureResult Voronoi::fractureExtruded(const Path& region, float thickness) {
    Mesh slab = extrudePath(region, thickness);
    // Constrain seeds to the front (Z=0) plane so the cuts run straight through
    // the slab (prismatic). Honor an explicit setSeedPlane() if one was set.
    if (!hasSeedPlane_) {
        setSeedPlane(Plane::fromPointNormal(Vec3(0, 0, 0), Vec3(0, 0, 1)));
    }
    return fracture(slab);
}

// =============================================================================
// 2D fracture
// =============================================================================
namespace {

struct Line2 {
    Vec2 normal;
    float d;
    float sd(const Vec2& p) const { return normal.dot(p) + d; }
    // Perpendicular bisector; sd < 0 on a's side.
    static Line2 bisector(const Vec2& a, const Vec2& b) {
        Vec2 n = (b - a).normalized();
        Vec2 m = (a + b) * 0.5f;
        return {n, -n.dot(m)};
    }
};

// Clip a polygon by a half-plane, keeping sd <= eps (one Sutherland-Hodgman
// edge step). Handles concave subjects.
vector<Vec2> clipByHalfPlane(const vector<Vec2>& poly, const Line2& ln, float eps) {
    vector<Vec2> out;
    size_t m = poly.size();
    if (m < 2) return out;
    for (size_t i = 0; i < m; ++i) {
        const Vec2& cur = poly[i];
        const Vec2& nxt = poly[(i + 1) % m];
        float dc = ln.sd(cur), dn = ln.sd(nxt);
        if (dc <= eps) out.push_back(cur);
        bool crossing = (dc > eps && dn < -eps) || (dc < -eps && dn > eps);
        if (crossing) {
            float t = dc / (dc - dn);
            out.push_back(cur.lerp(nxt, t));
        }
    }
    return out;
}

bool pointInPoly(const Vec2& pt, const vector<Vec2>& poly) {
    bool c = false;
    size_t m = poly.size();
    for (size_t i = 0, j = m - 1; i < m; j = i++) {
        if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
            (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) /
                        (poly[j].y - poly[i].y) + poly[i].x)) {
            c = !c;
        }
    }
    return c;
}

} // namespace

vector<Vec2> Voronoi::resolveSeeds2D(const vector<Vec2>& region) {
    vector<Vec2> result;
    for (const Vec3& s : seeds_) result.push_back(Vec2(s.x, s.y));
    if (explicitSeeds_) return result;

    int target = max(seedCount_, static_cast<int>(result.size()));
    if (target < 1) target = 1;

    Vec2 mn = region[0], mx = region[0];
    for (const Vec2& p : region) {
        mn.x = min(mn.x, p.x); mn.y = min(mn.y, p.y);
        mx.x = max(mx.x, p.x); mx.y = max(mx.y, p.y);
    }
    if (hasRandomSeed_) tc::randomSeed(randomSeed_);

    // Rejection-sample inside the polygon so seeds land in the region.
    int guard = 0, guardMax = target * 200 + 100;
    while (static_cast<int>(result.size()) < target && guard < guardMax) {
        ++guard;
        Vec2 p(tc::random(mn.x, mx.x), tc::random(mn.y, mx.y));
        if (pointInPoly(p, region)) result.push_back(p);
    }
    // Fallback for thin/degenerate polygons where rejection keeps missing.
    while (static_cast<int>(result.size()) < target) {
        result.push_back(Vec2(tc::random(mn.x, mx.x), tc::random(mn.y, mx.y)));
    }
    return result;
}

FractureResult2D Voronoi::fracture2D(const Path& region) {
    FractureResult2D result;

    // Outer boundary polygon (first subpath).
    vector<Vec2> poly;
    const vector<Vec3>& rv = region.getVertices();
    if (region.getNumSubpaths() > 0) {
        auto rng = region.getSubpathRange(0);
        for (size_t i = rng.first; i < rng.second; ++i) poly.push_back(Vec2(rv[i].x, rv[i].y));
    } else {
        for (const Vec3& v : rv) poly.push_back(Vec2(v.x, v.y));
    }
    if (poly.size() < 3) return result;

    vector<Vec2> seeds = resolveSeeds2D(poly);
    const int n = static_cast<int>(seeds.size());
    if (n == 0) return result;

    Rect b = region.getBounds();
    float diag = Vec2(b.width, b.height).length();
    float eps = max(1e-5f, diag * 1e-6f);
    float interfaceEps = diag * 1e-3f;

    // First pass: clip the region by each bisector half-plane.
    vector<vector<Vec2>> cellPolys(n);
    for (int i = 0; i < n; ++i) {
        vector<Vec2> cell = poly;
        for (int j = 0; j < n && cell.size() >= 3; ++j) {
            if (j == i) continue;
            cell = clipByHalfPlane(cell, Line2::bisector(seeds[i], seeds[j]), eps);
        }
        cellPolys[i] = std::move(cell);
    }

    vector<int> seedToCell(n, -1);
    int cellCount = 0;
    for (int i = 0; i < n; ++i) {
        if (cellPolys[i].size() >= 3) seedToCell[i] = cellCount++;
    }

    struct Acc { Vec2 sum{0, 0}; int count = 0; Vec2 normal{0, 0}; };
    map<pair<int, int>, Acc> ifaceAcc;
    vector<vector<bool>> neighborOf(n, vector<bool>());

    // Second pass: classify edges -> neighbors + interfaces.
    for (int i = 0; i < n; ++i) {
        if (seedToCell[i] < 0) continue;
        const vector<Vec2>& cell = cellPolys[i];
        vector<bool>& isNeighbor = neighborOf[i];
        isNeighbor.assign(n, false);
        size_t m = cell.size();
        for (size_t e = 0; e < m; ++e) {
            Vec2 mid = (cell[e] + cell[(e + 1) % m]) * 0.5f;
            float di = mid.distance(seeds[i]);
            int bestJ = -1;
            float bestErr = interfaceEps;
            for (int j = 0; j < n; ++j) {
                if (j == i || seedToCell[j] < 0) continue;
                float err = fabs(mid.distance(seeds[j]) - di);
                if (err < bestErr) { bestErr = err; bestJ = j; }
            }
            if (bestJ >= 0) {
                isNeighbor[bestJ] = true;
                int a = min(i, bestJ), bb = max(i, bestJ);
                Acc& acc = ifaceAcc[{a, bb}];
                acc.sum += mid;
                acc.count++;
                acc.normal = Line2::bisector(seeds[a], seeds[bb]).normal;
            }
        }
    }

    // Emit cells.
    result.cells.reserve(cellCount);
    for (int i = 0; i < n; ++i) {
        if (seedToCell[i] < 0) continue;
        VoronoiCell2D vc;
        Path p(cellPolys[i]);
        p.close();
        vc.path = p;
        vc.seed = seeds[i];
        Vec2 c(0, 0);
        for (const Vec2& q : cellPolys[i]) c += q;
        vc.centroid = c * (1.0f / static_cast<float>(cellPolys[i].size()));
        for (int j = 0; j < n; ++j) {
            if (j < static_cast<int>(neighborOf[i].size()) && neighborOf[i][j] && seedToCell[j] >= 0) {
                vc.neighbors.push_back(seedToCell[j]);
            }
        }
        result.cells.push_back(std::move(vc));
    }

    // Emit interfaces.
    for (auto& kv : ifaceAcc) {
        int a = seedToCell[kv.first.first];
        int bb = seedToCell[kv.first.second];
        if (a < 0 || bb < 0) continue;
        Interface2D it;
        it.cellA = a;
        it.cellB = bb;
        it.point = kv.second.count > 0
                       ? kv.second.sum * (1.0f / static_cast<float>(kv.second.count))
                       : Vec2(0, 0);
        it.normal = kv.second.normal;
        result.interfaces.push_back(it);
    }

    return result;
}

// -----------------------------------------------------------------------------
vector<Vec3> seedsOnSphere(const Vec3& center, float radius, int n) {
    vector<Vec3> out;
    if (n <= 0) return out;
    const float golden = static_cast<float>(TAU) * 0.6180339887498949f;
    for (int i = 0; i < n; ++i) {
        float t = (i + 0.5f) / static_cast<float>(n);
        float z = 1.0f - 2.0f * t;             // -1..1
        float r = sqrt(max(0.0f, 1.0f - z * z));
        float phi = golden * i;
        out.push_back(center + Vec3(cos(phi) * r, sin(phi) * r, z) * radius);
    }
    return out;
}

} // namespace tcx::voronoi
