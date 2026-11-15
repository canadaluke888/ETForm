#ifndef ETFORM_FORM_HELPERS_H
#define ETFORM_FORM_HELPERS_H

typedef struct {
    char **items;
    int count;
    int capacity;
} ETFormStringList;

int etform_insert_int_preset(int *values, int count, int capacity, int current);
int etform_next_form_index(int current, int count, int dir);

void etform_string_list_init(ETFormStringList *list);
void etform_string_list_free(ETFormStringList *list);
int etform_string_list_append(ETFormStringList *list, const char *value);
int etform_string_list_set(ETFormStringList *list, int index, const char *value);
int etform_string_list_delete(ETFormStringList *list, int index);
int etform_string_list_copy(ETFormStringList *dest, const ETFormStringList *src);

#endif
