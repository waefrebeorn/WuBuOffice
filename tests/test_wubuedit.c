/* test_wubunote.c -- pure EditBuf model (Notepad++-class editing core). */
#include "edit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

/* reload a fresh 3-line doc and place the cursor */
static void reset(EditBuf *b) {
    edit_load(b, "hello world\nfoo bar\nbaz");
    edit_cursor_set(b, 0, 0);
}

int main(void) {
    EditBuf *b = edit_create();
    CK(b != NULL, "create");
    CK(edit_line_count(b) == 1, "starts with 1 empty line");

    /* load multi-line */
    edit_load(b, "hello world\nfoo bar\nbaz");
    CK(edit_line_count(b) == 3, "load -> 3 lines");
    CK(strcmp(edit_line(b, 0), "hello world") == 0, "line0");
    CK(strcmp(edit_line(b, 2), "baz") == 0, "line2");
    CK(edit_dirty(b) == 0, "load clears dirty");

    /* insert char */
    reset(b);
    edit_cursor_set(b, 0, 5);
    edit_put_char(b, 'X');
    CK(strcmp(edit_line(b, 0), "helloX world") == 0, "put char mid-line");
    CK(edit_dirty(b) == 1, "put sets dirty");
    CK(edit_cursor_col(b) == 6, "cursor advanced after put");

    /* backspace */
    reset(b);
    edit_cursor_set(b, 0, 5);
    edit_put_char(b, 'X');
    edit_backspace(b);
    CK(strcmp(edit_line(b, 0), "hello world") == 0, "backspace removed X");
    CK(edit_cursor_col(b) == 5, "cursor back after backspace");

    /* new line splits (leading space on split is dropped) */
    reset(b);
    edit_cursor_set(b, 0, 5);
    edit_new_line(b);
    CK(edit_line_count(b) == 4, "newline -> 4 lines");
    CK(strcmp(edit_line(b, 0), "hello") == 0, "split left");
    CK(strcmp(edit_line(b, 1), "world") == 0, "split right (no leading space)");
    CK(edit_cursor_row(b) == 1 && edit_cursor_col(b) == 0, "cursor on new line");

    /* delete at EOL joins with the next line */
    reset(b);
    edit_cursor_set(b, 0, 5);
    edit_new_line(b);              /* -> hello / world / foo bar / baz */
    edit_cursor_set(b, 1, 5);    /* end of 'world' */
    edit_delete(b);
    CK(edit_line_count(b) == 3, "delete at EOL joins -> 3 lines");
    CK(strcmp(edit_line(b, 1), "worldfoo bar") == 0, "joined with next line");

    /* backspace at col0 joins upward */
    reset(b);
    edit_cursor_set(b, 0, 5);
    edit_new_line(b);              /* -> hello / world / foo bar / baz */
    edit_cursor_set(b, 1, 0);
    edit_backspace(b);
    CK(edit_line_count(b) == 3, "backspace@col0 joins up -> 3 lines");
    CK(strcmp(edit_line(b, 0), "helloworld") == 0, "joined up");

    /* arrows */
    reset(b);
    edit_cursor_set(b, 0, 0);
    edit_arrow_right(b); edit_arrow_right(b);
    CK(edit_cursor_col(b) == 2, "arrow right x2");
    edit_arrow_left(b);
    CK(edit_cursor_col(b) == 1, "arrow left");
    edit_cursor_end(b);
    CK(edit_cursor_col(b) == strlen(edit_line(b,0)), "arrow end");
    edit_cursor_home(b);
    CK(edit_cursor_col(b) == 0, "arrow home");
    edit_arrow_down(b);
    CK(edit_cursor_row(b) == 1, "arrow down");
    edit_arrow_up(b);
    CK(edit_cursor_row(b) == 0, "arrow up");

    /* goto line (1-based, clamped) */
    reset(b);
    edit_goto_line(b, 2);
    CK(edit_cursor_row(b) == 1, "goto line 2");
    edit_goto_line(b, 999);
    CK(edit_cursor_row(b) == edit_line_count(b) - 1, "goto clamps to last");
    edit_goto_line(b, 0);
    CK(edit_cursor_row(b) == 0, "goto clamps to first");

    /* find-next: forward progression across lines (cursor lands at
     * the match start; the next call resumes just past it) */
    edit_load(b, "alpha\nalpha\nalpha");
    edit_cursor_set(b, 0, 0);
    CK(edit_find_next(b, "alpha") == 1, "find first");
    CK(edit_cursor_row(b) == 0, "found on line0");
    CK(edit_find_next(b, "alpha") == 1, "find second");
    CK(edit_cursor_row(b) == 1, "found on line1");
    CK(edit_find_next(b, "alpha") == 1, "find third");
    CK(edit_cursor_row(b) == 2, "found on line2");

    /* find-next: wrap to the top when past the last match */
    edit_load(b, "beta\nalpha");
    edit_cursor_set(b, 0, 0);
    CK(edit_find_next(b, "alpha") == 1 && edit_cursor_row(b) == 1, "finds on line1");
    CK(edit_find_next(b, "alpha") == 1 && edit_cursor_row(b) == 1, "wraps back to line1 (only match)");

    /* find-next: no match leaves the cursor unchanged */
    edit_load(b, "aaa\nbbb");
    edit_cursor_set(b, 0, 3);
    CK(edit_find_next(b, "zzz") == 0, "no match -> 0");
    CK(edit_cursor_row(b) == 0 && edit_cursor_col(b) == 3, "cursor unchanged on no match");
    CK(edit_find_next(b, NULL) == 0, "NULL needle -> 0");

    /* serialize round-trip (no trailing newline in source) */
    edit_load(b, "line one\nline two");
    char *s = edit_serialize(b);
    CK(s && strcmp(s, "line one\nline two") == 0, "serialize (no trailing nl)");
    free(s);

    /* trailing newline in source -> kept as a final empty line on load */
    edit_load(b, "a\nb\n");
    s = edit_serialize(b);
    CK(s && strcmp(s, "a\nb\n") == 0, "trailing nl preserved");
    free(s);

    edit_free(b);
    CK(1, "free ok");

    if (fails) { printf("WUBUEDIT TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUEDIT TESTS PASSED\n");
    return 0;
}
