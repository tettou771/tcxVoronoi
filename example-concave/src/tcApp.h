#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi concave example (Phase 2)
//
// Fractures a concave mesh (torus by default) into Voronoi cells. The same
// slicer handles concave cross-sections (multi-loop / holed caps) via earcut.
// Cycle through box / sphere / torus / cylinder to compare convex vs concave.
//   - m: next mesh
//   - space: pause / resume explode
//   - r: re-fracture with a new random seed set
// =============================================================================
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void rebuildMesh();
    void refracture();

    Mesh source_;
    int meshKind_ = 2;  // 0=box 1=sphere 2=torus 3=cylinder
    const char* meshName_ = "torus";

    FractureResult fracture_;
    vector<Color> cellColors_;

    Light light_;
    float explode_ = 0.0f;
    bool paused_ = false;
    unsigned int seed_ = 3;
    int cellCount_ = 40;
};
