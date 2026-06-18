#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 600);
    settings.setTitle("tcxVoronoi - Concave Fracture Example");

    return TC_RUN_APP(tcApp, settings);
}
