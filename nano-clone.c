#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/*
 * A Simplified Nano-Like Text Editor
 *
 * Controls:
 *   - Arrow keys: Move cursor around.
 *   - Printable keys: Insert characters.
 *   - Backspace: Delete character before cursor.
 *   - Ctrl+O: Save
 *   - Ctrl+X: Exit
 *
 * If a filename is provided as an argument, it will attempt to open it,
 * allowing editing of that file. If no file is provided, it starts with
 * an empty buffer.
 *
 * This is a very simplified demonstration and not a complete clone of nano.
 */

typedef struct {
    char **lines;   // Array of lines
    int numlines;   // Number of lines in the buffer
    int row;        // Cursor position in terms of lines
    int col;        // Cursor position in terms of characters
    int topline;    // The line number currently at the top of the screen
    int leftcol;    // The column number currently at the left of the screen
    int screenrows; // Number of rows in the terminal
    int screencols; // Number of columns in the terminal
    char *filename; // Name of the file currently editing
    int modified;   // Has the file been modified?
} EditorState;

static EditorState E;

/* Forward declarations */
void editor_init(const char *filename);
void editor_free(void);
void editor_load_file(const char *filename);
int  editor_save_file(void);
void editor_refresh_screen(void);
void editor_process_key(int c);
void editor_insert_char(char ch);
void editor_delete_char(void);
void editor_insert_line(int at, const char *s);
void editor_delete_line(int at);
void editor_status_message(const char *msg);

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    if (argc > 1) filename = argv[1];

    initscr();
    raw();               // raw input (no line buffering)
    noecho();            // don't echo characters typed
    keypad(stdscr, TRUE);
    curs_set(1);         // show the cursor
    start_color();

    editor_init(filename);

    while (1) {
        editor_refresh_screen();
        int c = getch();
        editor_process_key(c);
    }

    endwin();
    editor_free();
    return 0;
}

void editor_init(const char *filename) {
    getmaxyx(stdscr, E.screenrows, E.screencols);

    // Reserve last 3 lines for status bars/help lines
    E.screenrows -= 3;

    E.numlines = 0;
    E.lines = NULL;
    E.row = 0;
    E.col = 0;
    E.topline = 0;
    E.leftcol = 0;
    E.modified = 0;
    E.filename = NULL;

    if (filename) {
        E.filename = strdup(filename);
        editor_load_file(filename);
    } else {
        editor_insert_line(0, "");
    }

    editor_status_message("HELP: Ctrl+O = Save | Ctrl+X = Exit");
}

void editor_free(void) {
    if (E.filename) free(E.filename);
    for (int i = 0; i < E.numlines; i++) {
        free(E.lines[i]);
    }
    free(E.lines);
}

void editor_status_message(const char *msg) {
    // We store a short message that can be displayed in the status bar area.
    // In this simplified version, we’ll just print immediately during refresh.
    // For a more robust solution, store and print it on refresh.
    move(E.screenrows + 1, 0);
    clrtoeol();
    attron(A_REVERSE);
    mvprintw(E.screenrows + 1, 0, "%s", msg);
    attroff(A_REVERSE);
}

void editor_load_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT) {
            // File does not exist, start empty
            editor_insert_line(0, "");
            return;
        } else {
            // Another error
            editor_insert_line(0, "");
            editor_status_message("Error opening file.");
            return;
        }
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    while ((len = getline(&line, &cap, fp)) != -1) {
        // Strip newline
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[len-1] = '\0';
        editor_insert_line(E.numlines, line);
    }
    free(line);
    fclose(fp);

    if (E.numlines == 0)
        editor_insert_line(0, "");
}

int editor_save_file(void) {
    if (E.filename == NULL) {
        // For simplicity, if no filename provided at start,
        // we just name it "untitled.txt".
        E.filename = strdup("untitled.txt");
    }

    FILE *fp = fopen(E.filename, "w");
    if (!fp) {
        editor_status_message("Error: Cannot open file for writing!");
        return -1;
    }

    for (int i = 0; i < E.numlines; i++) {
        fprintf(fp, "%s\n", E.lines[i]);
    }

    fclose(fp);
    E.modified = 0;
    editor_status_message("File saved successfully!");
    return 0;
}

void editor_insert_line(int at, const char *s) {
    if (at < 0 || at > E.numlines) return;
    E.lines = realloc(E.lines, sizeof(char*) * (E.numlines + 1));
    memmove(&E.lines[at+1], &E.lines[at], sizeof(char*) * (E.numlines - at));
    E.lines[at] = strdup(s);
    E.numlines++;
    E.modified = 1;
}

void editor_delete_line(int at) {
    if (at < 0 || at >= E.numlines) return;
    free(E.lines[at]);
    memmove(&E.lines[at], &E.lines[at+1], sizeof(char*) * (E.numlines - at - 1));
    E.numlines--;
    E.modified = 1;
    if (E.numlines == 0) {
        editor_insert_line(0, "");
    }
}

void editor_insert_char(char ch) {
    if (E.row < 0 || E.row >= E.numlines) return;

    char *line = E.lines[E.row];
    int len = (int)strlen(line);

    if (E.col < 0) E.col = 0;
    if (E.col > len) E.col = len;

    char *newline = malloc(len + 2);
    memcpy(newline, line, E.col);
    newline[E.col] = ch;
    memcpy(&newline[E.col+1], &line[E.col], len - E.col + 1);

    free(E.lines[E.row]);
    E.lines[E.row] = newline;
    E.col++;
    E.modified = 1;
}

void editor_delete_char(void) {
    if (E.row < 0 || E.row >= E.numlines) return;
    if (E.col == 0 && E.row == 0) return;

    char *line = E.lines[E.row];
    int len = (int)strlen(line);

    if (E.col > 0) {
        // Delete character before cursor in the same line
        char *newline = malloc(len);
        memcpy(newline, line, E.col - 1);
        memcpy(&newline[E.col-1], &line[E.col], len - E.col + 1);
        free(E.lines[E.row]);
        E.lines[E.row] = newline;
        E.col--;
        E.modified = 1;
    } else {
        // At the beginning of a line, we merge this line with the previous one
        int prev_len = (int)strlen(E.lines[E.row - 1]);
        char *newline = malloc(prev_len + len + 1);
        strcpy(newline, E.lines[E.row - 1]);
        strcat(newline, line);
        free(E.lines[E.row - 1]);
        free(E.lines[E.row]);
        memmove(&E.lines[E.row-1], &E.lines[E.row], sizeof(char*) * (E.numlines - E.row));
        E.numlines--;
        E.lines[E.numlines] = NULL;
        E.lines[E.row - 1] = newline;
        E.row--;
        E.col = prev_len;
        E.modified = 1;
    }
}

void editor_move_cursor(int key) {
    switch (key) {
        case KEY_UP:
            if (E.row > 0) E.row--;
            if (E.col > (int)strlen(E.lines[E.row]))
                E.col = (int)strlen(E.lines[E.row]);
            break;
        case KEY_DOWN:
            if (E.row < E.numlines - 1) E.row++;
            if (E.col > (int)strlen(E.lines[E.row]))
                E.col = (int)strlen(E.lines[E.row]);
            break;
        case KEY_LEFT:
            if (E.col > 0) {
                E.col--;
            } else if (E.row > 0) {
                E.row--;
                E.col = (int)strlen(E.lines[E.row]);
            }
            break;
        case KEY_RIGHT:
            if (E.col < (int)strlen(E.lines[E.row])) {
                E.col++;
            } else if (E.row < E.numlines - 1) {
                E.row++;
                E.col = 0;
            }
            break;
    }
}


void editor_process_key(int c) {
    if (c == 24) { // Ctrl+X
        // Exit
        if (E.modified) {
            editor_status_message("File modified. Ctrl+O to save, Ctrl+X to exit without saving.");
            int c2 = getch();
            if (c2 != 24) return;
        }
        endwin();
        editor_free();
        exit(0);
    } else if (c == 15) { // Ctrl+O to save
        editor_save_file();
        return;
    }

    switch (c) {
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            editor_move_cursor(c);
            break;
        case KEY_BACKSPACE:
        case 127:
            editor_delete_char();
            break;
        case '\r':  // Carriage Return (ASCII 13)
        case '\n':  // Line Feed (ASCII 10)
            // Move down one line, creating a new line if at the bottom
            if (E.row < E.numlines - 1) {
                E.row++;
                int line_len = (int)strlen(E.lines[E.row]);
                if (E.col > line_len) {
                    E.col = line_len;
                }
            } else {
                // If at the last line, add a new empty line
                editor_insert_line(E.numlines, "");
                E.row++;
                E.col = 0;
            }
            break;
        default:
            if (isprint(c)) {
                editor_insert_char(c);
            }
            break;
    }
}

void editor_draw_rows(void) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = E.topline + y;
        move(y, 0);
        clrtoeol();
        if (filerow < E.numlines) {
            char *line = E.lines[filerow];
            int len = (int)strlen(line);
            if (len > E.leftcol) {
                int drawlen = len - E.leftcol;
                if (drawlen > E.screencols) drawlen = E.screencols;
                for (int i = 0; i < drawlen; i++)
                    addch(line[E.leftcol + i]);
            }
        }
    }
}

void editor_draw_status_bar(void) {
    attron(A_REVERSE);
    char status[80];
    int len;
    if (E.filename)
        len = snprintf(status, sizeof(status), "File: %s %s", E.filename, E.modified ? "(modified)" : "");
    else
        len = snprintf(status, sizeof(status), "File: (No Name) %s", E.modified ? "(modified)" : "");
    int rlen = len;
    if (rlen > E.screencols) rlen = E.screencols;
    move(E.screenrows, 0);
    for (int i = 0; i < rlen; i++) addch(status[i]);
    for (int i = rlen; i < E.screencols; i++) addch(' ');
    attroff(A_REVERSE);

    // Display a help line (like nano)
    move(E.screenrows+2, 0);
    clrtoeol();
    printw("^X Exit  ^O Save");
}

void editor_scroll(void) {
    if (E.row < E.topline) {
        E.topline = E.row;
    }
    if (E.row >= E.topline + E.screenrows) {
        E.topline = E.row - E.screenrows + 1;
    }

    if (E.col < E.leftcol) {
        E.leftcol = E.col;
    }
    if (E.col >= E.leftcol + E.screencols) {
        E.leftcol = E.col - E.screencols + 1;
    }
}

void editor_refresh_screen(void) {
    editor_scroll();
    editor_draw_rows();
    editor_draw_status_bar();
    move(E.row - E.topline, E.col - E.leftcol);
    refresh();
}

/* and this is the end, my friend */

