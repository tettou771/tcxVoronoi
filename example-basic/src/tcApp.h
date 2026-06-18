#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi basic example (Phase 1: 3D convex fracture)
//
// Fractures a box into Voronoi cells and animates them exploding apart and
// reassembling, so the fracture is obvious at a glance.
//   - space: pause / resume the explode animation
//   - r: re-fracture with a new random seed set
// =============================================================================
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void refracture();

    FractureResult fracture_;
    vector<Color> cellColors_;

    Light light_;
    float explode_ = 0.0f;
    bool paused_ = false;
    unsigned int seed_ = 7;
    int cellCount_ = 25;
};
