#include "form_helpers.h"

#include <stdlib.h>
#include <string.h>

int etform_insert_int_preset(int *values, int count, int capacity, int current) {
    int insert_at = count;

    if (!values || count < 0 || capacity < count) {
        return count;
    }
    for (int i = 0; i < count; i++) {
        if (values[i] == current) {
            return count;
        }
        if (values[i] > current && insert_at == count) {
            insert_at = i;
        }
    }
    if (count >= capacity) {
        return count;
    }
    for (int i = count; i > insert_at; i--) {
        values[i] = values[i - 1];
    }
    values[insert_at] = current;
    return count + 1;
}

int etform_next_form_index(int current, int count, int dir) {
    if (count <= 0) {
        return 0;
    }
    if (current < 0 || current >= count) {
        current = 0;
    }
    if (dir < 0) {
        return current == 0 ? count - 1 : current - 1;
    }
    if (dir > 0) {
        return current == count - 1 ? 0 : current + 1;
    }
    return current;
}

void etform_string_list_init(ETFormStringList *list) {
    if (!list) {
        return;
    }
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void etform_string_list_free(ETFormStringList *list) {
    if (!list) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    etform_string_list_init(list);
}

static int ensure_capacity(ETFormStringList *list, int needed) {
    if (!list || needed < 0) {
        return -1;
    }
    if (list->capacity >= needed) {
        return 0;
    }
    int next_capacity = list->capacity ? list->capacity * 2 : 8;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }
    char **next = realloc(list->items, (size_t)next_capacity * sizeof(char *));
    if (!next) {
        return -1;
    }
    list->items = next;
    list->capacity = next_capacity;
    return 0;
}

int etform_string_list_append(ETFormStringList *list, const char *value) {
    if (!list || !value || !*value || ensure_capacity(list, list->count + 1) < 0) {
        return -1;
    }
    char *copy = strdup(value);
    if (!copy) {
        return -1;
    }
    list->items[list->count++] = copy;
    return 0;
}

int etform_string_list_set(ETFormStringList *list, int index, const char *value) {
    if (!list || index < 0 || index >= list->count || !value || !*value) {
        return -1;
    }
    char *copy = strdup(value);
    if (!copy) {
        return -1;
    }
    free(list->items[index]);
    list->items[index] = copy;
    return 0;
}

int etform_string_list_delete(ETFormStringList *list, int index) {
    if (!list || index < 0 || index >= list->count) {
        return -1;
    }
    free(list->items[index]);
    for (int i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
    return 0;
}

int etform_string_list_copy(ETFormStringList *dest, const ETFormStringList *src) {
    if (!dest || !src) {
        return -1;
    }
    etform_string_list_init(dest);
    for (int i = 0; i < src->count; i++) {
        if (etform_string_list_append(dest, src->items[i]) < 0) {
            etform_string_list_free(dest);
            return -1;
        }
    }
    return 0;
}
