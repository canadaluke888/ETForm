#include "panel_manager.h"

#include <stdlib.h>

#define GHOSTCODE_PM_MAX 64

static GhostcodePmNode *GHOSTCODE_PM_LIST[GHOSTCODE_PM_MAX];
static int GHOSTCODE_PM_COUNT = 0;
static int GHOSTCODE_PM_NEXT_ORDER = 1;

static int compare_nodes(const void *left, const void *right) {
    const GhostcodePmNode *a = *(GhostcodePmNode * const *)left;
    const GhostcodePmNode *b = *(GhostcodePmNode * const *)right;
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
    qsort(GHOSTCODE_PM_LIST, (size_t)GHOSTCODE_PM_COUNT, sizeof(GhostcodePmNode *), compare_nodes);
    for (int i = 0; i < GHOSTCODE_PM_COUNT; i++) {
        top_panel(GHOSTCODE_PM_LIST[i]->panel);
    }
}

void ghostcode_pm_init(void) {
    GHOSTCODE_PM_COUNT = 0;
    GHOSTCODE_PM_NEXT_ORDER = 1;
}

void ghostcode_pm_teardown(void) {
    for (int i = 0; i < GHOSTCODE_PM_COUNT; i++) {
        if (!GHOSTCODE_PM_LIST[i]) {
            continue;
        }
        del_panel(GHOSTCODE_PM_LIST[i]->panel);
        delwin(GHOSTCODE_PM_LIST[i]->win);
        free(GHOSTCODE_PM_LIST[i]);
        GHOSTCODE_PM_LIST[i] = NULL;
    }
    GHOSTCODE_PM_COUNT = 0;
}

GhostcodePmNode *ghostcode_pm_add(int y, int x, int h, int w, int z_index, GhostcodePmLayerRole role) {
    if (GHOSTCODE_PM_COUNT >= GHOSTCODE_PM_MAX || h < 1 || w < 1) {
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

    GhostcodePmNode *node = calloc(1, sizeof(GhostcodePmNode));
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
    node->order = GHOSTCODE_PM_NEXT_ORDER++;
    node->visible = true;
    GHOSTCODE_PM_LIST[GHOSTCODE_PM_COUNT++] = node;
    restack();
    return node;
}

void ghostcode_pm_remove(GhostcodePmNode *node) {
    if (!node) {
        return;
    }

    int index = -1;
    for (int i = 0; i < GHOSTCODE_PM_COUNT; i++) {
        if (GHOSTCODE_PM_LIST[i] == node) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        for (int i = index; i < GHOSTCODE_PM_COUNT - 1; i++) {
            GHOSTCODE_PM_LIST[i] = GHOSTCODE_PM_LIST[i + 1];
        }
        GHOSTCODE_PM_COUNT--;
    }

    del_panel(node->panel);
    delwin(node->win);
    free(node);
    restack();
}

void ghostcode_pm_show(GhostcodePmNode *node) {
    if (!node) {
        return;
    }
    show_panel(node->panel);
    node->visible = true;
}

void ghostcode_pm_hide(GhostcodePmNode *node) {
    if (!node) {
        return;
    }
    hide_panel(node->panel);
    node->visible = false;
}

void ghostcode_pm_move(GhostcodePmNode *node, int y, int x) {
    if (!node) {
        return;
    }
    node->y = y;
    node->x = x;
    move_panel(node->panel, y, x);
}

void ghostcode_pm_resize(GhostcodePmNode *node, int h, int w) {
    if (!node || h < 1 || w < 1) {
        return;
    }
    node->h = h;
    node->w = w;
    wresize(node->win, h, w);
    replace_panel(node->panel, node->win);
}

void ghostcode_pm_wnoutrefresh(GhostcodePmNode *node) {
    if (!node) {
        return;
    }
    wnoutrefresh(node->win);
}

void ghostcode_pm_update(void) {
    restack();
    update_panels();
    doupdate();
}

void ghostcode_pm_center(GhostcodePmNode *node) {
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
    ghostcode_pm_move(node, y, x);
}

void ghostcode_pm_on_resize(void) {
    restack();
}
