#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi prismatic (glass/tile) example (Phase 4)
//
// A glass pane is already a mesh, so we fracture an existing box with a 2D
// Voronoi pattern via setSeedPlane(): all seeds lie on the front plane, so the
// cuts run straight through the slab. The shards keep the pane's real front/back
// faces. Uniform = irregular glass; Grid = regular tile cracks.
//   - d: toggle distribution (Uniform / Grid)
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
    void refracture();

    Mesh pane_;
    Distribution dist_ = Distribution::Uniform;

    FractureResult fracture_;
    vector<Color> cellColors_;

    Light light_;
    float explode_ = 0.0f;
    bool paused_ = false;
    unsigned int seed_ = 5;
    int cellCount_ = 28;
};
