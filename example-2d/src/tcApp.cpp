#include "tcApp.h"

// ---------------------------------------------------------------------------
void tcApp::setup() {
    logNotice("tcApp") << "tcxVoronoi 2D example";
    logNotice("tcApp") << "  - s: shape   space: pause   r: re-fracture";
    rebuildShape();
    refracture();
}

// ---------------------------------------------------------------------------
void tcApp::rebuildShape() {
    // Built centered at the origin; translated to screen center when drawn.
    shape_.clear();
    if (shapeKind_ == 0) {
        // Concave 5-point star.
        const int points = 5;
        const float outer = 240.0f, inner = 100.0f;
        for (int i = 0; i < points * 2; ++i) {
            float a = static_cast<float>(i) / (points * 2) * TAU - TAU * 0.25f;
            float r = (i % 2 == 0) ? outer : inner;
            shape_.addVertex(Vec2(cos(a) * r, sin(a) * r));
        }
    } else {
        // Wobbly blob.
        const int n = 48;
        for (int i = 0; i < n; ++i) {
            float a = static_cast<float>(i) / n * TAU;
            float r = 200.0f + 40.0f * sin(a * 3.0f) + 25.0f * cos(a * 7.0f);
            shape_.addVertex(Vec2(cos(a) * r, sin(a) * r));
        }
    }
    shape_.close();
}

// ---------------------------------------------------------------------------
void tcApp::refracture() {
    Voronoi v;
    v.setSeedCount(cellCount_).setRandomSeed(seed_);
    fracture_ = v.fracture2D(shape_);

    cellColors_.clear();
    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        float h = static_cast<float>(i) / max(1, n);
        cellColors_.push_back(Color::fromHSB(h, 0.55f, 0.95f));
    }

    logNotice("tcApp") << "2D shape -> " << n << " cells, "
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
    clear(0.08f, 0.08f, 0.1f);

    float cx = getWindowWidth() / 2.0f;
    float cy = getWindowHeight() / 2.0f;

    pushMatrix();
    translate(cx, cy, 0.0f);

    const float spread = 1.2f;
    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        VoronoiCell2D& cell = fracture_.cells[i];
        pushMatrix();
        translate(cell.centroid * (explode_ * spread));
        setColor(cellColors_[i]);
        cell.path.drawFill();
        setColor(0.0f, 0.0f, 0.0f, 0.35f);
        cell.path.drawStroke();
        popMatrix();
    }
    popMatrix();

    setColor(1.0f, 1.0f, 1.0f);
    float y = 20;
    drawBitmapString("tcxVoronoi - 2D fracture", 10, y); y += 16;
    drawBitmapString(string("shape: ") + (shapeKind_ == 0 ? "star" : "blob") + " (s)", 10, y); y += 16;
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
    } else if (key == 's' || key == 'S') {
        shapeKind_ = (shapeKind_ + 1) % 2;
        rebuildShape();
        refracture();
    }
}
