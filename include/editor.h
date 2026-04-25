#ifndef EDITOR_H
#define EDITOR_H

#define EDITOR_MAX_LINES 200
#define EDITOR_LINE_LENGTH 160 // Doubled for horizontal scrolling

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_LINE_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_y;
    int view_offset_y; 
    int view_offset_x; // Horizontal scrolling
    int modified;
} editor_t;

void editor();
void init_editor(editor_t* ed);

#endif
