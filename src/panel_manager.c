#include "panel_manager.h"

#include <stdlib.h>

#define TFORM_PM_MAX 64

static TFormPmNode *TFORM_PM_LIST[TFORM_PM_MAX];
static int TFORM_PM_COUNT = 0;
static int TFORM_PM_NEXT_ORDER = 1;

static int compare_nodes(const void *left, const void *right) {
    const TFormPmNode *a = *(TFormPmNode * const *)left;
    const TFormPmNode *b = *(TFormPmNode * const *)right;
    if (a->z_index < b->z_index) {
        return -1;
    }
    if (a->z_index > b->z_index) {
        return 1;
    }
    if (a->order < b->order) {
        return -1;
    }
    if (a->order > b->order) {
        return 1;
    }
    return 0;
}

static void restack(void) {
    qsort(TFORM_PM_LIST, (size_t)TFORM_PM_COUNT, sizeof(TFormPmNode *), compare_nodes);
    for (int i = 0; i < TFORM_PM_COUNT; i++) {
        top_panel(TFORM_PM_LIST[i]->panel);
    }
}

void tform_pm_init(void) {
    TFORM_PM_COUNT = 0;
    TFORM_PM_NEXT_ORDER = 1;
}

void tform_pm_teardown(void) {
    for (int i = 0; i < TFORM_PM_COUNT; i++) {
        if (!TFORM_PM_LIST[i]) {
            continue;
        }
        del_panel(TFORM_PM_LIST[i]->panel);
        delwin(TFORM_PM_LIST[i]->win);
        free(TFORM_PM_LIST[i]);
        TFORM_PM_LIST[i] = NULL;
    }
    TFORM_PM_COUNT = 0;
}

TFormPmNode *tform_pm_add(int y, int x, int h, int w, int z_index, TFormPmLayerRole role) {
    if (TFORM_PM_COUNT >= TFORM_PM_MAX || h < 1 || w < 1) {
        return NULL;
    }

    WINDOW *win = newwin(h, w, y, x);
    if (!win) {
        return NULL;
    }
    PANEL *panel = new_panel(win);
    if (!panel) {
        delwin(win);
        return NULL;
    }

    TFormPmNode *node = calloc(1, sizeof(TFormPmNode));
    if (!node) {
        del_panel(panel);
        delwin(win);
        return NULL;
    }

    node->panel = panel;
    node->win = win;
    node->z_index = z_index;
    node->role = role;
    node->y = y;
    node->x = x;
    node->h = h;
    node->w = w;
    node->order = TFORM_PM_NEXT_ORDER++;
    node->visible = true;
    TFORM_PM_LIST[TFORM_PM_COUNT++] = node;
    restack();
    return node;
}

void tform_pm_remove(TFormPmNode *node) {
    if (!node) {
        return;
    }

    int index = -1;
    for (int i = 0; i < TFORM_PM_COUNT; i++) {
        if (TFORM_PM_LIST[i] == node) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        for (int i = index; i < TFORM_PM_COUNT - 1; i++) {
            TFORM_PM_LIST[i] = TFORM_PM_LIST[i + 1];
        }
        TFORM_PM_COUNT--;
    }

    del_panel(node->panel);
    delwin(node->win);
    free(node);
    restack();
}

void tform_pm_show(TFormPmNode *node) {
    if (!node) {
        return;
    }
    show_panel(node->panel);
    node->visible = true;
}

void tform_pm_hide(TFormPmNode *node) {
    if (!node) {
        return;
    }
    hide_panel(node->panel);
    node->visible = false;
}

void tform_pm_move(TFormPmNode *node, int y, int x) {
    if (!node) {
        return;
    }
    node->y = y;
    node->x = x;
    move_panel(node->panel, y, x);
}

void tform_pm_resize(TFormPmNode *node, int h, int w) {
    if (!node || h < 1 || w < 1) {
        return;
    }
    node->h = h;
    node->w = w;
    wresize(node->win, h, w);
    replace_panel(node->panel, node->win);
}

void tform_pm_wnoutrefresh(TFormPmNode *node) {
    if (!node) {
        return;
    }
    wnoutrefresh(node->win);
}

void tform_pm_update(void) {
    restack();
    update_panels();
    doupdate();
}

void tform_pm_center(TFormPmNode *node) {
    if (!node) {
        return;
    }
    int y = (LINES - node->h) / 2;
    int x = (COLS - node->w) / 2;
    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }
    tform_pm_move(node, y, x);
}

void tform_pm_on_resize(void) {
    restack();
}
