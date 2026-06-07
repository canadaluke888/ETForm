#ifndef TFORM_FORM_HELPERS_H
#define TFORM_FORM_HELPERS_H

typedef struct {
    char **items;
    int count;
    int capacity;
} TFormStringList;

int tform_insert_int_preset(int *values, int count, int capacity, int current);
int tform_next_form_index(int current, int count, int dir);

void tform_string_list_init(TFormStringList *list);
void tform_string_list_free(TFormStringList *list);
int tform_string_list_append(TFormStringList *list, const char *value);
int tform_string_list_set(TFormStringList *list, int index, const char *value);
int tform_string_list_delete(TFormStringList *list, int index);
int tform_string_list_copy(TFormStringList *dest, const TFormStringList *src);

#endif
