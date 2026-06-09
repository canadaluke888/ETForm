#ifndef GHOSTCODE_DIALOGS_H
#define GHOSTCODE_DIALOGS_H

#define TFORM_FILE_PICKER_FILE 0
#define TFORM_FILE_PICKER_DIRECTORY 1

char *ghostcode_query_dialog(const char *title, const char *initial_text);
char *ghostcode_file_picker_dialog(const char *title, int mode, const char *start_dir);
char *ghostcode_calendar_dialog(const char *title, const char *initial_date);
void ghostcode_message_dialog(const char *message);
void ghostcode_readonly_dialog(const char *title, const char *message);

#endif
