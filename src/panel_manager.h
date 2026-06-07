#ifndef GHOSTCODE_PANEL_MANAGER_H
#define GHOSTCODE_PANEL_MANAGER_H

#include <curses.h>
#include <panel.h>
#include <stdbool.h>

typedef enum {
    GHOSTCODE_PM_LAYER_MAIN = 10,
    GHOSTCODE_PM_LAYER_FOOTER = 30,
    GHOSTCODE_PM_LAYER_MODAL_SHADOW = 995,
    GHOSTCODE_PM_LAYER_MODAL = 1000
} GhostcodePmLayerRole;

typedef struct {
    PANEL *panel;
    WINDOW *win;
    int z_index;
    GhostcodePmLayerRole role;
    int y;
    int x;
    int h;
    int w;
    int order;
    bool visible;
} GhostcodePmNode;

void ghostcode_pm_init(void);
void ghostcode_pm_teardown(void);
GhostcodePmNode *ghostcode_pm_add(int y, int x, int h, int w, int z_index, GhostcodePmLayerRole role);
void ghostcode_pm_remove(GhostcodePmNode *node);
void ghostcode_pm_show(GhostcodePmNode *node);
void ghostcode_pm_hide(GhostcodePmNode *node);
void ghostcode_pm_move(GhostcodePmNode *node, int y, int x);
void ghostcode_pm_resize(GhostcodePmNode *node, int h, int w);
void ghostcode_pm_wnoutrefresh(GhostcodePmNode *node);
void ghostcode_pm_update(void);
void ghostcode_pm_center(GhostcodePmNode *node);
void ghostcode_pm_on_resize(void);

#endif
