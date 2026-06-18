#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi "glass break" example
//
// Click the pane and it shatters from where you clicked. The point of this
// sample is to show that *guiding* a fracture is just choosing where to put the
// seeds: tcxVoronoi never grows pattern-specific knobs — you compute a point
// cloud (here makeImpactSeeds(): dense rings at the impact, sparse outward) and
// hand it over with setSeeds(). See makeImpactSeeds() in tcApp.cpp.
//   - click: shatter from the cursor
// =============================================================================
class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void mousePressed(const MouseEventArgs& e) override;

private:
    void shatterAt(Vec2 impact);

    Rect pane_;
    Vec2 impact_;
    FractureResult2D fracture_;
    vector<Color> cellColors_;

    Tween<float> sep_;          // 0 = shards connected -> 1 = pulled apart
    unsigned int seed_ = 1;
};
