#ifndef TFORM_PANEL_MANAGER_H
#define TFORM_PANEL_MANAGER_H

#include <curses.h>
#include <panel.h>
#include <stdbool.h>

typedef enum {
    TFORM_PM_LAYER_MAIN = 10,
    TFORM_PM_LAYER_FOOTER = 30,
    TFORM_PM_LAYER_MODAL_SHADOW = 995,
    TFORM_PM_LAYER_MODAL = 1000
} TFormPmLayerRole;

typedef struct {
    PANEL *panel;
    WINDOW *win;
    int z_index;
    TFormPmLayerRole role;
    int y;
    int x;
    int h;
    int w;
    int order;
    bool visible;
} TFormPmNode;

void tform_pm_init(void);
void tform_pm_teardown(void);
TFormPmNode *tform_pm_add(int y, int x, int h, int w, int z_index, TFormPmLayerRole role);
void tform_pm_remove(TFormPmNode *node);
void tform_pm_show(TFormPmNode *node);
void tform_pm_hide(TFormPmNode *node);
void tform_pm_move(TFormPmNode *node, int y, int x);
void tform_pm_resize(TFormPmNode *node, int h, int w);
void tform_pm_wnoutrefresh(TFormPmNode *node);
void tform_pm_update(void);
void tform_pm_center(TFormPmNode *node);
void tform_pm_on_resize(void);

#endif
