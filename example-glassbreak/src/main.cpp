#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(900, 700);
    settings.setTitle("tcxVoronoi - Glass Break");

    return TC_RUN_APP(tcApp, settings);
}
