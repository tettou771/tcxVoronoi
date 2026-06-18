#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 700);
    settings.setTitle("tcxVoronoi - 2D Fracture Example");

    return TC_RUN_APP(tcApp, settings);
}
