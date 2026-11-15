#ifndef ETFORM_PANEL_MANAGER_H
#define ETFORM_PANEL_MANAGER_H

#include <curses.h>
#include <panel.h>
#include <stdbool.h>

typedef enum {
    ETFORM_PM_LAYER_MAIN = 10,
    ETFORM_PM_LAYER_FOOTER = 30,
    ETFORM_PM_LAYER_MODAL_SHADOW = 995,
    ETFORM_PM_LAYER_MODAL = 1000
} ETFormPmLayerRole;

typedef struct {
    PANEL *panel;
    WINDOW *win;
    int z_index;
    ETFormPmLayerRole role;
    int y;
    int x;
    int h;
    int w;
    int order;
    bool visible;
} ETFormPmNode;

void etform_pm_init(void);
void etform_pm_teardown(void);
ETFormPmNode *etform_pm_add(int y, int x, int h, int w, int z_index, ETFormPmLayerRole role);
void etform_pm_remove(ETFormPmNode *node);
void etform_pm_show(ETFormPmNode *node);
void etform_pm_hide(ETFormPmNode *node);
void etform_pm_move(ETFormPmNode *node, int y, int x);
void etform_pm_resize(ETFormPmNode *node, int h, int w);
void etform_pm_wnoutrefresh(ETFormPmNode *node);
void etform_pm_update(void);
void etform_pm_center(ETFormPmNode *node);
void etform_pm_on_resize(void);

#endif
