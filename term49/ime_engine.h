/* Term49 offline Pinyin adapter. Apache-2.0. */
#ifndef TERM49_IME_ENGINE_H
#define TERM49_IME_ENGINE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define IME_PAGE 3
#define IME_MAX_INPUT 64
#define IME_MAX_TEXT 128
enum { IME_BACKSPACE=0x10001, IME_ESCAPE, IME_ENTER, IME_LEFT, IME_RIGHT, IME_UP, IME_DOWN, IME_TAB };
typedef void (*ime_commit_fn)(const uint16_t *, size_t);
int ime_init(const char *dictionary, const char *user_dictionary, ime_commit_fn commit);
void ime_close(void);
int ime_ready(void);
int ime_enabled(void);
void ime_toggle(void);
void ime_cancel(void);
int ime_key(int key, int shortcut_modifier);
int ime_choose_visible(int index);
void ime_page(int direction);
const char *ime_spelling(void);
size_t ime_candidate_count(void);
size_t ime_page_number(void);
int ime_selected(void);
int ime_candidate(int visible_index, uint16_t *out, size_t capacity);
int ime_fixed(uint16_t *out, size_t capacity);
const char *ime_notice(void);
#ifdef __cplusplus
}
#endif
#endif
