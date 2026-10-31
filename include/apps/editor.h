#ifndef APPS_EDITOR_H
#define APPS_EDITOR_H

/*
 * editor.h:
 * Multi-Line Notepad-like Text Editor with VFS File Integration.
 *
 * Why this is needed:
 * Provides a windowed text editor with multi-line buffer, caret cursor,
 * character insertion, newline handling, backspace deletion, and status bar.
 * Integrates with VFS to open files from storage and save modified documents.
 */

/* Launches or brings to front the Text Editor application window */
void app_editor_open(void);

/* Launches the Text Editor with a specific file loaded from VFS */
void app_editor_open_file(const char* filepath);

#endif /* APPS_EDITOR_H */
