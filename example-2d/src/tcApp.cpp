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

    fracture_ = voronoiFracture2D(star, 32);

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
