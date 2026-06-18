#pragma once

#include <TrussC.h>
#include <tcxVoronoi.h>

using namespace std;
using namespace tc;
using namespace tcx;

// =============================================================================
// tcApp - tcxVoronoi 2D example (Phase 3)
//
// Partitions a concave star into Voronoi cells with fracture2D() and draws the
// cells. 2D fracture is visible at a glance, so this stays static and minimal.
// =============================================================================
class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    FractureResult2D fracture_;
    vector<Color> colors_;
};
