#include "panel_manager.h"

#include <stdlib.h>

#define ETFORM_PM_MAX 64

static ETFormPmNode *ETFORM_PM_LIST[ETFORM_PM_MAX];
static int ETFORM_PM_COUNT = 0;
static int ETFORM_PM_NEXT_ORDER = 1;

static int compare_nodes(const void *left, const void *right) {
    const ETFormPmNode *a = *(ETFormPmNode * const *)left;
    const ETFormPmNode *b = *(ETFormPmNode * const *)right;
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
    qsort(ETFORM_PM_LIST, (size_t)ETFORM_PM_COUNT, sizeof(ETFormPmNode *), compare_nodes);
    for (int i = 0; i < ETFORM_PM_COUNT; i++) {
        top_panel(ETFORM_PM_LIST[i]->panel);
    }
}

void etform_pm_init(void) {
    ETFORM_PM_COUNT = 0;
    ETFORM_PM_NEXT_ORDER = 1;
}

void etform_pm_teardown(void) {
    for (int i = 0; i < ETFORM_PM_COUNT; i++) {
        if (!ETFORM_PM_LIST[i]) {
            continue;
        }
        del_panel(ETFORM_PM_LIST[i]->panel);
        delwin(ETFORM_PM_LIST[i]->win);
        free(ETFORM_PM_LIST[i]);
        ETFORM_PM_LIST[i] = NULL;
    }
    ETFORM_PM_COUNT = 0;
}

ETFormPmNode *etform_pm_add(int y, int x, int h, int w, int z_index, ETFormPmLayerRole role) {
    if (ETFORM_PM_COUNT >= ETFORM_PM_MAX || h < 1 || w < 1) {
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

    ETFormPmNode *node = calloc(1, sizeof(ETFormPmNode));
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
    node->order = ETFORM_PM_NEXT_ORDER++;
    node->visible = true;
    ETFORM_PM_LIST[ETFORM_PM_COUNT++] = node;
    restack();
    return node;
}

void etform_pm_remove(ETFormPmNode *node) {
    if (!node) {
        return;
    }

    int index = -1;
    for (int i = 0; i < ETFORM_PM_COUNT; i++) {
        if (ETFORM_PM_LIST[i] == node) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        for (int i = index; i < ETFORM_PM_COUNT - 1; i++) {
            ETFORM_PM_LIST[i] = ETFORM_PM_LIST[i + 1];
        }
        ETFORM_PM_COUNT--;
    }

    del_panel(node->panel);
    delwin(node->win);
    free(node);
    restack();
}

void etform_pm_show(ETFormPmNode *node) {
    if (!node) {
        return;
    }
    show_panel(node->panel);
    node->visible = true;
}

void etform_pm_hide(ETFormPmNode *node) {
    if (!node) {
        return;
    }
    hide_panel(node->panel);
    node->visible = false;
}

void etform_pm_move(ETFormPmNode *node, int y, int x) {
    if (!node) {
        return;
    }
    node->y = y;
    node->x = x;
    move_panel(node->panel, y, x);
}

void etform_pm_resize(ETFormPmNode *node, int h, int w) {
    if (!node || h < 1 || w < 1) {
        return;
    }
    node->h = h;
    node->w = w;
    wresize(node->win, h, w);
    replace_panel(node->panel, node->win);
}

void etform_pm_wnoutrefresh(ETFormPmNode *node) {
    if (!node) {
        return;
    }
    wnoutrefresh(node->win);
}

void etform_pm_update(void) {
    restack();
    update_panels();
    doupdate();
}

void etform_pm_center(ETFormPmNode *node) {
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
    etform_pm_move(node, y, x);
}

void etform_pm_on_resize(void) {
    restack();
}
