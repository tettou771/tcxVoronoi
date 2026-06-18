#include "tcApp.h"

// ---------------------------------------------------------------------------
void tcApp::setup() {
    // Concave 5-point star, built directly in screen coordinates.
    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;

    Path star;
    const int points = 5;
    for (int i = 0; i < points * 2; ++i) {
        float a = static_cast<float>(i) / (points * 2) * TAU - TAU * 0.25f;
        float r = (i % 2 == 0) ? 240.0f : 100.0f;
        star.addVertex(Vec2(cx + cos(a) * r, cy + sin(a) * r));
    }
    star.close();

    // Simplest form: 32 randomly-placed cells.
    fracture_ = voronoiFracture2D(star, 32);

    // ---- Controlling the fracture --------------------------------------------
    // For more control, build a Voronoi generator and configure it with chained
    // setters (the object holds the settings and can be reused):
    //
    //   Voronoi v;
    //   v.addSeed(impactPoint)          // place your own seed(s), e.g. a hit point
    //    .setSeedCount(40)              // total cells; the rest are auto-filled
    //    .setDistribution(Distribution::Uniform)  // Uniform (default) or Grid
    //    .setRandomSeed(123);           // fix the RNG for reproducible results
    //   fracture_ = v.fracture2D(star);
    //
    // Seed rules:
    //   - addSeed/addSeeds points are always kept; setSeedCount() tops up the
    //     remainder with the chosen Distribution (auto-generated inside the shape).
    //   - setSeeds(points) replaces EVERYTHING and disables auto-fill — the
    //     escape hatch when you want to control every single seed:
    //       v.setSeeds({ {x0,y0}, {x1,y1}, ... });
    //
    // Result: fracture_.cells[i] has { path, seed, centroid, neighbors }.
    //   fracture_.interfaces  : shared edges { cellA, cellB, point, normal }
    //   fracture_.neighborsOf(i) / interfaceBetween(a, b)
    // --------------------------------------------------------------------------

    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        colors_.push_back(Color::fromHSB(static_cast<float>(i) / max(1, n), 0.55f, 0.95f));
    }
}

// ---------------------------------------------------------------------------
void tcApp::draw() {
    clear(0.08f, 0.08f, 0.1f);

    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        setColor(colors_[i]);
        fracture_.cells[i].path.drawFill();
        setColor(0.0f, 0.0f, 0.0f, 0.35f);
        fracture_.cells[i].path.drawStroke();
    }

    setColor(1.0f, 1.0f, 1.0f);
    drawBitmapString("tcxVoronoi - 2D fracture (" +
                     toString((int)fracture_.cells.size()) + " cells)", 10, 20);
}
