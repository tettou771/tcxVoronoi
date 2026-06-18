#pragma once

// Umbrella header for tcxVoronoi.
// NOTE: this deliberately does NOT include tcxVoronoiPhysics.h. Physics
// integration is opt-in: include <tcxVoronoiPhysics.h> explicitly (and add
// tcxPhysics to addons.make) only when you want it, so that tcxVoronoi stays
// usable with zero physics dependency.

#include <TrussC.h>

#include "tcxVoronoiTypes.h"
#include "tcxVoronoiSlice.h"

#include <vector>

namespace tcx {

// How auto-generated seeds are distributed inside the mesh bounds.
// Poisson / Surface are planned; Uniform and Grid cover Phase 1.
enum class Distribution { Uniform, Grid };

// =============================================================================
// Voronoi - configurable fracture generator.
//
// The object IS the settings: configure it with chained setters, then run it
// with fracture(). It can be reused across multiple meshes.
//
//   // simple
//   auto r = voronoiFracture(mesh, 30);
//
//   // controlled
//   Voronoi v;
//   v.addSeed(impactPoint)        // your own points
//    .setSeedCount(40)            // fill the rest
//    .setDistribution(Distribution::Uniform)
//    .setRandomSeed(123);
//   FractureResult r = v.fracture(mesh);
//
//   // escape hatch: specify every seed yourself
//   v.setSeeds(myPoints);
// =============================================================================
class Voronoi {
public:
    Voronoi() = default;

    // Accumulate explicit seed points (kept; the rest are auto-filled to reach
    // setSeedCount()). Vec2 overloads store z=0 for use with fracture2D().
    Voronoi& addSeed(const tc::Vec3& p) { seeds_.push_back(p); return *this; }
    Voronoi& addSeed(const tc::Vec2& p) { seeds_.push_back(tc::Vec3(p.x, p.y, 0.0f)); return *this; }
    Voronoi& addSeeds(const std::vector<tc::Vec3>& ps) {
        seeds_.insert(seeds_.end(), ps.begin(), ps.end());
        return *this;
    }
    Voronoi& addSeeds(const std::vector<tc::Vec2>& ps) {
        for (const tc::Vec2& p : ps) seeds_.push_back(tc::Vec3(p.x, p.y, 0.0f));
        return *this;
    }

    // Total target seed count. Remaining (count - explicit) seeds are generated
    // by the chosen distribution within the mesh bounds.
    Voronoi& setSeedCount(int n) { seedCount_ = n; return *this; }
    Voronoi& setDistribution(Distribution d) { distribution_ = d; return *this; }
    Voronoi& setRandomSeed(unsigned int s) { randomSeed_ = s; hasRandomSeed_ = true; return *this; }

    // Replace ALL seeds with exactly these points; disables auto-fill. This is
    // the escape hatch for full manual control.
    Voronoi& setSeeds(const std::vector<tc::Vec3>& ps) {
        seeds_ = ps;
        explicitSeeds_ = true;
        return *this;
    }
    Voronoi& setSeeds(const std::vector<tc::Vec2>& ps) {
        seeds_.clear();
        for (const tc::Vec2& p : ps) seeds_.push_back(tc::Vec3(p.x, p.y, 0.0f));
        explicitSeeds_ = true;
        return *this;
    }

    // The seeds that will be used for the next fracture (after auto-fill is
    // resolved this is populated; before, it reflects what you've added).
    const std::vector<tc::Vec3>& seeds() const { return seeds_; }

    // Fracture a (closed) mesh into Voronoi fragments.
    FractureResult fracture(const tc::Mesh& mesh);

    // Partition a 2D region (a closed Path) into Voronoi cells. The region may
    // be concave; the outer boundary (first subpath) is used.
    FractureResult2D fracture2D(const tc::Path& region);

private:
    std::vector<tc::Vec3> resolveSeeds(const tc::Mesh& mesh);
    std::vector<tc::Vec2> resolveSeeds2D(const std::vector<tc::Vec2>& region);

    std::vector<tc::Vec3> seeds_;
    int seedCount_ = 0;
    Distribution distribution_ = Distribution::Uniform;
    unsigned int randomSeed_ = 0;
    bool hasRandomSeed_ = false;
    bool explicitSeeds_ = false;
};

// -----------------------------------------------------------------------------
// Convenience free functions for the common case.
// -----------------------------------------------------------------------------
inline FractureResult voronoiFracture(const tc::Mesh& mesh, int count) {
    return Voronoi().setSeedCount(count).fracture(mesh);
}

inline FractureResult2D voronoiFracture2D(const tc::Path& region, int count) {
    return Voronoi().setSeedCount(count).fracture2D(region);
}

// -----------------------------------------------------------------------------
// Seed-generation helpers (compose freely, feed into addSeeds()/setSeeds()).
// -----------------------------------------------------------------------------

// n points spread roughly evenly on a sphere (Fibonacci sphere).
std::vector<tc::Vec3> seedsOnSphere(const tc::Vec3& center, float radius, int n);

} // namespace tcx
