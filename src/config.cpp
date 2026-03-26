#include "panini.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void ConfigSetDefaults(PaniniConfig* cfg) {
    cfg->enabled      = false;
    cfg->strength     = 0.5f;
    cfg->verticalComp = 0.0f;
    cfg->fill         = 0.8f;
    cfg->debug        = false;
}

void LoadConfig(const char* path, PaniniConfig* cfg) {
    FILE* f = fopen(path, "r");
    if (!f) return; // File doesn't exist; keep defaults.

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char key[64];
        char val[64];
        if (sscanf(line, "%63[^=]=%63s", key, val) == 2) {
            if (strcmp(key, "enabled") == 0)
                cfg->enabled = (atoi(val) != 0);
            else if (strcmp(key, "strength") == 0)
                cfg->strength = clampf((float)atof(val), 0.0f, 1.0f);
            else if (strcmp(key, "vertical_comp") == 0)
                cfg->verticalComp = clampf((float)atof(val), -1.0f, 1.0f);
            else if (strcmp(key, "fill") == 0)
                cfg->fill = clampf((float)atof(val), 0.0f, 1.0f);
            else if (strcmp(key, "debug") == 0)
                cfg->debug = (atoi(val) != 0);
        }
    }
    fclose(f);
}
