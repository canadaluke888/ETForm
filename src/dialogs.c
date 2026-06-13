#include "dialogs.h"

#include "braille_canvas.h"
#include "editor_theme.h"
#include "panel_manager.h"
#include "ui_text.h"

#include <curses.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
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

static int calendar_is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int calendar_days_in_month(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && calendar_is_leap_year(year)) {
        return 29;
    }
    if (month < 1 || month > 12) {
        return 31;
    }
    return days[month - 1];
}

static void calendar_clamp_date(int *month, int *day, int *year) {
    if (*year < 1) {
        *year = 1;
    } else if (*year > 9999) {
        *year = 9999;
    }
    if (*month < 1) {
        *month = 12;
    } else if (*month > 12) {
        *month = 1;
    }
    int max_day = calendar_days_in_month(*month, *year);
    if (*day < 1) {
        *day = 1;
    } else if (*day > max_day) {
        *day = max_day;
    }
}

static int calendar_first_weekday(int month, int year) {
    struct tm date;
    memset(&date, 0, sizeof(date));
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = 1;
    date.tm_isdst = -1;
    if (mktime(&date) == (time_t)-1) {
        return 0;
    }
    return date.tm_wday;
}

static void calendar_parse_initial(const char *initial_date, int *month, int *day, int *year) {
    int parsed_year = 0;
    int parsed_month = 0;
    int parsed_day = 0;
    if (initial_date && sscanf(initial_date, "%d-%d-%d", &parsed_year, &parsed_month, &parsed_day) == 3) {
        *year = parsed_year;
        *month = parsed_month;
        *day = parsed_day;
        calendar_clamp_date(month, day, year);
        return;
    }
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    if (local) {
        *year = local->tm_year + 1900;
        *month = local->tm_mon + 1;
        *day = local->tm_mday;
    } else {
        *year = 1970;
        *month = 1;
        *day = 1;
    }
    calendar_clamp_date(month, day, year);
}

static char *calendar_format_date(int month, int day, int year) {
    char *result = calloc(11, 1);
    if (!result) {
        return NULL;
    }
    snprintf(result, 11, "%04d-%02d-%02d", year, month, day);
    return result;
}

static void calendar_adjust_numeric(int *month, int *day, int *year, int part, int delta) {
    if (part == 0) {
        *month += delta;
        if (*month < 1) {
            *month = 12;
        } else if (*month > 12) {
            *month = 1;
        }
    } else if (part == 1) {
        *day += delta;
    } else {
        *year += delta;
    }
    calendar_clamp_date(month, day, year);
}

static void calendar_move_day(int *day, int month, int year, int delta) {
    int max_day = calendar_days_in_month(month, year);
    *day += delta;
    if (*day < 1) {
        *day = 1;
    } else if (*day > max_day) {
        *day = max_day;
    }
}

static void calendar_draw_repeat(WINDOW *win, const char *text, int count) {
    if (!win || !text || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        waddstr(win, text);
    }
}

static void calendar_draw_rule(
    WINDOW *win,
    int y,
    int x,
    int cell_w,
    const char *left,
    const char *mid,
    const char *right,
    const char *fill
) {
    if (!win || cell_w < 2) {
        return;
    }
    wmove(win, y, x);
    waddstr(win, left);
    for (int col = 0; col < 7; col++) {
        calendar_draw_repeat(win, fill, cell_w - 1);
        waddstr(win, col == 6 ? right : mid);
    }
}

char *ghostcode_calendar_dialog(const char *title, const char *initial_date) {
    int month = 1;
    int day = 1;
    int year = 1970;
    calendar_parse_initial(initial_date, &month, &day, &year);
    int selected_day = day;
    int focus_top = 1;
    int numeric_part = 0;

    int height = LINES > 23 ? 23 : LINES - 1;
    int width = COLS > 78 ? 78 : COLS - 2;
    if (height < 21) {
        height = 21;
    }
    if (width < 56) {
        width = 56;
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

    for (;;) {
        selected_day = day;
        int max_day = calendar_days_in_month(month, year);
        if (selected_day > max_day) {
            selected_day = max_day;
            day = selected_day;
        }

        if (shadow) {
            wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
            werase(shadow->win);
        }
        wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        werase(modal->win);
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        box(modal->win, 0, 0);
        mvwprintw(modal->win, 0, 2, " %.*s ", width - 6, title ? title : "Calendar");
        mvwprintw(modal->win, height - 2, 2, "[Tab] Section  [Arrows] Change  [Enter] Select  [Esc] Done");
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));

        int top_y = 2;
        int top_x = 4;
        int top_w = width - 8;
        mvwaddch(modal->win, top_y, top_x, ACS_ULCORNER);
        mvwhline(modal->win, top_y, top_x + 1, ACS_HLINE, top_w - 2);
        mvwaddch(modal->win, top_y, top_x + top_w - 1, ACS_URCORNER);
        mvwaddch(modal->win, top_y + 2, top_x, ACS_LLCORNER);
        mvwhline(modal->win, top_y + 2, top_x + 1, ACS_HLINE, top_w - 2);
        mvwaddch(modal->win, top_y + 2, top_x + top_w - 1, ACS_LRCORNER);
        mvwaddch(modal->win, top_y + 1, top_x, ACS_VLINE);
        mvwaddch(modal->win, top_y + 1, top_x + top_w - 1, ACS_VLINE);
        int month_x = top_x + 3;
        int day_x = top_x + top_w / 3 + 3;
        int year_x = top_x + (top_w * 2) / 3 + 3;
        mvwprintw(modal->win, top_y + 1, month_x, "Month:");
        mvwprintw(modal->win, top_y + 1, day_x, "Day:");
        mvwprintw(modal->win, top_y + 1, year_x, "Year:");
        int active_attr = ghostcode_color_pair(GHOSTCODE_PAIR_SYNTAX_KEYWORD) | A_BOLD;
        if (focus_top && numeric_part == 0) {
            wattron(modal->win, active_attr);
        }
        mvwprintw(modal->win, top_y + 1, month_x + 7, "%02d", month);
        if (focus_top && numeric_part == 0) {
            wattroff(modal->win, active_attr);
        }
        if (focus_top && numeric_part == 1) {
            wattron(modal->win, active_attr);
        }
        mvwprintw(modal->win, top_y + 1, day_x + 5, "%02d", day);
        if (focus_top && numeric_part == 1) {
            wattroff(modal->win, active_attr);
        }
        if (focus_top && numeric_part == 2) {
            wattron(modal->win, active_attr);
        }
        mvwprintw(modal->win, top_y + 1, year_x + 6, "%04d", year);
        if (focus_top && numeric_part == 2) {
            wattroff(modal->win, active_attr);
        }

        int grid_y = 6;
        int cell_w = (width - 8) / 7;
        if (cell_w < 6) {
            cell_w = 6;
        }
        int grid_w = cell_w * 7 + 1;
        int grid_x = (width - grid_w) / 2;
        if (grid_x < 2) {
            grid_x = 2;
        }
        const char *headers[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        int grid_attr = ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG);
        wattron(modal->win, grid_attr);
        calendar_draw_rule(modal->win, grid_y, grid_x, cell_w, "┏", "┳", "┓", "━");
        for (int col = 0; col < 7; col++) {
            int cell_x = grid_x + col * cell_w;
            mvwaddstr(modal->win, grid_y + 1, cell_x, col == 0 ? "┃" : "┃");
            int label_x = cell_x + 1 + ((cell_w - 1) - 3) / 2;
            mvwprintw(modal->win, grid_y + 1, label_x, "%s", headers[col]);
        }
        mvwaddstr(modal->win, grid_y + 1, grid_x + grid_w - 1, "┃");
        calendar_draw_rule(modal->win, grid_y + 2, grid_x, cell_w, "┣", "╇", "┫", "━");
        wattroff(modal->win, grid_attr);

        int first = calendar_first_weekday(month, year);
        for (int row = 0; row < 6; row++) {
            int draw_y = grid_y + 3 + row * 2;
            for (int col = 0; col < 7; col++) {
                int index = row * 7 + col;
                int cell_day = index - first + 1;
                int draw_x = grid_x + col * cell_w;
                wattron(modal->win, grid_attr);
                mvwaddstr(modal->win, draw_y, draw_x, col == 0 ? "┃" : "│");
                if (col == 6) {
                    mvwaddstr(modal->win, draw_y, draw_x + cell_w, "┃");
                }
                wattroff(modal->win, grid_attr);
                if (cell_day < 1 || cell_day > max_day) {
                    continue;
                }
                int day_attr = 0;
                if (!focus_top && cell_day == selected_day) {
                    day_attr = A_REVERSE | A_BOLD;
                } else if (cell_day == day) {
                    day_attr = A_BOLD;
                }
                if (day_attr) {
                    wattron(modal->win, day_attr);
                }
                if (!focus_top && cell_day == selected_day) {
                    mvwhline(modal->win, draw_y, draw_x + 1, ' ', cell_w - 1);
                }
                mvwprintw(modal->win, draw_y, draw_x + 2, "%2d", cell_day);
                if (day_attr) {
                    wattroff(modal->win, day_attr);
                }
            }
            wattron(modal->win, grid_attr);
            if (row < 5) {
                calendar_draw_rule(modal->win, draw_y + 1, grid_x, cell_w, "┠", "┼", "┨", "─");
            }
            wattroff(modal->win, grid_attr);
        }
        wattron(modal->win, grid_attr);
        calendar_draw_rule(modal->win, grid_y + 14, grid_x, cell_w, "┗", "┷", "┛", "━");
        wattroff(modal->win, grid_attr);

        ghostcode_pm_wnoutrefresh(shadow);
        ghostcode_pm_wnoutrefresh(modal);
        ghostcode_pm_update();

        int ch = wgetch(modal->win);
        if (focus_top) {
            if (ch == 27 || ch == '\n' || ch == KEY_ENTER) {
                char *result = calendar_format_date(month, day, year);
                ghostcode_pm_remove(modal);
                ghostcode_pm_remove(shadow);
                ghostcode_pm_update();
                return result;
            }
            if (ch == '\t') {
                focus_top = 0;
            } else if (ch == KEY_LEFT) {
                numeric_part = numeric_part > 0 ? numeric_part - 1 : 2;
            } else if (ch == KEY_RIGHT) {
                numeric_part = numeric_part < 2 ? numeric_part + 1 : 0;
            } else if (ch == KEY_UP) {
                calendar_adjust_numeric(&month, &day, &year, numeric_part, 1);
            } else if (ch == KEY_DOWN) {
                calendar_adjust_numeric(&month, &day, &year, numeric_part, -1);
            }
        } else {
            if (ch == 27) {
                focus_top = 1;
            } else if (ch == '\t') {
                focus_top = 1;
            } else if (ch == KEY_LEFT) {
                calendar_move_day(&day, month, year, -1);
            } else if (ch == KEY_RIGHT) {
                calendar_move_day(&day, month, year, 1);
            } else if (ch == KEY_UP) {
                calendar_move_day(&day, month, year, -7);
            } else if (ch == KEY_DOWN) {
                calendar_move_day(&day, month, year, 7);
            } else if (ch == '\n' || ch == KEY_ENTER) {
                focus_top = 1;
            }
        }
    }
}

static int clock_wrap(int value, int minimum, int maximum) {
    int range = maximum - minimum + 1;
    while (value < minimum) value += range;
    while (value > maximum) value -= range;
    return value;
}

static int clock_parse_initial(const char *value, int *hour, int *minute, int *second) {
    int parsed_hour = 0;
    int parsed_minute = 0;
    int parsed_second = 0;
    char suffix[3] = "";
    int fields;

    if (!value) return 0;
    fields = sscanf(value, " %d:%d:%d %2s", &parsed_hour, &parsed_minute, &parsed_second, suffix);
    if (fields < 3) {
        parsed_second = 0;
        suffix[0] = '\0';
        fields = sscanf(value, " %d:%d %2s", &parsed_hour, &parsed_minute, suffix);
    }
    if (fields < 2 || parsed_minute < 0 || parsed_minute > 59 ||
        parsed_second < 0 || parsed_second > 59) {
        return 0;
    }
    if (suffix[0]) {
        if (parsed_hour < 1 || parsed_hour > 12) return 0;
        if ((suffix[0] == 'P' || suffix[0] == 'p') &&
            (suffix[1] == 'M' || suffix[1] == 'm')) {
            if (parsed_hour != 12) parsed_hour += 12;
        } else if ((suffix[0] == 'A' || suffix[0] == 'a') &&
                   (suffix[1] == 'M' || suffix[1] == 'm')) {
            if (parsed_hour == 12) parsed_hour = 0;
        } else {
            return 0;
        }
    } else if (parsed_hour < 0 || parsed_hour > 23) {
        return 0;
    }
    *hour = parsed_hour;
    *minute = parsed_minute;
    *second = parsed_second;
    return 1;
}

static void clock_local_time(int *hour, int *minute, int *second) {
    time_t now = time(NULL);
    struct tm local_value;
    if (localtime_r(&now, &local_value)) {
        *hour = local_value.tm_hour;
        *minute = local_value.tm_min;
        *second = local_value.tm_sec;
    } else {
        *hour = 0;
        *minute = 0;
        *second = 0;
    }
}

static long long clock_monotonic_seconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (long long)now.tv_sec;
}

static int clock_seconds_of_day(int hour, int minute, int second) {
    return hour * 3600 + minute * 60 + second;
}

static void clock_split_seconds(int total, int *hour, int *minute, int *second) {
    total %= 24 * 60 * 60;
    if (total < 0) total += 24 * 60 * 60;
    *hour = total / 3600;
    *minute = (total / 60) % 60;
    *second = total % 60;
}

static char *clock_format_value(int hour, int minute, int second, int format_12h, int include_seconds) {
    char *result = calloc(16, 1);
    if (!result) return NULL;
    if (format_12h) {
        const char *suffix = hour >= 12 ? "PM" : "AM";
        int display_hour = hour % 12;
        if (display_hour == 0) display_hour = 12;
        if (include_seconds) {
            snprintf(result, 16, "%02d:%02d:%02d %s", display_hour, minute, second, suffix);
        } else {
            snprintf(result, 16, "%02d:%02d %s", display_hour, minute, suffix);
        }
    } else if (include_seconds) {
        snprintf(result, 16, "%02d:%02d:%02d", hour, minute, second);
    } else {
        snprintf(result, 16, "%02d:%02d", hour, minute);
    }
    return result;
}

static void clock_draw_hand(
    BrailleCanvas *canvas,
    int center_x,
    int center_y,
    double angle,
    double length
) {
    int end_x = center_x + (int)lround(sin(angle) * length);
    int end_y = center_y - (int)lround(cos(angle) * length);
    braille_canvas_draw_line(canvas, center_x, center_y, end_x, end_y);
}

static void clock_draw_face(
    WINDOW *win,
    int origin_y,
    int origin_x,
    int char_width,
    int char_height,
    int hour,
    int minute,
    int second
) {
    BrailleCanvas canvas;
    memset(&canvas, 0, sizeof(canvas));
    if (char_width < 4 || char_height < 3 ||
        braille_canvas_init(&canvas, char_width, char_height) < 0) {
        return;
    }
    int dot_width = braille_canvas_dot_width(&canvas);
    int dot_height = braille_canvas_dot_height(&canvas);
    int center_x = dot_width / 2;
    int center_y = dot_height / 2;
    int radius = center_x < center_y ? center_x : center_y;
    radius -= 2;
    if (radius < 2) radius = 2;
    const double pi = 3.14159265358979323846;
    braille_canvas_draw_circle(&canvas, center_x, center_y, radius);
    for (int marker = 0; marker < 12; marker++) {
        double angle = (double)marker * pi / 6.0;
        int marker_x = center_x + (int)lround(sin(angle) * (radius - 1));
        int marker_y = center_y - (int)lround(cos(angle) * (radius - 1));
        braille_canvas_set_dot(&canvas, marker_x, marker_y);
    }
    clock_draw_hand(
        &canvas, center_x, center_y,
        ((double)(hour % 12) + (double)minute / 60.0) * pi / 6.0,
        radius * 0.50
    );
    clock_draw_hand(
        &canvas, center_x, center_y,
        ((double)minute + (double)second / 60.0) * pi / 30.0,
        radius * 0.72
    );
    clock_draw_hand(
        &canvas, center_x, center_y,
        (double)second * pi / 30.0,
        radius * 0.88
    );
    wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_SYNTAX_KEYWORD) | A_BOLD);
    braille_canvas_render(&canvas, win, origin_y, origin_x, char_height, char_width);
    wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_SYNTAX_KEYWORD) | A_BOLD);
    braille_canvas_free(&canvas);
}

char *ghostcode_clock_dialog(
    const char *title,
    const char *initial_time,
    int format_12h,
    int include_seconds
) {
    int initial_hour;
    int initial_minute;
    int initial_second;
    if (!clock_parse_initial(initial_time, &initial_hour, &initial_minute, &initial_second)) {
        clock_local_time(&initial_hour, &initial_minute, &initial_second);
    }
    int base_seconds = clock_seconds_of_day(initial_hour, initial_minute, initial_second);
    long long base_tick = clock_monotonic_seconds();
    int selected = 0;
    int control_count = include_seconds ? 3 : 2;
    if (format_12h) control_count++;

    int height = LINES > 20 ? 20 : LINES;
    int width = COLS > 72 ? 72 : COLS;
    if (height < 10 || width < 30) return NULL;
    int y = (LINES - height) / 2;
    int x = (COLS - width) / 2;
    GhostcodePmNode *shadow = ghostcode_pm_add(
        y + 1, x + 2, height, width,
        GHOSTCODE_PM_LAYER_MODAL_SHADOW, GHOSTCODE_PM_LAYER_MODAL_SHADOW
    );
    GhostcodePmNode *modal = ghostcode_pm_add(
        y, x, height, width, GHOSTCODE_PM_LAYER_MODAL, GHOSTCODE_PM_LAYER_MODAL
    );
    if (!modal) {
        ghostcode_pm_remove(shadow);
        return NULL;
    }
    keypad(modal->win, TRUE);
    wtimeout(modal->win, 200);

    for (;;) {
        long long elapsed = clock_monotonic_seconds() - base_tick;
        int hour;
        int minute;
        int second;
        clock_split_seconds(base_seconds + (int)elapsed, &hour, &minute, &second);

        if (shadow) {
            wbkgd(shadow->win, ghostcode_color_pair(GHOSTCODE_PAIR_SHADOW));
            werase(shadow->win);
        }
        wbkgd(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        werase(modal->win);
        wattron(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));
        box(modal->win, 0, 0);
        mvwprintw(modal->win, 0, 2, " %.*s ", width - 6, title ? title : "Clock");
        mvwprintw(modal->win, height - 2, 2, "[Left/Right] Field  [Up/Down] Change  [Enter] Select  [Esc] Cancel");
        wattroff(modal->win, ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG));

        int digital_width = width >= 58 ? 24 : width - 4;
        int digital_x = 3;
        int digital_y = 3;
        int display_hour = format_12h ? hour % 12 : hour;
        if (format_12h && display_hour == 0) display_hour = 12;
        const char *labels[4] = {"Hour", "Minute", "Second", "Period"};
        int values[4] = {display_hour, minute, second, hour >= 12};
        int logical_to_display[4] = {0, 1, 2, 3};
        if (!include_seconds && format_12h) logical_to_display[2] = 3;
        for (int control = 0; control < control_count; control++) {
            int display_index = logical_to_display[control];
            int attr = ghostcode_color_pair(GHOSTCODE_PAIR_DIALOG);
            if (control == selected) {
                attr = (int)(ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR) | A_BOLD);
            }
            wattron(modal->win, attr);
            if (display_index == 3) {
                mvwprintw(modal->win, digital_y + control * 2, digital_x,
                          "%-8s  %s", labels[display_index], values[display_index] ? "PM" : "AM");
            } else {
                mvwprintw(modal->win, digital_y + control * 2, digital_x,
                          "%-8s  %02d", labels[display_index], values[display_index]);
            }
            wattroff(modal->win, attr);
        }
        char *formatted = clock_format_value(hour, minute, second, format_12h, include_seconds);
        if (formatted) {
            wattron(modal->win, A_BOLD);
            mvwprintw(modal->win, height - 4, digital_x, "%.*s", digital_width - 1, formatted);
            wattroff(modal->win, A_BOLD);
            free(formatted);
        }

        if (width >= 58) {
            int canvas_x = digital_x + digital_width;
            int canvas_width = width - canvas_x - 2;
            int canvas_height = height - 5;
            clock_draw_face(modal->win, 2, canvas_x, canvas_width, canvas_height, hour, minute, second);
        } else {
            int canvas_height = height - (digital_y + control_count * 2) - 3;
            if (canvas_height >= 3) {
                clock_draw_face(modal->win, digital_y + control_count * 2, 3,
                                width - 6, canvas_height, hour, minute, second);
            }
        }

        ghostcode_pm_wnoutrefresh(shadow);
        ghostcode_pm_wnoutrefresh(modal);
        ghostcode_pm_update();

        int ch = wgetch(modal->win);
        if (ch == ERR) continue;
        if (ch == 27) {
            ghostcode_pm_remove(modal);
            ghostcode_pm_remove(shadow);
            ghostcode_pm_update();
            return NULL;
        }
        if (ch == '\n' || ch == KEY_ENTER) {
            char *result = clock_format_value(hour, minute, second, format_12h, include_seconds);
            ghostcode_pm_remove(modal);
            ghostcode_pm_remove(shadow);
            ghostcode_pm_update();
            return result;
        }
        if (ch == KEY_LEFT) {
            selected = clock_wrap(selected - 1, 0, control_count - 1);
            continue;
        }
        if (ch == KEY_RIGHT || ch == '\t') {
            selected = clock_wrap(selected + 1, 0, control_count - 1);
            continue;
        }
        if (ch == KEY_UP || ch == KEY_DOWN) {
            int delta = ch == KEY_UP ? 1 : -1;
            int display_index = logical_to_display[selected];
            if (display_index == 0) {
                hour = clock_wrap(hour + delta, 0, 23);
            } else if (display_index == 1) {
                minute = clock_wrap(minute + delta, 0, 59);
            } else if (display_index == 2) {
                second = clock_wrap(second + delta, 0, 59);
            } else {
                hour = (hour + 12) % 24;
            }
            base_seconds = clock_seconds_of_day(hour, minute, second);
            base_tick = clock_monotonic_seconds();
        }
    }
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
