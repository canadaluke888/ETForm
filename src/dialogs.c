#include "dialogs.h"

#include "editor_theme.h"
#include "panel_manager.h"
#include "ui_text.h"

#include <curses.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CTRL_E 5

static void append_char(char **buffer, size_t *length, size_t *capacity, char ch) {
    if (*length + 2 >= *capacity) {
        *capacity *= 2;
        char *next = realloc(*buffer, *capacity);
        if (!next) {
            return;
        }
        *buffer = next;
    }
    if (!*buffer) {
        return;
    }
    (*buffer)[(*length)++] = ch;
    (*buffer)[*length] = '\0';
}

char *ghostcode_query_dialog(const char *title, const char *initial_text) {
    size_t capacity = 256;
    size_t length = initial_text ? strlen(initial_text) : 0;
    while (capacity <= length + 2) {
        capacity *= 2;
    }
    char *buffer = calloc(capacity, 1);
    if (!buffer) {
        return NULL;
    }
    if (initial_text) {
        memcpy(buffer, initial_text, length);
    }

    keypad(stdscr, TRUE);
    int height = 7;
    int width = COLS > 56 ? 56 : COLS - 2;
    if (height > LINES) {
        height = LINES;
    }
    if (width < 28) {
        width = 28;
    }
    if (width > COLS) {
        width = COLS;
    }
    int y = (LINES - height) / 2;
    int x = (COLS - width) / 2;
    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }

    GhostcodePmNode *shadow = ghostcode_pm_add(
        y + 1, x + 2, height, width, GHOSTCODE_PM_LAYER_MODAL_SHADOW, GHOSTCODE_PM_LAYER_MODAL_SHADOW
    );
    GhostcodePmNode *modal = ghostcode_pm_add(y, x, height, width, GHOSTCODE_PM_LAYER_MODAL, GHOSTCODE_PM_LAYER_MODAL);
    if (!modal) {
        ghostcode_pm_remove(shadow);
        free(buffer);
        return NULL;
    }
    keypad(modal->win, TRUE);

    for (;;) {
        if (shadow) {
            wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
            werase(shadow->win);
        }
        wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        werase(modal->win);
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        box(modal->win, 0, 0);
        mvwprintw(modal->win, 0, 2, " %.*s ", width - 6, title);
        mvwprintw(modal->win, height - 2, 2, "[Enter] Enter  [Esc] Exit");
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));

        mvwhline(modal->win, 2, 2, ' ', width - 4);
        wmove(modal->win, 2, 2);
        size_t clipped = (size_t)(width - 5);
        if (length > clipped) {
            ghostcode_ui_text_addstr_width(modal->win, buffer + (length - clipped), width - 5);
        } else {
            ghostcode_ui_text_addstr_width(modal->win, buffer, width - 5);
        }
        int cursor_x = 2 + (int)(length > clipped ? clipped : length);
        if (cursor_x > width - 3) {
            cursor_x = width - 3;
        }
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR));
        mvwaddch(modal->win, 2, cursor_x, ' ');
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR));
        wmove(modal->win, 2, cursor_x);

        ghostcode_pm_wnoutrefresh(shadow);
        ghostcode_pm_wnoutrefresh(modal);
        ghostcode_pm_update();

        int ch = wgetch(modal->win);
        if (ch == 27) {
            free(buffer);
            ghostcode_pm_remove(modal);
            ghostcode_pm_remove(shadow);
            ghostcode_pm_update();
            return NULL;
        }
        if (ch == '\n' || ch == KEY_ENTER) {
            ghostcode_pm_remove(modal);
            ghostcode_pm_remove(shadow);
            ghostcode_pm_update();
            return buffer;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (length > 0) {
                buffer[--length] = '\0';
            }
        } else if (ch >= 32 && ch <= 126) {
            append_char(&buffer, &length, &capacity, (char)ch);
        }
    }
}

typedef struct {
    char *label;
    char *path;
    int is_dir;
    int is_parent;
} FilePickerEntry;

static void file_picker_entry_free(FilePickerEntry *entry) {
    if (!entry) {
        return;
    }
    free(entry->label);
    free(entry->path);
}

static int file_picker_entry_cmp(const void *left, const void *right) {
    const FilePickerEntry *a = (const FilePickerEntry *)left;
    const FilePickerEntry *b = (const FilePickerEntry *)right;
    if (a->is_parent != b->is_parent) {
        return b->is_parent - a->is_parent;
    }
    if (a->is_dir != b->is_dir) {
        return b->is_dir - a->is_dir;
    }
    return strcmp(a->label ? a->label : "", b->label ? b->label : "");
}

static char *path_join(const char *dir, const char *name) {
    size_t dir_len = strlen(dir ? dir : "");
    size_t name_len = strlen(name ? name : "");
    int needs_sep = dir_len > 0 && dir[dir_len - 1] != '/';
    char *path = malloc(dir_len + (size_t)needs_sep + name_len + 1);
    if (!path) {
        return NULL;
    }
    memcpy(path, dir, dir_len);
    if (needs_sep) {
        path[dir_len++] = '/';
    }
    memcpy(path + dir_len, name, name_len);
    path[dir_len + name_len] = '\0';
    return path;
}

static int append_file_picker_entry(
    FilePickerEntry **entries,
    int *count,
    int *capacity,
    const char *label,
    const char *path,
    int is_dir,
    int is_parent
) {
    if (*count >= *capacity) {
        int next_capacity = *capacity ? *capacity * 2 : 64;
        FilePickerEntry *next = realloc(*entries, (size_t)next_capacity * sizeof(FilePickerEntry));
        if (!next) {
            return 0;
        }
        *entries = next;
        *capacity = next_capacity;
    }
    FilePickerEntry *entry = &(*entries)[(*count)++];
    entry->label = strdup(label ? label : "");
    entry->path = strdup(path ? path : "");
    entry->is_dir = is_dir;
    entry->is_parent = is_parent;
    if (!entry->label || !entry->path) {
        file_picker_entry_free(entry);
        (*count)--;
        return 0;
    }
    return 1;
}

static int file_picker_is_root(const char *current_dir, const char *root_dir) {
    return current_dir && root_dir && strcmp(current_dir, root_dir) == 0;
}

static int file_picker_path_within_root(const char *path, const char *root_dir) {
    if (!path || !root_dir) {
        return 0;
    }
    size_t root_len = strlen(root_dir);
    return strncmp(path, root_dir, root_len) == 0 && (path[root_len] == '\0' || path[root_len] == '/');
}

static int file_picker_go_parent(char *current_dir, size_t current_dir_size, const char *root_dir) {
    if (!current_dir || current_dir_size == 0 || file_picker_is_root(current_dir, root_dir)) {
        return 0;
    }
    char *parent_path = path_join(current_dir, "..");
    if (!parent_path) {
        return 0;
    }
    char resolved[PATH_MAX];
    int changed = 0;
    if (realpath(parent_path, resolved) && file_picker_path_within_root(resolved, root_dir)) {
        snprintf(current_dir, current_dir_size, "%s", resolved);
        changed = 1;
    }
    free(parent_path);
    return changed;
}

static int load_file_picker_entries(
    const char *current_dir,
    const char *root_dir,
    int mode,
    FilePickerEntry **entries,
    int *count
) {
    DIR *dir = opendir(current_dir);
    if (!dir) {
        return 0;
    }
    int capacity = 0;
    *entries = NULL;
    *count = 0;
    if (!file_picker_is_root(current_dir, root_dir)) {
        char *parent_path = path_join(current_dir, "..");
        if (!parent_path) {
            closedir(dir);
            return 0;
        }
        char resolved_parent[PATH_MAX];
        int parent_ok = realpath(parent_path, resolved_parent) &&
            file_picker_path_within_root(resolved_parent, root_dir) &&
            append_file_picker_entry(entries, count, &capacity, "../", resolved_parent, 1, 1);
        free(parent_path);
        if (!parent_ok) {
            closedir(dir);
            return 0;
        }
    }

    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL) {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
            continue;
        }
        char *path = path_join(current_dir, dent->d_name);
        if (!path) {
            continue;
        }
        struct stat st;
        if (stat(path, &st) != 0) {
            free(path);
            continue;
        }
        int is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && mode == TFORM_FILE_PICKER_DIRECTORY) {
            free(path);
            continue;
        }
        char *label = NULL;
        if (is_dir) {
            size_t len = strlen(dent->d_name);
            label = malloc(len + 2);
            if (label) {
                memcpy(label, dent->d_name, len);
                label[len] = '/';
                label[len + 1] = '\0';
            }
        }
        if (!append_file_picker_entry(entries, count, &capacity, label ? label : dent->d_name, path, is_dir, 0)) {
            free(label);
            free(path);
            continue;
        }
        free(label);
        free(path);
    }
    closedir(dir);
    qsort(*entries, (size_t)*count, sizeof(FilePickerEntry), file_picker_entry_cmp);
    return 1;
}

static void free_file_picker_entries(FilePickerEntry *entries, int count) {
    for (int i = 0; i < count; i++) {
        file_picker_entry_free(&entries[i]);
    }
    free(entries);
}

static char *absolute_path_copy(const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        return strdup(resolved);
    }
    return strdup(path ? path : "");
}

char *ghostcode_file_picker_dialog(const char *title, int mode, const char *start_dir) {
    char launch_dir[PATH_MAX];
    char root_dir[PATH_MAX];
    char current_dir[PATH_MAX];
    if (!getcwd(launch_dir, sizeof(launch_dir))) {
        return NULL;
    }
    if (!realpath(start_dir && *start_dir ? start_dir : launch_dir, root_dir)) {
        snprintf(root_dir, sizeof(root_dir), "%s", launch_dir);
    }
    snprintf(current_dir, sizeof(current_dir), "%s", root_dir);

    int selected = 0;
    int top = 0;
    int height = LINES > 20 ? 20 : LINES - 2;
    int width = COLS > 84 ? 84 : COLS - 2;
    if (height < 10) {
        height = 10;
    }
    if (width < 40) {
        width = 40;
    }
    if (height > LINES) {
        height = LINES;
    }
    if (width > COLS) {
        width = COLS;
    }
    int y = (LINES - height) / 2;
    int x = (COLS - width) / 2;
    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }

    GhostcodePmNode *shadow = ghostcode_pm_add(
        y + 1, x + 2, height, width, GHOSTCODE_PM_LAYER_MODAL_SHADOW, GHOSTCODE_PM_LAYER_MODAL_SHADOW
    );
    GhostcodePmNode *modal = ghostcode_pm_add(y, x, height, width, GHOSTCODE_PM_LAYER_MODAL, GHOSTCODE_PM_LAYER_MODAL);
    if (!modal) {
        ghostcode_pm_remove(shadow);
        return NULL;
    }
    keypad(modal->win, TRUE);

    char *result = NULL;
    for (;;) {
        FilePickerEntry *entries = NULL;
        int entry_count = 0;
        if (!load_file_picker_entries(current_dir, root_dir, mode, &entries, &entry_count)) {
            break;
        }
        if (selected == 0 && entry_count > 1 && entries[0].is_parent) {
            selected = 1;
        }
        if (selected >= entry_count) {
            selected = entry_count - 1;
        }
        if (selected < 0) {
            selected = 0;
        }
        int visible = height - 6;
        if (visible < 1) {
            visible = 1;
        }
        if (selected < top) {
            top = selected;
        }
        if (selected >= top + visible) {
            top = selected - visible + 1;
        }
        int max_top = entry_count > visible ? entry_count - visible : 0;
        if (top > max_top) {
            top = max_top;
        }
        if (top < 0) {
            top = 0;
        }

        if (shadow) {
            wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
            werase(shadow->win);
        }
        wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        werase(modal->win);
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        box(modal->win, 0, 0);
        mvwprintw(modal->win, 0, 2, " %.*s ", width - 6, title ? title : "File Picker");
        mvwprintw(modal->win, 2, 2, "%.*s", width - 4, current_dir);
        const char *footer = mode == TFORM_FILE_PICKER_DIRECTORY
            ? "[Enter] Open  [B] Back  [Ctrl+E] Choose Dir  [Esc] Exit"
            : "[Enter] Open/Choose  [B] Back  [Esc] Exit";
        mvwprintw(modal->win, height - 2, 2, "%.*s", width - 4, footer);
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));

        for (int row = 0; row < visible; row++) {
            int index = top + row;
            int draw_y = 4 + row;
            mvwhline(modal->win, draw_y, 2, ' ', width - 4);
            if (index >= entry_count) {
                continue;
            }
            int attr = entries[index].is_dir ? ghostcode_color_pair(GHOSTCODE_PAIR_SYNTAX_KEYWORD) : 0;
            if (index == selected) {
                attr = ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR) | A_BOLD;
            }
            if (attr) {
                wattron(modal->win, attr);
            }
            wmove(modal->win, draw_y, 2);
            ghostcode_ui_text_addstr_width(modal->win, entries[index].label, width - 4);
            if (attr) {
                wattroff(modal->win, attr);
            }
        }

        ghostcode_pm_wnoutrefresh(shadow);
        ghostcode_pm_wnoutrefresh(modal);
        ghostcode_pm_update();

        int ch = wgetch(modal->win);
        if (ch == 27) {
            free_file_picker_entries(entries, entry_count);
            break;
        }
        if (ch == KEY_UP && entry_count > 0) {
            selected = selected > 0 ? selected - 1 : entry_count - 1;
        } else if (ch == KEY_DOWN && entry_count > 0) {
            selected = selected + 1 < entry_count ? selected + 1 : 0;
        } else if (ch == 'b' || ch == 'B') {
            if (file_picker_go_parent(current_dir, sizeof(current_dir), root_dir)) {
                selected = 0;
                top = 0;
            }
        } else if (ch == CTRL_E && mode == TFORM_FILE_PICKER_DIRECTORY && entry_count > 0) {
            if (entries[selected].is_dir) {
                result = absolute_path_copy(entries[selected].path);
            } else {
                result = absolute_path_copy(current_dir);
            }
            free_file_picker_entries(entries, entry_count);
            break;
        } else if ((ch == '\n' || ch == KEY_ENTER) && entry_count > 0) {
            if (entries[selected].is_dir) {
                char resolved[PATH_MAX];
                if (realpath(entries[selected].path, resolved) && file_picker_path_within_root(resolved, root_dir)) {
                    snprintf(current_dir, sizeof(current_dir), "%s", resolved);
                    selected = 0;
                    top = 0;
                }
            } else if (mode == TFORM_FILE_PICKER_FILE) {
                result = absolute_path_copy(entries[selected].path);
                free_file_picker_entries(entries, entry_count);
                break;
            }
        }
        free_file_picker_entries(entries, entry_count);
    }

    ghostcode_pm_remove(modal);
    ghostcode_pm_remove(shadow);
    ghostcode_pm_update();
    return result;
}

void ghostcode_message_dialog(const char *message) {
    int width = COLS > 60 ? 60 : COLS - 2;
    int height = 7;
    if (width < 24) {
        width = 24;
    }
    if (width > COLS) {
        width = COLS;
    }
    if (height > LINES) {
        height = LINES;
    }
    int y = (LINES - height) / 2;
    int x = (COLS - width) / 2;
    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }

    GhostcodePmNode *shadow = ghostcode_pm_add(
        y + 1, x + 2, height, width, GHOSTCODE_PM_LAYER_MODAL_SHADOW, GHOSTCODE_PM_LAYER_MODAL_SHADOW
    );
    GhostcodePmNode *modal = ghostcode_pm_add(y, x, height, width, GHOSTCODE_PM_LAYER_MODAL, GHOSTCODE_PM_LAYER_MODAL);
    if (!modal) {
        ghostcode_pm_remove(shadow);
        return;
    }
    keypad(modal->win, TRUE);
    if (shadow) {
        wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
        werase(shadow->win);
    }
    wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
    werase(modal->win);
    wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
    box(modal->win, 0, 0);
    mvwprintw(modal->win, height / 2, 2, "%.*s", width - 4, message);
    mvwprintw(modal->win, height - 2, 2, "Press any key");
    wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
    ghostcode_pm_wnoutrefresh(shadow);
    ghostcode_pm_wnoutrefresh(modal);
    ghostcode_pm_update();
    wgetch(modal->win);
    ghostcode_pm_remove(modal);
    ghostcode_pm_remove(shadow);
    ghostcode_pm_update();
}

void ghostcode_readonly_dialog(const char *title, const char *message) {
    int width = COLS > 72 ? 72 : COLS - 2;
    int height = LINES > 14 ? 14 : LINES - 2;
    if (width < 32) {
        width = 32;
    }
    if (width > COLS) {
        width = COLS;
    }
    if (height < 8) {
        height = 8;
    }
    if (height > LINES) {
        height = LINES;
    }
    int y = (LINES - height) / 2;
    int x = (COLS - width) / 2;
    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }

    GhostcodePmNode *shadow = ghostcode_pm_add(
        y + 1, x + 2, height, width, GHOSTCODE_PM_LAYER_MODAL_SHADOW, GHOSTCODE_PM_LAYER_MODAL_SHADOW
    );
    GhostcodePmNode *modal = ghostcode_pm_add(y, x, height, width, GHOSTCODE_PM_LAYER_MODAL, GHOSTCODE_PM_LAYER_MODAL);
    if (!modal) {
        ghostcode_pm_remove(shadow);
        return;
    }
    keypad(modal->win, TRUE);
    for (;;) {
        if (shadow) {
            wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
            werase(shadow->win);
        }
        wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        werase(modal->win);
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        box(modal->win, 0, 0);
        mvwprintw(modal->win, 0, 2, " %.*s ", width - 6, title ? title : "Info");
        mvwprintw(modal->win, height - 2, 2, "[Enter] Close  [Esc] Close");
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));

        int row = 2;
        int col = 2;
        int max_col = width - 3;
        for (const char *p = message ? message : ""; *p && row < height - 3; p++) {
            if (*p == '\n' || col >= max_col) {
                row++;
                col = 2;
                if (*p == '\n') {
                    continue;
                }
            }
            if (row < height - 3) {
                mvwaddch(modal->win, row, col++, *p);
            }
        }
        ghostcode_pm_wnoutrefresh(shadow);
        ghostcode_pm_wnoutrefresh(modal);
        ghostcode_pm_update();
        int ch = wgetch(modal->win);
        if (ch == 27 || ch == '\n' || ch == KEY_ENTER) {
            break;
        }
    }
    ghostcode_pm_remove(modal);
    ghostcode_pm_remove(shadow);
    ghostcode_pm_update();
}
