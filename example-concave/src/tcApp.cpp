#include "tcApp.h"

// ---------------------------------------------------------------------------
void tcApp::setup() {
    logNotice("tcApp") << "tcxVoronoi concave example";
    logNotice("tcApp") << "  - m: next mesh   space: pause   r: re-fracture";

    light_.setDirectional(Vec3(-1, 1, -1));
    light_.setAmbient(0.18f, 0.18f, 0.22f);
    light_.setDiffuse(1.0f, 1.0f, 0.95f);
    light_.setSpecular(1.0f, 1.0f, 1.0f);

    rebuildMesh();
    refracture();
}

// ---------------------------------------------------------------------------
void tcApp::rebuildMesh() {
    switch (meshKind_) {
        case 0: source_ = createBox(220.0f);                 meshName_ = "box";      break;
        case 1: source_ = createSphere(130.0f, 24);          meshName_ = "sphere";   break;
        case 2: source_ = createTorus(120.0f, 50.0f, 32, 20); meshName_ = "torus";    break;
        default: source_ = createCylinder(90.0f, 220.0f, 28); meshName_ = "cylinder"; break;
    }
}

// ---------------------------------------------------------------------------
void tcApp::refracture() {
    Voronoi v;
    v.setSeedCount(cellCount_).setRandomSeed(seed_);
    fracture_ = v.fracture(source_);

    cellColors_.clear();
    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        float h = static_cast<float>(i) / max(1, n);
        cellColors_.push_back(Color::fromHSB(h, 0.55f, 0.95f));
    }

    logNotice("tcApp") << meshName_ << " -> " << n << " cells, "
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

    addLight(light_);
    setCameraPosition(cx, cy, 1000);

    float t = getElapsedTime();

    pushMatrix();
    translate(cx, cy, 0.0f);
    rotateY(sin(t * 0.3f));
    rotateX(cos(t * 0.18f) * 0.5f);

    const float spread = 1.5f;
    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        const VoronoiCell& cell = fracture_.cells[i];
        pushMatrix();
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
    drawBitmapString("tcxVoronoi - concave fracture", 10, y); y += 16;
    drawBitmapString(string("mesh: ") + meshName_ + " (m to change)", 10, y); y += 16;
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
    } else if (key == 'm' || key == 'M') {
        meshKind_ = (meshKind_ + 1) % 4;
        rebuildMesh();
        refracture();
    }
}
