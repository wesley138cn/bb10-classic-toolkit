/* Modified for BB10 Classic Toolkit, 2026-10-21; see ../MODIFICATIONS.md. */
/* Bounded terminal scrollback. No SDL surfaces are retained in stored lines. */
#ifndef SCROLLBACK_H
#define SCROLLBACK_H
#define HISTORY_LIMIT 4096
struct history;
void history_append(struct screenchar *line, int width);
void history_clear(buf_t *owner);
void history_resize_top(int delta);
void history_destroy(buf_t *owner);
void history_move(int older);
void history_live(void);
int history_active(void);
int history_count(void);
int history_position(void);
int history_total(void);
struct screenchar *history_cell(int row,int col);
void history_invalidate(void);
#endif
