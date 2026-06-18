#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 640);
    settings.setTitle("tcxVoronoi - Prismatic Glass Fracture");

    return TC_RUN_APP(tcApp, settings);
}
