#include "tcApp.h"

// ---------------------------------------------------------------------------
void tcApp::setup() {
    logNotice("tcApp") << "tcxVoronoi prismatic (glass) example";
    logNotice("tcApp") << "  - d: distribution   space: pause   r: re-fracture";

    light_.setDirectional(Vec3(-1, 1, -1));
    light_.setAmbient(0.18f, 0.18f, 0.22f);
    light_.setDiffuse(1.0f, 1.0f, 0.95f);
    light_.setSpecular(1.0f, 1.0f, 1.0f);

    // The glass pane is just a thin box — an ordinary mesh, like real geometry.
    pane_ = createBox(420.0f, 300.0f, 26.0f);
    refracture();
}

// ---------------------------------------------------------------------------
void tcApp::refracture() {
    // Prismatic fracture of an existing mesh: constrain seeds to the front
    // (Z=0) plane, so the Voronoi cuts run straight through the pane and each
    // shard keeps the pane's real front/back faces.
    Voronoi v;
    v.setSeedPlane(Plane::fromPointNormal(Vec3(0, 0, 0), Vec3(0, 0, 1)))
     .setDistribution(dist_)
     .setSeedCount(cellCount_)
     .setRandomSeed(seed_);
    fracture_ = v.fracture(pane_);

    // (If you DON'T already have a mesh, build one from a 2D outline instead:
    //    fracture_ = Voronoi().setSeedCount(28).fractureExtruded(outlinePath, 26);)

    cellColors_.clear();
    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        float h = static_cast<float>(i) / max(1, n);
        cellColors_.push_back(Color::fromHSB(h, 0.45f, 0.96f));
    }

    logNotice("tcApp") << (dist_ == Distribution::Grid ? "grid" : "uniform")
                       << " -> " << n << " shards, "
                       << fracture_.interfaces.size() << " interfaces";
}

// ---------------------------------------------------------------------------
void tcApp::update() {
    if (!paused_) {
        explode_ = 0.5f - 0.5f * cos(getElapsedTime() * 0.8f);
    }
}

// ---------------------------------------------------------------------------
void tcApp::draw() {
    clear(0.07f, 0.07f, 0.09f);

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;

    addLight(light_);
    setCameraPosition(cx, cy, 1000);

    float t = getElapsedTime();

    pushMatrix();
    translate(cx, cy, 0.0f);
    rotateY(sin(t * 0.25f) * 0.6f);
    rotateX(0.35f + cos(t * 0.15f) * 0.2f);

    const float spread = 1.3f;
    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        const VoronoiCell& cell = fracture_.cells[i];
        pushMatrix();
        // Shards spread apart in the plane (their centroids sit near Z=0).
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
    drawBitmapString("tcxVoronoi - prismatic glass/tile fracture", 10, y); y += 16;
    drawBitmapString(string("distribution: ") +
                     (dist_ == Distribution::Grid ? "Grid (tile)" : "Uniform (glass)") +
                     " (d)", 10, y); y += 16;
    drawBitmapString("shards: " + toString((int)fracture_.cells.size()), 10, y); y += 16;
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
    } else if (key == 'd' || key == 'D') {
        dist_ = (dist_ == Distribution::Uniform) ? Distribution::Grid : Distribution::Uniform;
        refracture();
    }
}
