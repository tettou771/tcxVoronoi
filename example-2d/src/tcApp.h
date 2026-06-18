#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi 2D example (Phase 3)
//
// Partitions a 2D shape (a concave star Path) into Voronoi cells via
// fracture2D(), then animates the cells exploding apart and back together.
//   - space: pause / resume explode
//   - r: re-fracture with a new random seed set
//   - s: toggle shape (star / blob)
// =============================================================================
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void rebuildShape();
    void refracture();

    Path shape_;
    int shapeKind_ = 0;  // 0=star 1=blob

    FractureResult2D fracture_;
    vector<Color> cellColors_;

    float explode_ = 0.0f;
    bool paused_ = false;
    unsigned int seed_ = 1;
    int cellCount_ = 32;
};
