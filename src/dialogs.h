#ifndef ETFORM_DIALOGS_H
#define ETFORM_DIALOGS_H

#define ETFORM_FILE_PICKER_FILE 0
#define ETFORM_FILE_PICKER_DIRECTORY 1

char *etform_query_dialog(const char *title, const char *initial_text);
char *etform_file_picker_dialog(const char *title, int mode, const char *start_dir);
char *etform_calendar_dialog(const char *title, const char *initial_date);
char *etform_clock_dialog(
    const char *title,
    const char *initial_time,
    int format_12h,
    int include_seconds
);
void etform_message_dialog(const char *message);
void etform_readonly_dialog(const char *title, const char *message);

#endif
