#include "tcApp.h"

// ===========================================================================
// makeImpactSeeds  --  THE point of this example.
//
// Guiding a fracture = deciding where the seeds go. tcxVoronoi just consumes a
// point cloud, so this recipe lives here in user code, not in the addon.
//
// Radial-crack look: crucially we do NOT seed the impact itself. We place seeds
// in a ring AROUND it, so the impact lands where several cell edges meet -> the
// cracks radiate OUT of the click as long, thin wedges. A second, sparser ring
// further out adds the concentric breaks real glass has; a few stray points
// give the outer plates. Few seeds = long shards.
// ===========================================================================
static vector<Vec2> makeImpactSeeds(Vec2 impact, const Rect& bounds) {
    vector<Vec2> pts;
    float scale = min(bounds.width, bounds.height);

    // Ring 1 (the radial wedges): seeds sit away from the click, so the impact
    // becomes a junction of cell edges and each cell is a long thin spoke.
    const int spokes = 12;
    const float r1 = scale * 0.16f;
    float phase = tc::random(TAU);
    for (int i = 0; i < spokes; ++i) {
        float a = phase + (i + tc::random(-0.18f, 0.18f)) / spokes * TAU;
        pts.push_back(impact + Vec2(cos(a), sin(a)) * (r1 * tc::random(0.85f, 1.15f)));
    }

    // Ring 2 (concentric break): sparser, larger, angle staggered between spokes.
    const int spokes2 = 7;
    const float r2 = scale * 0.42f;
    float phase2 = phase + TAU / (spokes2 * 2);
    for (int i = 0; i < spokes2; ++i) {
        float a = phase2 + (i + tc::random(-0.25f, 0.25f)) / spokes2 * TAU;
        pts.push_back(impact + Vec2(cos(a), sin(a)) * (r2 * tc::random(0.8f, 1.2f)));
    }

    // A few stray points for the outer plates.
    for (int i = 0; i < 4; ++i) {
        pts.push_back(Vec2(tc::random(bounds.x, bounds.getRight()),
                           tc::random(bounds.y, bounds.getBottom())));
    }
    return pts;  // ~23 seeds
}

// ---------------------------------------------------------------------------
void tcApp::setup() {
    logNotice("tcApp") << "tcxVoronoi glass-break example - click the pane to shatter";

    float w = getWindowWidth(), h = getWindowHeight();
    pane_ = Rect(w * 0.12f, h * 0.12f, w * 0.76f, h * 0.76f);

    shatterAt(pane_.getCenter());  // start pre-cracked from the middle
}

// ---------------------------------------------------------------------------
void tcApp::shatterAt(Vec2 impact) {
    impact_ = impact;

    // Build the pane outline as a Path, fracture with our hand-built seeds.
    Path pane;
    pane.addVertex(Vec2(pane_.x, pane_.y));
    pane.addVertex(Vec2(pane_.getRight(), pane_.y));
    pane.addVertex(Vec2(pane_.getRight(), pane_.getBottom()));
    pane.addVertex(Vec2(pane_.x, pane_.getBottom()));
    pane.close();

    tc::randomSeed(seed_++);
    Voronoi v;
    v.setSeeds(makeImpactSeeds(impact, pane_));  // <- our point cloud, verbatim
    fracture_ = v.fracture2D(pane);

    cellColors_.clear();
    int n = static_cast<int>(fracture_.cells.size());
    for (int i = 0; i < n; ++i) {
        // Cool, faintly-varied translucent glass.
        float tint = 0.52f + 0.06f * sin(i * 1.7f);
        cellColors_.push_back(Color::fromHSB(tint, 0.18f, 0.95f, 0.55f));
    }

    // Animate the shards apart with a TrussC Tween (auto-updates each frame).
    sep_.from(0.0f).to(1.0f).duration(0.9f).ease(EaseType::Cubic, EaseMode::Out).start();
}

// ---------------------------------------------------------------------------
void tcApp::update() {
}

// ---------------------------------------------------------------------------
void tcApp::draw() {
    clear(0.05f, 0.06f, 0.09f);

    // On click the pane starts whole (connected) and the shards drift apart:
    // the Tween eases separation 0 -> 1 over 0.9s (Cubic ease-out).
    float sep = sep_.getValue();

    for (size_t i = 0; i < fracture_.cells.size(); ++i) {
        const VoronoiCell2D& cell = fracture_.cells[i];

        // Each shard slides away from the impact along its radial direction, so
        // the pane visibly comes apart from the connected state.
        Vec2 dir = cell.centroid - impact_;
        float len = dir.length();
        dir = len > 1e-3f ? dir * (1.0f / len) : Vec2(0, 0);
        Vec2 offset = dir * (sep * 38.0f);

        pushMatrix();
        translate(offset.x, offset.y, 0.0f);
        setColor(cellColors_[i]);
        cell.path.drawFill();
        setColor(0.02f, 0.03f, 0.05f, 0.9f);  // dark crack lines
        cell.path.drawStroke();
        popMatrix();
    }

    setColor(1.0f, 1.0f, 1.0f);
    drawBitmapString("tcxVoronoi - glass break (click the pane)", 12, 22);
    drawBitmapString("seeds computed in makeImpactSeeds() - see tcApp.cpp", 12, 40);
}

// ---------------------------------------------------------------------------
void tcApp::mousePressed(const MouseEventArgs& e) {
    // Clamp the impact into the pane and shatter from there.
    Vec2 p(clamp(e.x, pane_.x, pane_.getRight()),
           clamp(e.y, pane_.y, pane_.getBottom()));
    shatterAt(p);
}
