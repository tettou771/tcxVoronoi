#include "tcApp.h"

// ---------------------------------------------------------------------------
void tcApp::setup() {
    logNotice("tcApp") << "tcxVoronoi basic example (3D convex fracture)";
    logNotice("tcApp") << "  - space: pause/resume explode";
    logNotice("tcApp") << "  - r: re-fracture";

    light_.setDirectional(Vec3(-1, 1, -1));
    light_.setAmbient(0.18f, 0.18f, 0.22f);
    light_.setDiffuse(1.0f, 1.0f, 0.95f);
    light_.setSpecular(1.0f, 1.0f, 1.0f);

    refracture();
}

// ---------------------------------------------------------------------------
void tcApp::refracture() {
    Mesh box = createBox(220.0f);

    // The Voronoi generator holds the fracture settings (chained setters) and
    // can be reused across meshes. Here: N cells, reproducible RNG.
    Voronoi v;
    v.setSeedCount(cellCount_).setRandomSeed(seed_);
    fracture_ = v.fracture(box);

    // ---- More control --------------------------------------------------------
    //   v.addSeed({0, 100, 0})          // place your own seed(s) — e.g. an
    //    .addSeeds(seedsOnSphere({0,0,0}, 80, 12))  // a ring of impact seeds
    //    .setSeedCount(40)              // total cells; the rest are auto-filled
    //    .setDistribution(Distribution::Uniform);   // Uniform (default) or Grid
    //
    //   v.setSeeds(myPoints);           // escape hatch: every seed yourself
    //                                   // (replaces all, disables auto-fill)
    //
    // Seeds you add are always kept; setSeedCount() tops up the remainder.
    //
    // Result: fracture_.cells[i] has { mesh, seed, centroid, neighbors }.
    //   fracture_.interfaces  : shared faces { cellA, cellB, point, normal }
    //   fracture_.neighborsOf(i) / interfaceBetween(a, b)
    //   (handy for spawning effects between pieces as they separate)
    // --------------------------------------------------------------------------

    // One stable color per cell (spread around the hue wheel).
    cellColors_.clear();
    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        float h = static_cast<float>(i) / max(1, n);
        cellColors_.push_back(Color::fromHSB(h, 0.55f, 0.95f));
    }

    logNotice("tcApp") << "fractured into " << n << " cells, "
                       << fracture_.interfaces.size() << " interfaces";
}

// ---------------------------------------------------------------------------
void tcApp::update() {
    if (!paused_) {
        // Ping-pong 0..1: pieces fly apart, then reassemble.
        explode_ = 0.5f - 0.5f * cos(getElapsedTime() * 0.8f);
    }
}

// ---------------------------------------------------------------------------
void tcApp::draw() {
    clear(0.08f, 0.08f, 0.1f);

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;

    addLight(light_);
    setCameraPosition(cx, cy, 1000);

    float t = getElapsedTime();

    pushMatrix();
    translate(cx, cy, 0.0f);
    rotateY(sin(t * 0.3f));
    rotateX(cos(t * 0.18f) * 0.5f);

    const float spread = 1.4f;
    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        const VoronoiCell& cell = fracture_.cells[i];
        pushMatrix();
        // Explode each piece outward along its centroid direction.
        translate(cell.centroid * (explode_ * spread));
        Material mat = Material::plastic(cellColors_[i]);
        setMaterial(mat);
        cell.mesh.draw();
        popMatrix();
    }
    popMatrix();

    clearMaterial();
    clearLights();

    setColor(1.0f, 1.0f, 1.0f);
    float y = 20;
    drawBitmapString("tcxVoronoi - 3D convex fracture", 10, y); y += 16;
    drawBitmapString("cells: " + toString((int)fracture_.cells.size()), 10, y); y += 16;
    drawBitmapString("explode: " + toString(explode_, 2), 10, y); y += 16;
    drawBitmapString("space: pause   r: re-fracture", 10, y); y += 16;
}

// ---------------------------------------------------------------------------
void tcApp::keyPressed(int key) {
    if (key == ' ') {
        paused_ = !paused_;
    } else if (key == 'r' || key == 'R') {
        seed_ += 1;
        refracture();
    }
}
