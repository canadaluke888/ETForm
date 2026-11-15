#ifndef TFORM_DIALOGS_H
#define TFORM_DIALOGS_H

#define TFORM_FILE_PICKER_FILE 0
#define TFORM_FILE_PICKER_DIRECTORY 1

char *tform_query_dialog(const char *title, const char *initial_text);
char *tform_file_picker_dialog(const char *title, int mode, const char *start_dir);
char *tform_calendar_dialog(const char *title, const char *initial_date);
char *tform_clock_dialog(
    const char *title,
    const char *initial_time,
    int format_12h,
    int include_seconds
);
void tform_message_dialog(const char *message);
void tform_readonly_dialog(const char *title, const char *message);

#endif
