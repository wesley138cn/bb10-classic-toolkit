/* Modified for BB10 Classic Toolkit, 2026-10-21; see ../MODIFICATIONS.md. */
/*
 * Copyright (c) 2013 Todd Mortimer
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ioctl.h>
#include <unix.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>

#include <bps/screen.h>
#include <bps/virtualkeyboard.h>
#include <bps/deviceinfo.h>
#include <unicode/utf.h>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_syswm.h"
#include "SDL_thread.h"

#include "types.h"
#include "terminal.h"
#include "ecma48.h"
#include "preferences.h"
#include "buffer.h"
#include "io.h"
#include "colors.h"
#include "ime_engine.h"
#include "scrollback.h"
#include <assert.h>

static int exit_application = 0;
static int cjk_render_test = 0;

static char slave_ptyname[L_ctermid];

static int cursor_x = 0;
static int cursor_y = 0;
char draw_cursor = 1;

char flash = 0;

static pref_t *prefs = NULL;
static symmenu_t *current_symmenu = NULL;

static char symmenu_lock = 0;
static char altsym_lock = 0;

static char metamode = 0;
static int metamode_doubletap_key = 0;
static struct timespec metamode_last;
static SDL_Color metamode_cursor_fg = SDL_BLACK;
static SDL_Color metamode_cursor_bg = SDL_GREEN;
static SDL_Surface* metamode_cursor;
static int vmodifiers = 0;

static TTF_Font* font;
static TTF_Font* cjk_font;
static int font_baseline;
#define LATIN_FONT_PATH "/usr/fonts/font_repository/monotype/andalemo.ttf"
static int text_width;
static int text_height;
static int text_height_padding;
static int advance;
static int default_text_color_arr[PREFS_COLOR_NUM_ELEMENTS];
static int default_bg_color_arr[PREFS_COLOR_NUM_ELEMENTS];
static SDL_Color default_text_color = SDL_WHITE;
static SDL_Color default_bg_color = SDL_BLACK;
struct font_style default_text_style;

struct screenchar blank_sc;
static SDL_Surface* flash_surface;
static SDL_Surface* cursor;
static SDL_Surface* inv_cursor;
static SDL_Surface* screen;
static SDL_Surface* ctrl_key_indicator;
static SDL_Surface* alt_key_indicator;
static SDL_Surface* shift_key_indicator;
static SDL_Surface* altsym_indicator;
SDL_Surface* blank_surface;

static pid_t child_pid = -1;

static char virtualkeyboard_visible = 0;
static void scroll_render_controls(void);
static char key_repeat_done = 0;

static SDL_mutex *input_mutex = NULL;

static int event_pipe[2];

/* from buffer.c */
extern int rows;
extern int cols;
extern buf_t* buf;
extern int MAX_COLS;
extern int MAX_ROWS;
extern int TEXT_BUFFER_SIZE;
extern struct scroll_region sr;

#define PB_D_PIXELS 32

int is_terminfo_keystrokes(const char* keystrokes){
	if(keystrokes[0] == 'k'){
		if(0 == strncmp(keystrokes, "kcub1", 5)){ return KEYCODE_LEFT; }
		if(0 == strncmp(keystrokes, "kcud1", 5)){ return KEYCODE_DOWN; }
		if(0 == strncmp(keystrokes, "kcuf1", 5)){ return KEYCODE_RIGHT; }
		if(0 == strncmp(keystrokes, "kcuu1", 5)){ return KEYCODE_UP; }
		if(0 == strncmp(keystrokes, "khome", 5)){ return KEYCODE_HOME; }
		if(0 == strncmp(keystrokes, "kend", 4)){ return KEYCODE_END; }
		if(0 == strncmp(keystrokes, "kf1", 3)){ return KEYCODE_F1; }
		if(0 == strncmp(keystrokes, "kf2", 3)){ return KEYCODE_F2; }
		if(0 == strncmp(keystrokes, "kf3", 3)){ return KEYCODE_F3; }
		if(0 == strncmp(keystrokes, "kf4", 3)){ return KEYCODE_F4; }
		if(0 == strncmp(keystrokes, "kf5", 3)){ return KEYCODE_F5; }
		if(0 == strncmp(keystrokes, "kf6", 3)){ return KEYCODE_F6; }
		if(0 == strncmp(keystrokes, "kf7", 3)){ return KEYCODE_F7; }
		if(0 == strncmp(keystrokes, "kf8", 3)){ return KEYCODE_F8; }
		if(0 == strncmp(keystrokes, "kf9", 3)){ return KEYCODE_F9; }
		if(0 == strncmp(keystrokes, "kf10", 4)){ return KEYCODE_F10; }
		if(0 == strncmp(keystrokes, "kf11", 4)){ return KEYCODE_F11; }
		if(0 == strncmp(keystrokes, "kf12", 4)){ return KEYCODE_F12; }
	}
	return 0;
}

static int ime_translate_key(int key);
int send_metamode_keystrokes(const char* keystrokes){

	UChar* ukeystrokes;
	size_t ukeystrokes_len;
	size_t keystrokes_len;
	int terminfo_key = 0;
	UChar terminfo_keystrokes[CHARACTER_BUFFER];

	if(keystrokes){
		terminfo_key = is_terminfo_keystrokes(keystrokes);
		/* if the keystrokes for this key match a terminfo pattern,
		 * send the appropriate sequence instead of the literal string */
		if(terminfo_key){
            if(ime_key(ime_translate_key(terminfo_key),0))return 1;
			ukeystrokes_len = ecma48_parse_control_codes(terminfo_key, 0, terminfo_keystrokes);
			/* and write out to the tty whatever the keys were */
			io_write_master(terminfo_keystrokes, ukeystrokes_len);
			return 1;
		}
		// else
		keystrokes_len = strlen(keystrokes);
        if(ime_enabled() && keystrokes_len==1){
            int key=(unsigned char)keystrokes[0];
            if(key==27)key=IME_ESCAPE;
            if(ime_key(key,0))return 1;
        }
        if(*ime_spelling())ime_key(IME_ENTER,0);
		/* libconfig will return ascii strings, but we can put utf8 in there too */
		ukeystrokes = (UChar*)calloc(keystrokes_len, sizeof(UChar));
		ukeystrokes_len = io_read_utf8_string(keystrokes, keystrokes_len, ukeystrokes);
		/* and write out to the tty whatever the keys were */
		io_write_master(ukeystrokes, ukeystrokes_len);
		free(ukeystrokes);
		return 1;
	}
	/* no keystrokes saved for this key */
	return 0;
}

int get_virtualkeyboard_height(){
	int rc, vkb_h;
	rc = virtualkeyboard_get_height(&vkb_h);
	if(rc != BPS_SUCCESS){
		fprintf(stderr, "Could not get virtual keyboard height\n");
		vkb_h = 0; // assume zero?
	}
	return vkb_h;
}

int is_passport() {
	deviceinfo_details_t *di_t = NULL;
	int rc = deviceinfo_get_details(&di_t);
	if(rc != BPS_SUCCESS){
		fprintf(stderr, "Could not get device info");
		return 0;
	}
	
	int passport = 0;
	if(strncmp("Passport", deviceinfo_details_get_model_name(di_t), 8) == 0){
		passport = 1;
	}
	deviceinfo_free_details(&di_t);

	return passport;
}

int get_wm_info(SDL_SysWMinfo* info){
	SDL_version version;
	SDL_VERSION(&version);
	info->version = version;
	return SDL_GetWMInfo(info);
}

void metamode_toggle(){
	metamode = metamode ? 0 : 1;
}

void altsym_toggle() {
	altsym_lock = altsym_lock ? 0 : 1;
}

void symmenu_stick(){
	PRINT(stderr, "Sticking Sym key\n");
	symmenu_lock = 1;
}

void symmenu_toggle(symmenu_t *target){
	if (current_symmenu == NULL){
		current_symmenu = target;
		// resize to show menu
		if (prefs->rescreen_for_symmenu) {
			setup_screen_size(screen->w, screen->h - current_symmenu->surface->h);
		}
		if (prefs->sticky_sym_key) {
			symmenu_stick();
		}
	} else {
		current_symmenu = NULL;
		if (prefs->rescreen_for_symmenu) {
			// resize to take full screen
			setup_screen_size(screen->w, screen->h);
		}
		symmenu_lock = 0;
	}
}

static const char* symkey_for_mousedown(symmenu_t *menu, Uint16 x, Uint16 y) {
	for (int row = 0; menu->keys[row] != NULL; ++row) {
		for (int col = 0; menu->keys[row][col].map != NULL; ++col) {
			symkey_t *key = &menu->keys[row][col];

			if((x >= key->hitbox.x) &&
			   (x <= key->hitbox.x + key->hitbox.w) &&
			   (y >= key->hitbox.y) &&
			   (y <= key->hitbox.y + key->hitbox.h)) {
				if (!symmenu_lock) {
					symmenu_toggle(NULL);
				} else {
					key->flash = 1;
				}
				return key->map->to;
			}
		}
	}
	
	return NULL;
}

/* Resolve face independently from the terminal's column width. */
static SDL_Surface *render_font_cell(const UChar *str, SDL_Color fg, SDL_Color bg) {
    int width = buf_char_width(str[0]) * advance;
    if (width < advance) width = advance;
    TTF_Font *face = (buf_char_width(str[0]) == 2 || !TTF_GlyphIsProvided(font, str[0]))
                    ? cjk_font : font;
    TTF_SetFontStyle(face, TTF_GetFontStyle(font));
    SDL_Surface *glyph = TTF_RenderUNICODE_Shaded(face, str, fg, bg);
    if (!glyph) return NULL;
    SDL_Surface *cell = SDL_CreateRGBSurface(SDL_SWSURFACE, width, text_height, 32,
                                            0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    if (!cell) { SDL_FreeSurface(glyph); return NULL; }
    SDL_FillRect(cell, NULL, SDL_MapRGB(cell->format, bg.r, bg.g, bg.b));
    SDL_Rect dest;
    dest.x = (width - glyph->w) / 2;
    dest.y = font_baseline - TTF_FontAscent(face);
    dest.w = glyph->w; dest.h = glyph->h;
    SDL_BlitSurface(glyph, NULL, cell, &dest);
    SDL_FreeSurface(glyph);
    return cell;
}

int font_init(int font_size){
	if(font_size < MIN_FONT_SIZE){
		fprintf(stderr, "Refusing to set font size to %d - too small\n",font_size);
		int default_font_columns = (atoi(getenv("WIDTH")) <= 720) ? 45 : 60;
		font_size = preferences_guess_best_font_size(prefs, default_font_columns);
	}


    /* Latin retains the signed terminal's original face and advance. */
    font = TTF_OpenFont(LATIN_FONT_PATH, font_size);
    if (!font) {
        fprintf(stderr, "Cannot open original Latin font: %s\n", TTF_GetError());
        return TERM_FAILURE;
    }
    int minx, maxx, miny, maxy;
    if (TTF_GlyphMetrics(font, 'X', &minx, &maxx, &miny, &maxy, &advance)) {
        TTF_CloseFont(font); font = NULL;
        return TERM_FAILURE;
    }
    /* A fullwidth em is exactly two Latin columns, without stretching. */
    cjk_font = TTF_OpenFont(prefs->font_path, 2 * advance);
    if (!cjk_font) {
        fprintf(stderr, "Cannot open CJK font: %s\n", TTF_GetError());
        TTF_CloseFont(font); font = NULL;
        return TERM_FAILURE;
    }
    TTF_Font *faces[2] = {font, cjk_font};
    for (int i=0; i<2; ++i) {
        TTF_SetFontStyle(faces[i], TTF_STYLE_NORMAL);
        TTF_SetFontOutline(faces[i], 0);
        TTF_SetFontKerning(faces[i], 0);
        TTF_SetFontHinting(faces[i], TTF_HINTING_NORMAL);
    }
    text_width = advance;
    font_baseline = TTF_FontAscent(font);
    if (TTF_FontAscent(cjk_font) > font_baseline) font_baseline = TTF_FontAscent(cjk_font);
    int descent = TTF_FontDescent(font);
    if (TTF_FontDescent(cjk_font) < descent) descent = TTF_FontDescent(cjk_font);
    text_height = font_baseline - descent;
    text_height_padding = 0;
    fprintf(stderr, "DUAL_FONT latin=%s cjk=%s cell=%d wide=%d line=%d\n",
            TTF_FontFaceFamilyName(font), TTF_FontFaceFamilyName(cjk_font),
            advance, 2*advance, text_height);

	/* get default colour settings from prefs struct*/
	default_text_color.r = (Uint8)prefs->text_color[0];
	default_text_color.g = (Uint8)prefs->text_color[1];
	default_text_color.b = (Uint8)prefs->text_color[2];
	default_text_color.unused = 0;
	
	default_bg_color.r = (Uint8)prefs->background_color[0];
	default_bg_color.g = (Uint8)prefs->background_color[1];
	default_bg_color.b = (Uint8)prefs->background_color[2];
	default_bg_color.unused = 0;

	default_text_style.fg_color = default_text_color;
	default_text_style.bg_color = default_bg_color;
	default_text_style.style = TTF_STYLE_NORMAL;
	default_text_style.reverse = 0;

	/* initialize special characters */
	UChar str[2] = {' ', NULL};
	blank_surface = render_font_cell(str, default_text_color, default_bg_color);
	if (blank_surface == NULL){
		PRINT(stderr, "Couldn't render blank surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	blank_sc.c = ' ';
	blank_sc.style = default_text_style;
	blank_sc.surface = blank_surface;
	flash_surface = render_font_cell(str, default_bg_color, default_text_color);
	if (flash_surface == NULL){
		PRINT(stderr, "Couldn't render flash surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	str[0] = 'A';
	alt_key_indicator = render_font_cell(str, metamode_cursor_fg, metamode_cursor_bg);
	if (alt_key_indicator == NULL){
		PRINT(stderr, "Couldn't render alt_key_indicator surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	str[0] = 'C';
	ctrl_key_indicator = render_font_cell(str, metamode_cursor_fg, metamode_cursor_bg);
	if (ctrl_key_indicator == NULL){
		PRINT(stderr, "Couldn't render ctrl_key_indicator surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	str[0] = 0x2191;
	shift_key_indicator = render_font_cell(str, metamode_cursor_fg, metamode_cursor_bg);
	if (shift_key_indicator == NULL){
		PRINT(stderr, "Couldn't render shift_key_indicator surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	str[0] = 'a';
	altsym_indicator = render_font_cell(str, metamode_cursor_fg, metamode_cursor_bg);
	if (shift_key_indicator == NULL){
		PRINT(stderr, "Couldn't render altsym_indicator surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	str[0] = 'M';
	metamode_cursor = render_font_cell(str, metamode_cursor_fg, metamode_cursor_bg);
	if (metamode_cursor == NULL){
		PRINT(stderr, "Couldn't render metamode_cursor surface: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}

	/* Initialize the cursor */
	UChar cursorstr[2] = {' ', NULL};
	cursor = render_font_cell(cursorstr, default_bg_color, default_text_color);
	if (cursor == NULL){
		PRINT(stderr, "Couldn't render cursor char: %s\n", TTF_GetError());
		return TERM_FAILURE;
	}


	return TERM_SUCCESS;
}

void font_uninit(){

	SDL_FreeSurface(blank_surface);
	SDL_FreeSurface(flash_surface);
	SDL_FreeSurface(metamode_cursor);
	SDL_FreeSurface(ctrl_key_indicator);
	SDL_FreeSurface(alt_key_indicator);
	SDL_FreeSurface(shift_key_indicator);
	SDL_FreeSurface(altsym_indicator);
	SDL_FreeSurface(cursor);
	if(font != NULL){
		TTF_CloseFont(font);
		font = NULL;
	}
	if (cjk_font) { TTF_CloseFont(cjk_font); cjk_font = NULL; }
}

/* Integrated input panel; composed syllables never enter the child PTY. */
static int ime_area_height;
static void ime_send_text(const uint16_t *s, size_t n) {
    /* Existing terminal converter has a 40-byte buffer. Preserve long commits. */
    while(n){size_t part=n>8?8:n;
        if(part<n && s[part-1]>=0xd800 && s[part-1]<=0xdbff)--part;
        io_write_master((const UChar *)s,part);s+=part;n-=part;
    }
}
static void ime_resize(void) {
    if(!screen)return;
    setup_screen_size(screen->w,screen->h-(virtualkeyboard_visible?get_virtualkeyboard_height():0));
}
static void ime_switch_mode(void) {
    ime_toggle();vmodifiers=0;altsym_lock=0;key_repeat_done=0;
    ime_resize();
}
static int ime_translate_key(int key) {
    switch(key){
    case KEYCODE_BACKSPACE:return IME_BACKSPACE;
    case KEYCODE_ESCAPE:return IME_ESCAPE;
    case KEYCODE_RETURN:return IME_ENTER;
    case KEYCODE_LEFT:return IME_LEFT;
    case KEYCODE_RIGHT:return IME_RIGHT;
    case KEYCODE_UP:return IME_UP;
    case KEYCODE_DOWN:return IME_DOWN;
    case KEYCODE_TAB:return IME_TAB;
    default:return key;
    }
}
static void ime_draw_line(const uint16_t *s,int x,int y,int width,SDL_Color fg,SDL_Color bg) {
    SDL_Rect r={x,y,width,text_height};
    SDL_FillRect(screen,&r,SDL_MapRGB(screen->format,bg.r,bg.g,bg.b));
    TTF_SetFontStyle(font,TTF_STYLE_NORMAL);
    int end=x+width;
    for(size_t i=0;s[i];i++) {
        int w=buf_char_width(s[i])*advance;if(w<advance)w=advance;
        if(x+w>end)break;
        if(s[i+1] && x+w+advance>end){UChar ell[2]={0x2026,0};SDL_Surface *g=render_font_cell(ell,fg,bg);if(g){SDL_Rect dst={x,y,g->w,g->h};SDL_BlitSurface(g,NULL,screen,&dst);SDL_FreeSurface(g);}break;}
        UChar ch[2]={s[i],0};SDL_Surface *g=render_font_cell(ch,fg,bg);
        if(g){SDL_Rect dst={x,y,g->w,g->h};SDL_BlitSurface(g,NULL,screen,&dst);SDL_FreeSurface(g);}
        x+=w;
    }
}
static void ime_draw_utf8(const char *s,int x,int y,int width,SDL_Color fg,SDL_Color bg) {
    UChar str[512]={0};size_t len=strlen(s);if(len>500)len=500;
    int n=io_read_utf8_string(s,len,str);str[n]=0;ime_draw_line(str,x,y,width,fg,bg);
}
static void ime_render_panel(void) {
    if(history_active()){
        SDL_Color fg={104,222,177,0},bg={27,35,42,0};
        int y=rows*text_height;char label[160];
        SDL_Rect r={0,y,screen->w,screen->h-y};SDL_FillRect(screen,&r,SDL_MapRGB(screen->format,bg.r,bg.g,bg.b));
        snprintf(label,sizeof(label),"回看 %d/%d  点下方按钮翻页/返回",history_position()+1,history_total());
        ime_draw_utf8(label,0,y,screen->w,fg,bg);return;
    }
    if(!ime_ready() || current_symmenu || !ime_area_height)return;
    SDL_Color fg={235,239,240,0},bg={27,35,42,0},accent={104,222,177,0},sel={48,77,86,0};
    int y=rows*text_height;
    SDL_Rect fill={0,y,screen->w,ime_area_height};SDL_FillRect(screen,&fill,SDL_MapRGB(screen->format,bg.r,bg.g,bg.b));
    if(!ime_enabled()){
        ime_draw_utf8("[EN/中] Shift+Space  对话回看 v2",0,y,screen->w,accent,bg);return;
    }
    const char *py=ime_spelling();int visible=cols-22;if(visible<8)visible=8;
    if(strlen(py)>(size_t)visible)py+=strlen(py)-visible;
    char header[256];snprintf(header,sizeof(header),"[中/EN] %s  %lu/%lu",py,
        (unsigned long)ime_page_number()+1,(unsigned long)(ime_candidate_count()?(ime_candidate_count()+IME_PAGE-1)/IME_PAGE:1));
    ime_draw_utf8(header,0,y,screen->w,accent,bg);
    if(!*ime_spelling()){
        ime_draw_utf8("输入拼音，空格选字；触摸候选也可选字",0,y+text_height,screen->w,fg,bg);
        ime_draw_utf8("左右翻页，上下选择，退格修改",0,y+2*text_height,screen->w,fg,bg);
        ime_draw_utf8("回车保留拼音；再按回车才发送",0,y+3*text_height,screen->w,fg,bg);
    }else{
        for(int i=0;i<IME_PAGE;i++) {
            uint16_t line[IME_MAX_TEXT+4]={0},candidate[IME_MAX_TEXT]={0};line[0]='1'+i;line[1]=' ';line[2]=' ';
            if(ime_candidate(i,candidate,IME_MAX_TEXT))memcpy(line+3,candidate,sizeof(candidate));
            ime_draw_line(line,0,y+(i+1)*text_height,screen->w,fg,i==ime_selected()?sel:bg);
        }
    }
    if(*ime_notice())ime_draw_utf8(ime_notice(),0,y+4*text_height,screen->w,fg,bg);
    else ime_draw_utf8("取消        空格选字        < 上页  下页 >",0,y+4*text_height,screen->w,accent,bg);
}
static int ime_touch(int x,int y) {
    if(!ime_ready()||current_symmenu||y<rows*text_height)return 0;
    int line=(y-rows*text_height)/text_height;
    if(line==0 && x<10*advance){ime_switch_mode();return 1;}
    if(!ime_enabled())return 1;
    if(line>=1&&line<=IME_PAGE){ime_choose_visible(line-1);return 1;}
    if(line==4){
        if(x<10*advance)ime_cancel();
        else if(x<27*advance)ime_key(' ',0);
        else ime_page(x<36*advance?-1:1);
        return 1;
    }
    return 1;
}

void handle_activeevent(int gain, int state){
	if (gain && prefs->auto_show_vkb){
		PRINT(stderr, "Got ActiveEvent - initializing keyboard\n");
		virtualkeyboard_show();
	}
}

void handle_mousedown(Uint16 x, Uint16 y){
    if(ime_touch(x,y))return;
	/* check for hits in the metamode_hitbox */
	if((x >= prefs->metamode_hitbox->x) &&
	   (x <= prefs->metamode_hitbox->x + prefs->metamode_hitbox->w) &&
	   (y >= prefs->metamode_hitbox->y) &&
	   (y <= prefs->metamode_hitbox->y + prefs->metamode_hitbox->h)) {
		/* hit in the box */
		metamode_toggle();
	}
	/* touching the screen will reveal the keyboard on a Passport,
	 * since the system wide gesture doesn't work to reveal. */
	if (prefs->auto_show_vkb){
		virtualkeyboard_show();
	}

	/* check for symmenu touches */
	if(current_symmenu != NULL){
		send_metamode_keystrokes(symkey_for_mousedown(current_symmenu, x, y));
	}
}

/* Called before IME/PTY handling, after Meta double-tap recognition. */
static int scroll_key(int key,int repeat) {
    static int swallowed;
    if(repeat && key==swallowed)return 1;
    if(!repeat)swallowed=0;
    int k=key;if(k>='A' && k<='Z')k+='a'-'A';
    if(metamode && (k=='u'||k=='d'||k=='b')){
        swallowed=key;
        if(!repeat){
            if(k=='b')history_live();else history_move(k=='u'?rows-1:1-rows);
            metamode=0;vmodifiers=0;key_repeat_done=0;
        }
        return 1;
    }
    if(history_active()){
        swallowed=key;
        if(key==KEYCODE_ESCAPE || k=='b'){history_live();metamode=0;vmodifiers=0;}
        else if(key==KEYCODE_UP)history_move(1);
        else if(key==KEYCODE_DOWN)history_move(-1);
        else if(k=='u')history_move(rows-1);
        else if(k=='d')history_move(1-rows);
        /* Ordinary typing is deliberately blocked while reading history. */
        return 1;
    }
    return 0;
}
/* Always-visible controls. Clicks use the already working mouse-down path. */
static int scroll_bar_y(void) { return (rows+((ime_ready()&&ime_enabled())?5:1))*text_height; }
static int scroll_control(int x,int y) {
    int which;
    if(current_symmenu || y<scroll_bar_y())return 0;
    which=x/(screen->w/3);if(which>2)which=2;
    if(which==0)history_move(rows-1);
    else if(which==1)history_move(1-rows);
    else {history_live();metamode=0;vmodifiers=0;}
    fprintf(stderr,"SCROLL_CONTROL=%d active=%d saved=%d\n",which,history_active(),history_count());
    return 1;
}
static void scroll_render_controls(void) {
    int y=scroll_bar_y(),w=screen->w/3;char older[64];
    SDL_Color fg={240,245,247,0},bg={43,64,76,0},dim={144,157,163,0};
    if(current_symmenu)return;
    snprintf(older,sizeof(older),history_count()||history_active()?"[ 上翻 ]":"[ 暂无历史 ]");
    ime_draw_utf8(older,0,y,w-2,history_count()||history_active()?fg:dim,bg);
    ime_draw_utf8("[ 下翻 ]",w,y,w-2,history_active()?fg:dim,bg);
    ime_draw_utf8("[ 返回输入 ]",2*w,y,screen->w-2*w,fg,bg);
}
static int touch_down,touch_x,touch_y,touch_last,touch_drag,touch_panel,touch_control;
static int motion_only,motion_x,motion_y;
static Uint32 motion_time;
static void scroll_touch_down(int x,int y) {
    motion_only=0;
    touch_down=1;touch_x=x;touch_y=touch_last=y;touch_drag=0;
    touch_control=scroll_control(x,y);
    touch_panel=(y>=rows*text_height || current_symmenu!=NULL);
}
static void scroll_touch_move(int x,int y) {
    int amount;
    if(!touch_down || touch_panel)return;
    if(!touch_drag){
        if(abs(y-touch_y)<20 || abs(y-touch_y)<abs(x-touch_x))return;
        touch_drag=1;
    }
    amount=(y-touch_last)/text_height;
    if(amount){history_move(amount);touch_last+=amount*text_height;}
}
/* TCO sends motion-only streams, without a preceding SDL button-down.
 * A gap starts a new gesture; its first point is only an anchor, never a tap. */
static void scroll_motion_event(int x,int y,Uint32 now) {
    if(touch_down){scroll_touch_move(x,y);return;}
    if(current_symmenu || y>=rows*text_height){motion_only=0;return;}
    if(!motion_only || (Uint32)(now-motion_time)>250){
        motion_only=1;motion_x=x;motion_y=y;touch_last=y;touch_drag=0;
    }else{
        if(!touch_drag && abs(y-motion_y)>=20 && abs(y-motion_y)>=abs(x-motion_x))touch_drag=1;
        if(touch_drag){int amount=(y-touch_last)/text_height;if(amount){history_move(amount);touch_last+=amount*text_height;}}
    }
    motion_time=now;
}
static void scroll_touch_up(int x,int y) {
    if(!touch_down)return;
    scroll_touch_move(x,y);
    if(!touch_control && !touch_drag && abs(y-touch_y)<20 && abs(x-touch_x)<20){
        if(history_active()){
            if(touch_panel){history_live();metamode=0;vmodifiers=0;}
        }else handle_mousedown(x,y);
    }
    touch_down=0;motion_only=0;
}
/* Shared by the real SDL event loop and the queue-level integration tests. */
static int scroll_mouse_event(SDL_Event *e) {
    switch(e->type){
    case SDL_MOUSEBUTTONDOWN:
        if(e->button.button==SDL_BUTTON_WHEELUP){history_move(3);return 1;}
        if(e->button.button==SDL_BUTTON_WHEELDOWN){history_move(-3);return 1;}
        scroll_touch_down(e->button.x,e->button.y);return 1;
    case SDL_MOUSEBUTTONUP:
        if(e->button.button==SDL_BUTTON_WHEELUP||e->button.button==SDL_BUTTON_WHEELDOWN)return 1;
        scroll_touch_up(e->button.x,e->button.y);return 1;
    case SDL_MOUSEMOTION:scroll_motion_event(e->motion.x,e->motion.y,SDL_GetTicks());return 1;
    default:return 0;
    }
}

void handle_virtualkeyboard_event(bps_event_t *event){
	PRINT(stderr, "Virtual Keyboard event\n");
	int event_code = bps_event_get_code(event);
	int vkb_h;
	int resolution[2] = {screen->w, screen->h};

	vkb_h = get_virtualkeyboard_height();

	switch (event_code){
	case VIRTUALKEYBOARD_EVENT_VISIBLE:
		setup_screen_size(resolution[0], resolution[1] - vkb_h);
		virtualkeyboard_visible = 1;
		break;
	case VIRTUALKEYBOARD_EVENT_HIDDEN:
		setup_screen_size(resolution[0], resolution[1]);
		virtualkeyboard_visible = 0;
		break;
	case VIRTUALKEYBOARD_EVENT_INFO:
		vkb_h = virtualkeyboard_visible ? virtualkeyboard_event_get_height(event) : 0;
		setup_screen_size(resolution[0], resolution[1] - vkb_h);
		break;
	default:
		fprintf(stderr, "Unknown keyboard event code %d\n", event_code);
		break;
	}
}

void rescreen(int w, int h){

	int width  = w == -1 ? screen->w : w;
	int height = h == -1 ? screen->h : h;
	int vkb_h = 0;
	screen = SDL_SetVideoMode(width, height, PB_D_PIXELS, SDL_HWSURFACE | SDL_DOUBLEBUF);
	/* reset the font size as well */
	font_uninit();
	buf_clear_all_renders();
	if(font_init(prefs->font_size) == TERM_FAILURE){
		fprintf(stderr, "Couldn't initialize font\n");
		exit_application = 1;
	}

	setup_screen_size(width, height);
	if(virtualkeyboard_visible){
		vkb_h = get_virtualkeyboard_height();
		setup_screen_size(width, height - vkb_h);
	}
}

void toggle_vkeymod(int mod){
	PRINT(stderr, "Toggle modifier %d\n", mod);
	if(vmodifiers & mod){
		vmodifiers &= ~mod;
	}
	else {
		vmodifiers |= mod;
	}
}

static symmenu_t *get_keyhold_actions(int keycode) {
	if (!prefs->keyhold_actions) {
		return NULL;
	}

	int uppercase = 0;
	if (vmodifiers & KEYMOD_SHIFT) {
		uppercase = 1;
	}

	switch (keycode) {
	case KEYCODE_A:
		return prefs->accent_menus[0][uppercase];
	case KEYCODE_B:
		return prefs->accent_menus[1][uppercase];
	case KEYCODE_C:
		return prefs->accent_menus[2][uppercase];
	case KEYCODE_D:
		return prefs->accent_menus[3][uppercase];
	case KEYCODE_E:
		return prefs->accent_menus[4][uppercase];
	case KEYCODE_F:
		return prefs->accent_menus[5][uppercase];
	case KEYCODE_G:
		return prefs->accent_menus[6][uppercase];
	case KEYCODE_H:
		return prefs->accent_menus[7][uppercase];
	case KEYCODE_I:
		return prefs->accent_menus[8][uppercase];
	case KEYCODE_J:
		return prefs->accent_menus[9][uppercase];
	case KEYCODE_K:
		return prefs->accent_menus[10][uppercase];
	case KEYCODE_L:
		return prefs->accent_menus[11][uppercase];
	case KEYCODE_M:
		return prefs->accent_menus[12][uppercase];
	case KEYCODE_N:
		return prefs->accent_menus[13][uppercase];
	case KEYCODE_O:
		return prefs->accent_menus[14][uppercase];
	case KEYCODE_P:
		return prefs->accent_menus[15][uppercase];
	case KEYCODE_Q:
		return prefs->accent_menus[16][uppercase];
	case KEYCODE_R:
		return prefs->accent_menus[17][uppercase];
	case KEYCODE_S:
		return prefs->accent_menus[18][uppercase];
	case KEYCODE_T:
		return prefs->accent_menus[19][uppercase];
	case KEYCODE_U:
		return prefs->accent_menus[20][uppercase];
	case KEYCODE_V:
		return prefs->accent_menus[21][uppercase];
	case KEYCODE_W:
		return prefs->accent_menus[22][uppercase];
	case KEYCODE_X:
		return prefs->accent_menus[23][uppercase];
	case KEYCODE_Y:
		return prefs->accent_menus[24][uppercase];
	case KEYCODE_Z:
		return prefs->accent_menus[25][uppercase];
	}

	return NULL;
}

void handleKeyboardEvent(screen_event_t screen_event)
{
	int screen_val, screen_flags, screen_alt_val;
	int modifiers;
	int num_chars;
	int vkbd_h;
	int metamode_just_set = 0;
	UChar c[CHARACTER_BUFFER];
	UChar *target = c;
	struct timespec now;
	uint64_t now_t, diff_t, metamode_last_t;
	const char* keys = NULL;
	int32_t last_len = 0;
	int32_t bs_i = 0;
	size_t upcase_len = 0;
	UChar backspace = 0x8;

	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &screen_flags);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM, &screen_val);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_ALTERNATE_SYM, &screen_alt_val);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
	//screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_CAP, &cap);

	if (screen_flags & KEY_DOWN) {
        static int first_input = 1;
        if (first_input) { fprintf(stderr,"CJK_INPUT_RECEIVED\n"); fflush(stderr); first_input=0; }
		PRINT(stderr, "The '%d' key was pressed (modifiers: %d) (char %c) (alt %d)\n", (int)screen_val, modifiers, (char)screen_val, (int)screen_alt_val);
		fflush(stdout);

		/* if we're toggling metamode on or off with doubletap */
		if((screen_val == metamode_doubletap_key) && !(screen_flags & KEY_REPEAT)){
			clock_gettime(CLOCK_MONOTONIC, &now);
			now_t = timespec2nsec(&now);
			metamode_last_t = timespec2nsec(&metamode_last);
			diff_t = now_t > metamode_last_t ? now_t - metamode_last_t : now_t;
			if(diff_t <= prefs->metamode_doubletap_delay){
				metamode_toggle();
				metamode_just_set = 1;
			}
			metamode_last = now;
		}

        if(scroll_key(screen_val,screen_flags&KEY_REPEAT))return;
        if(screen_val==KEYCODE_SPACE && ((modifiers|vmodifiers)&KEYMOD_SHIFT)) {
            if(!(screen_flags&KEY_REPEAT))ime_switch_mode();
            return;
        }
        if(metamode && (screen_val=='i'||screen_val=='I')) {
            metamode_toggle();ime_switch_mode();return;
        }
		/* handle sticky keys */
		if(screen_val == KEYCODE_BB_SYM_KEY){
			if(!(screen_flags & KEY_REPEAT)){
				symmenu_toggle(prefs->main_symmenu);
			} else{
				/* they are holding it down */
				symmenu_stick();
			}
			return;
		}

		if(screen_val == KEYCODE_BB_ALT_KEY){
			if (prefs->sticky_alt_key) {
				if(screen_flags & KEY_REPEAT){
					return;
				} else {
					altsym_toggle();
					return;
				}
			}
		}
		
		if(!virtualkeyboard_visible
		   && ((screen_val == KEYCODE_LEFT_SHIFT) || (screen_val == KEYCODE_RIGHT_SHIFT))){
			if (prefs->sticky_shift_key) {
				if(screen_flags & KEY_REPEAT){
					return;
				} else {
					toggle_vkeymod(KEYMOD_SHIFT);
					return;
				}
			}
		}

		/* metamode sticky keys don't trigger repreat */
		if (metamode && !metamode_just_set) {
			keys = keystroke_lookup((char)screen_val, prefs->metamode_sticky_keys);
			if (keys != NULL){
				send_metamode_keystrokes(keys);
				return;
			}
		}

        if(ime_enabled() && !metamode && !altsym_lock && !current_symmenu) {
            int mods=modifiers|vmodifiers;
            int key=ime_translate_key(screen_val);
            if((mods&KEYMOD_SHIFT) && key>='a' && key<='z')key+='A'-'a';
            if((screen_flags&KEY_REPEAT) && key!=IME_BACKSPACE)return;
            if(ime_key(key,mods&(KEYMOD_CTRL|KEYMOD_ALT))) {vmodifiers=0;key_repeat_done=0;return;}
        }
		/* handle key repeat to upcase / metamode */
		if ((screen_flags & KEY_REPEAT) &&
		    prefs->keyhold_actions &&
		    !is_int_member(prefs->keyhold_actions_exempt, screen_val)) {
			if (!key_repeat_done) {
				/* Check for a metamode toggle key first */
				if (screen_val == prefs->metamode_hold_key) {
					io_write_master(&backspace, 1);
					metamode_toggle();
					key_repeat_done = 1;
					return;
				}
				
				symmenu_t *menu = get_keyhold_actions(screen_val);
				if (menu == NULL) {
					return;
				}
				
				last_len = io_upcase_last_write(&target, CHARACTER_BUFFER);
				/* write backspace */
				for(bs_i = 1; bs_i <= last_len; ++bs_i) {
					io_write_master(&backspace, 1);
				}

				/* select the mapping */

				// uppercase automatically if there's no accents or accents disabled
				if ((menu->entries[1].to == NULL) || (!prefs->keyhold_accents)) {
					/* We can upcase, send last_len backspaces and then the upcase char.
					 * Note that this really only works if the program on the other
					 * end of the line understands unicode, and can marry up backspaces
					 * with codepoints, instead of just blindly deleting one byte at a time. */
					send_metamode_keystrokes(menu->entries[0].to);
				} else {
					symmenu_toggle(menu);
				}
				key_repeat_done = 1;
				return;
			} else {
				// We have already handled this key repeat
				return;
			}
		} else {
			key_repeat_done = 0;
		}

		if(metamode && !metamode_just_set){
			keys = keystroke_lookup((char)screen_val, prefs->metamode_keys);
			if(keys != NULL){
				send_metamode_keystrokes(keys);
				metamode_toggle();
				return;
			}
			// else
			keys = keystroke_lookup((char)screen_val, prefs->metamode_func_keys);
			if(keys != NULL){
				int f = 0; /* check custom func commands */
				if(!f && (0 == strncmp(keys, "alt_down", 8)))          { toggle_vkeymod(KEYMOD_ALT);f=1;}
				if(!f && (0 == strncmp(keys, "ctrl_down", 9)))         { toggle_vkeymod(KEYMOD_CTRL);f=1;}
				if(!f && (0 == strncmp(keys, "rescreen", 8)))          { rescreen(-1, -1);f=1;}
				if(!f && (0 == strncmp(keys, "paste_clipboard", 15)))  { if(*ime_spelling())ime_key(IME_ENTER,0);io_paste_from_clipboard();f=1;}
			}
			metamode_toggle();
			return;
		}

		/* handle alt keys */
		if (altsym_lock) {
			keys = keystroke_lookup((char)screen_val, prefs->altsym_entries);
			altsym_toggle();
			if (keys != NULL){
				send_metamode_keystrokes(keys);
				return;
			}
		}

		/* handle sym keys */
		if (current_symmenu != NULL) {
			keys = keystroke_lookup((char)screen_val, current_symmenu->entries);
			if (keys != NULL){
				send_metamode_keystrokes(keys);
				symmenu_toggle(NULL);
				return;
			}
		}

		/* if we have virtual keymods, then put them in, then turn them off */
		modifiers |= vmodifiers;
		vmodifiers = 0;

		/* now process the keypress */
		switch (screen_val) {
		case KEYCODE_PAUSE      :
		case KEYCODE_SCROLL_LOCK:
		case KEYCODE_PRINT      :
		case KEYCODE_SYSREQ     :
		case KEYCODE_BREAK      :
			//case KEYCODE_ESCAPE     :
			//case KEYCODE_BACKSPACE  :
			//case KEYCODE_TAB        :
			//case KEYCODE_BACK_TAB   :
		case KEYCODE_LEFT_ALT   :
		case KEYCODE_RIGHT_ALT  :
		case KEYCODE_LEFT_SHIFT :
		case KEYCODE_RIGHT_SHIFT:
		case KEYCODE_MENU       :
			//case KEYCODE_INSERT     :
			//case KEYCODE_HOME       :
			//case KEYCODE_PG_UP      :
			//case KEYCODE_DELETE     :
			//case KEYCODE_END        :
			//case KEYCODE_PG_DOWN    :
		case KEYCODE_NUM_LOCK   :
			//case KEYCODE_F1         :
			//case KEYCODE_F2         :
			//case KEYCODE_F3         :
			//case KEYCODE_F4         :
			//case KEYCODE_F5         :
			//case KEYCODE_F6         :
			//case KEYCODE_F7         :
			//case KEYCODE_F8         :
			//case KEYCODE_F9         :
			//case KEYCODE_F10        :
			//case KEYCODE_F11        :
			//case KEYCODE_F12        :
			PRINT(stderr, "Modifier %d\n", screen_val);
			break;
		case KEYCODE_LEFT_CTRL  :
		case KEYCODE_RIGHT_CTRL :
			toggle_vkeymod(KEYMOD_CTRL);
			break;
		case KEYCODE_LEFT_HYPER :
		case KEYCODE_RIGHT_HYPER:
			toggle_vkeymod(KEYMOD_CTRL);
			break;
		case KEYCODE_CAPS_LOCK  :
			toggle_vkeymod(KEYMOD_CTRL);
			break;
		default:
			num_chars = ecma48_parse_control_codes(screen_val, modifiers, c);
			int nc;
			for(nc = 0; nc < num_chars; ++nc){
				PRINT(stderr, "Writing 0x%x\n", (int)c[nc]);
			}
			io_write_master((const UChar*)&c, num_chars);
			break;
		}
	}
}

void set_tty_window_size(){
	if(tcsetsize(io_get_master(), rows, cols) < 0){
		PRINT(stderr, "ERROR: tcsetsize() returned <0 (%s). Did not set child pty window size. \n", strerror(errno));
	}
	/* and send SIGWINCH */
	int pgrp;
	if(ioctl(io_get_master(), TIOCGPGRP, &pgrp) != -1){
		killpg(pgrp, SIGWINCH);
	} else {
		PRINT(stderr, "Could not get pgrp of tty: %s\n", strerror(errno));
		/* and assume the pgrp is our own, because we're not allowed to set pgrp.. */
		killpg(getpid(), SIGWINCH);
	}
}

/* Call _after_ we have calculated the text size */
void setup_screen_size(int s_w, int s_h){

	if(s_w <= 1 || s_h <= 1){
		/* refusing to do that */
		return;
	}

	int old_rows = rows;
    int old_top_line=buf->top_line;
	int old_bottom_line = buf_bottom_line();
	ime_area_height = ((ime_ready() && ime_enabled())?6:2)*text_height;
	rows = (s_h-ime_area_height) / text_height;
	if(rows<2)rows=2;
	cols = s_w / text_width;
	if (cols > MAX_COLS) cols = MAX_COLS;
	if (buf->col > cols) buf->col = cols;
	for (int k=0; k<TEXT_BUFFER_SIZE; ++k) buf_repair_wide(buf->text[k], cols);
	int diff_rows = rows - old_rows;
	PRINT(stderr, "Rows: %d Cols: %d\n", rows, cols);

	/* calculate where we should start drawing */
	if(buf->line && old_rows > rows){ // new size smaller
		buf->top_line = buf->line - buf->top_line + 1 > rows ? buf->line - rows + 1 : buf->top_line;
		// clear covered up lines that are below the cursor
		int toclear = old_bottom_line - buf_bottom_line();
		PRINT(stderr, "new rows: %d, new bottom: %d, old rows: %d, old_bottom: %d, clearing %d lines (SIZE: %d)\n",
		      rows, buf_bottom_line(), old_rows, old_bottom_line, toclear, TEXT_BUFFER_SIZE);
		buf_erase_lines(buf_bottom_line() + 1, toclear);
	} else if (buf->line && old_rows < rows){ // new size bigger
		//buf->top_line = buf->line - rows + 1 < 0 ? 0 : buf->line - rows + 1;
		buf->top_line = buf->top_line - diff_rows < 0 ? 0 : buf->top_line - diff_rows;
		// clear newly revealed lines of artifacts
		int toclear = buf_bottom_line() < TEXT_BUFFER_SIZE ?
			buf_bottom_line() - old_bottom_line :
			TEXT_BUFFER_SIZE - 1 - old_bottom_line;
		PRINT(stderr, "new rows: %d, new bottom: %d, old rows: %d, old_bottom: %d, clearing %d lines\n",
		      rows, buf_bottom_line(), old_rows, old_bottom_line, toclear);
		buf_erase_lines(old_bottom_line + 1, toclear);
	}

    history_resize_top(buf->top_line-old_top_line);

	/* and reset the scroll region */
	sr.top = 1;
	sr.bottom = rows;

	// set the tty size
	set_tty_window_size();
}

void lock_input(){
	if(SDL_LockMutex(input_mutex) == -1){
		fprintf(stderr, "Couldn't lock input mutex - exiting\n");
		exit_application = 1;
	}
}
void unlock_input(){
	if(SDL_UnlockMutex(input_mutex) == -1){
		fprintf(stderr, "Couldn't unlock input mutex - exiting\n");
		exit_application = 1;
	}
}

void indicate_event_input(){
	char *indicate_buf = "w";
	/* indicate that the render thread should run. Note that
	 * we are logging errors here, but aren't doing anything with them. */
	if(write(event_pipe[1], (void*)indicate_buf, 1) < 0){
		fprintf(stderr, "Error writing to event pipe: %d\n", errno);
	}
}


/* This function is intended for resizing the number of
 * colums after app init */
void set_screen_cols(int ncols){
	/* the user wants this number of columns */
	if (prefs->allow_resize_columns) {
		int new_fontsize = preferences_guess_best_font_size(prefs, ncols);
		font_uninit();
		buf_clear_all_renders();
		if(font_init(new_fontsize) == TERM_FAILURE){
			fprintf(stderr, "Error setting new font size\n");
			exit_application = 1;
		} else {
			setup_screen_size(screen->w, screen->h);
			/* and force the number of columns */
			cols = ncols;
			set_tty_window_size();
		}
	}
}

static int sdl_init() {
	/* init the input mutex */
	input_mutex = SDL_CreateMutex();
	
	/* init the event input pipe */
	if(pipe(event_pipe) == -1){
		fprintf(stderr, "Couldn't create event pipe\n");
		return TERM_FAILURE;
	}

	/* Initialize SDL */
	if (SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		PRINT(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
		return TERM_FAILURE;
	}
	PRINT(stderr, "Post SDL_Init()\n");

	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	
	// We get keyboard events from the SysWMEvents
	SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);

	screen_window_t window;
	screen_context_t context;

	SDL_SysWMinfo info;
	if(get_wm_info(&info) != 1){
		fprintf(stderr, "Couldn't get WM Info: %s\n",SDL_GetError());
		return TERM_FAILURE;
	}

	/* grab the orientation and resolution so we can start up that way */
	window = info.mainWindow;
	context = info.context;
	int wm_size[2] = {0,0};

	if (screen_get_window_property_iv(window, SCREEN_PROPERTY_SIZE, wm_size)) {
		fprintf(stderr, "Cannot get resolution: %s", strerror(errno));
		return TERM_FAILURE;
	}
	PRINT(stderr, "wm size returned: w:%d, h:%d\n", wm_size[0], wm_size[1]);

	/* Initialize the TTF library */
	if ( TTF_Init() < 0 ) {
		PRINT(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
		SDL_Quit();
		return TERM_FAILURE;
	}

	/* set screen idle mode */
	if(!prefs->screen_idle_awake){
		setenv("SCREEN_IDLE_NORMAL", "1", 0);
	}

	/* check to verify if the wm returned the native resolution */
	if (getenv("WIDTH") != NULL && getenv("HEIGHT") != NULL) {
		if(wm_size[0] != atoi(getenv("WIDTH")) || wm_size[1] != atoi(getenv("HEIGHT"))){
			fprintf(stderr, "SDL_WMInfo returned non-native screen resolution - forcing\n");
			wm_size[0] = atoi(getenv("WIDTH"));
			wm_size[1] = atoi(getenv("HEIGHT"));
		}
	}

	screen = SDL_SetVideoMode(wm_size[0], wm_size[1], PB_D_PIXELS, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if ( screen == NULL ) {
		PRINT(stderr, "Couldn't set %d x %d x %d video mode: %s\n", wm_size[0], wm_size[1], PB_D_PIXELS, SDL_GetError());
		TTF_Quit();
		SDL_Quit();
		return TERM_FAILURE;
	}

    const char *parent_group = getenv("TERM49_PARENT_GROUP");
    if (parent_group && parent_group[0]) {
        SDL_SysWMinfo attach_info;
        if (get_wm_info(&attach_info) != 1) return TERM_FAILURE;
        if (screen_join_window_group(attach_info.window, parent_group)) {
            fprintf(stderr,"CJK_ATTACH_FAILED group=%s error=%s\n",parent_group,strerror(errno));
            return TERM_FAILURE;
        }
        int z=10, visible=1, zero[3]={0,0,0};
        screen_set_window_property_iv(attach_info.window, SCREEN_PROPERTY_ZORDER, &z);
        screen_set_window_property_iv(attach_info.window, SCREEN_PROPERTY_VISIBLE, &visible);
        screen_flush_context(attach_info.context, 0);
        int tab=screen_tab_focus(attach_info.window, zero, SCREEN_EVENT_KEYBOARD, 0);
        int focus=-1;
        screen_get_window_property_iv(attach_info.window, SCREEN_PROPERTY_FOCUS, &focus);
        fprintf(stderr,"CJK_ATTACHED group=%s focus=%d tab_result=%d\n",parent_group,focus,tab);
        fflush(stderr);
    }

	if(font_init(prefs->font_size) == TERM_FAILURE){
		PRINT(stderr, "Couldn't initialize font\n");
		TTF_Quit();
		SDL_Quit();
		return TERM_FAILURE;
	}

	/* Don't show the mouse icon */
	SDL_ShowCursor(SDL_DISABLE);

	/* we allocate as much buffer as we will ever need */
	int largest_dimension = screen->w > screen->h ? screen->w : screen->h;
	MAX_ROWS = largest_dimension / MIN_FONT_SIZE;
	MAX_COLS = largest_dimension / MIN_FONT_SIZE;
	TEXT_BUFFER_SIZE = MAX_ROWS * 2;
	fprintf(stderr, "Allocating %d rows and %d cols\n",TEXT_BUFFER_SIZE, MAX_COLS);

	/* initialize the number of rows and columns */
	rows = screen->h / text_height;
	cols = screen->w / text_width;

	if(buf_init() == TERM_FAILURE){
		PRINT(stderr, "Couldn't initialize font\n");
		TTF_Quit();
		SDL_Quit();
		return TERM_FAILURE;
	}

	setup_screen_size(screen->w, screen->h);
	
	/* and set the last 'press' */
	clock_gettime(CLOCK_MONOTONIC, &metamode_last);

	ecma48_init();

	return TERM_SUCCESS;
}

void uninit(){
    ime_close();

	buf_uninit();

	SDL_DestroyMutex(input_mutex);

	font_uninit();
	SDL_FreeSurface(screen);

	ecma48_uninit();

	TTF_Quit();
	SDL_Quit();

	destroy_preferences(prefs);

	io_uninit();
}

SDL_Color adjust_color(SDL_Color in, struct font_style sty){
	int i;
	if(sty.style & TTF_STYLE_BOLD){
		for(i = 0; i < 8; ++i){
			if((in.b == term_colors[i].b) &&
			   (in.g == term_colors[i].g) &&
			   (in.r == term_colors[i].r)){
				in = term_colors[i+8];
				break;
			}
		}
	}
	return in;
}

void render() {

	int offset;
	struct screenchar* sc;
	SDL_Surface* torender;
	UChar str[2];
	str[1] = NULL;

	/* Set the background */
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, default_bg_color.r, default_bg_color.g, default_bg_color.b));

	for(int i = 0; i < rows; ++i){
		float x = 0.0;
		float y = text_height * (i);
		
		for(int j = 0; j < cols; ++j){
			/* guard against screen rotations that push the bottom of the screen past the
			 * bottom of the buffer. */
			sc = history_active()?history_cell(i,j):(i+buf->top_line < TEXT_BUFFER_SIZE ? &buf->text[i+buf->top_line][j] : &blank_sc);
            if(!sc)sc=&blank_sc;
			if (sc->c == 0) { x += advance; continue; }
			if((sc->surface == NULL) && (sc->c != 0)){
				// we have added a new char, but not rendered it yet
				str[0] = sc->c;
				TTF_SetFontStyle(font, sc->style.style);
				if(buf->inverse_video){
					sc->surface = render_font_cell(str, adjust_color(sc->style.bg_color, sc->style), sc->style.fg_color);
				} else {
					sc->surface = render_font_cell(str, adjust_color(sc->style.fg_color, sc->style), sc->style.bg_color);
				}
				if(sc->surface == NULL){
					PRINT(stderr, "Rendering failed for char %d\n", (int)sc->c);
				}
			}
			if(sc->surface == NULL || flash){
				// no glyph here - render blank
				if(buf->inverse_video){
					torender = flash ? blank_surface : flash_surface;
				} else {
					torender = flash ? flash_surface : blank_surface;
				}
			} else {
				torender = sc->surface;
			}

			/* construct the destination rectangle */
			SDL_Rect destrect;
			destrect.x = x;
			destrect.y = y;
			destrect.w = torender->w;
			destrect.h = torender->h;
			if(SDL_BlitSurface(torender, NULL, screen, &destrect) != 0){
				PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
			}
			x += advance;
		}
	}

	if (draw_cursor && !history_active()){
		// draw the cursor
		/* Free the old cursor if we have one */
		SDL_FreeSurface(inv_cursor);
		inv_cursor = NULL;
		/* Get the character under the cursor */

		int drawcols = buf->col;
		if(buf->col == cols){
			// Don't draw off the edge - also make backspace from the right margin work 'right'
			drawcols -= 1;
		}

		if (drawcols > 0 && buf->text[buf->line][drawcols].c == 0) --drawcols;
		sc = &buf->text[buf->line][drawcols];
		if(sc->c){
			str[0] = sc->c;
			TTF_SetFontStyle(font, sc->style.style);
			if(buf->inverse_video){
				inv_cursor = render_font_cell(str, adjust_color(sc->style.fg_color, sc->style), sc->style.bg_color);
			} else {
				inv_cursor = render_font_cell(str, adjust_color(sc->style.bg_color, sc->style), sc->style.fg_color);
			}
			if(inv_cursor == NULL){
				PRINT(stderr, "Rendering failed for char %d\n", (int)sc->c);
			}
		}
		cursor_x = drawcols;
		cursor_y = buf->line - buf->top_line;

		SDL_Rect destrect;
		destrect.x = cursor_x * advance;
		destrect.y = cursor_y * text_height;
		destrect.w = cursor->w;
		destrect.h = cursor->h;
		if(inv_cursor != NULL){
			SDL_BlitSurface(inv_cursor, NULL, screen, &destrect);
		} else {
			SDL_BlitSurface(buf->inverse_video ? blank_surface: cursor, NULL, screen, &destrect);
		}
	}

	if(metamode && metamode_cursor != NULL){
		/* draw the metamode cursor */
		SDL_Rect destrect;
		destrect.x = (cols-1) * advance;
		destrect.y = 0;
		destrect.w = metamode_cursor->w;
		destrect.h = metamode_cursor->h;
		SDL_BlitSurface(metamode_cursor, NULL, screen, &destrect);
	}

	if(vmodifiers & KEYMOD_CTRL){
		SDL_Rect destrect;
		destrect.x = (cols-1) * advance;
		destrect.y = 1 * text_height;
		destrect.w = ctrl_key_indicator->w;
		destrect.h = ctrl_key_indicator->h;
		SDL_BlitSurface(ctrl_key_indicator, NULL, screen, &destrect);
	}

	if(vmodifiers & KEYMOD_ALT){
		SDL_Rect destrect;
		destrect.x = (cols-1) * advance;
		destrect.y = 2 * text_height;
		destrect.w = alt_key_indicator->w;
		destrect.h = alt_key_indicator->h;
		SDL_BlitSurface(alt_key_indicator, NULL, screen, &destrect);
	}

	if(vmodifiers & KEYMOD_SHIFT){
		SDL_Rect destrect;
		destrect.x = (cols-1) * advance;
		destrect.y = 3 * text_height;
		destrect.w = shift_key_indicator->w;
		destrect.h = shift_key_indicator->h;
		SDL_BlitSurface(shift_key_indicator, NULL, screen, &destrect);
	}

	if (altsym_lock) {
		SDL_Rect destrect;
		destrect.x = (cols-1) * advance;
		destrect.y = 3 * text_height;
		destrect.w = shift_key_indicator->w;
		destrect.h = shift_key_indicator->h;
		SDL_BlitSurface(altsym_indicator, NULL, screen, &destrect);
	}

	if ((current_symmenu != NULL) && (current_symmenu->surface != NULL)) {
		/* blit symmenu surface */
		SDL_Rect destrect;
		destrect.w = current_symmenu->surface->w;
		destrect.h = current_symmenu->surface->h;
		destrect.x = 0;
		destrect.y = screen->h - current_symmenu->surface->h;;
	
		if (SDL_BlitSurface(current_symmenu->surface, NULL, screen, &destrect) != 0) {
			PRINT(stderr, "Symmenu blit failed: %s\n", SDL_GetError());
			return;
		}
	}

	ime_render_panel();
    scroll_render_controls();
	SDL_Flip(screen);

	if(flash){
		/* turn it off */
		flash = 0;
		/* write to the input pipe so we run again */
		indicate_event_input();
	}

}

static int pty_init() {
	// Set up the ttys and fork

	struct winsize winp;

	/* some sensible defaults - we change these later */
	winp.ws_row = 24;
	winp.ws_col = 80;
	winp.ws_xpixel = 1024;
	winp.ws_ypixel = 600;

	int pty_ret;
	int fd;
	int uid = getuid();
	int gid = getgid();
	char cttyname[L_ctermid];
	char envstr[100];
	int slave_fd;
	int master_fd;

	pty_ret = openpty(&master_fd, &slave_fd, slave_ptyname, NULL, &winp);
	if (pty_ret != 0){
		// error
		PRINT(stderr, "openpty returned: %s\n", strerror(errno));
		close(master_fd);
		close(slave_fd);
		return TERM_FAILURE;
	} else {
		PRINT(stderr, "openpty returned name: %s\n", slave_ptyname);
	}

	// turn off blocking on the master pty
	fcntl(master_fd, F_SETFL, fcntl(master_fd, F_GETFL) | O_NONBLOCK);

	// store the master_fd in IO
	io_set_master(master_fd);

	// fork and exec
	child_pid = fork();

	if (child_pid == 0) {
		// Child
		/*
		  struct termios tios;
		  if (tcgetattr(STDIN_FILENO, &tios) >= 0)
		  {
		  tios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		  tios.c_oflag &= ~(ONLCR);
		  (void) tcsetattr(STDIN_FILENO, TCSANOW, &tios);
		  }
		*/

		PRINT(stderr, "fork returned in child\n");
		ctermid(cttyname);
		PRINT(stderr, "controlling tty is: %s\n", cttyname);

		if(setuid(uid)<0) {PRINT(stderr, "ERROR (setuid)\n");}
		if(setgid(gid)<0) {PRINT(stderr, "ERROR (setgid)\n");}
		if(setsid()<0) {PRINT(stderr, "ERROR (setsid)\n");}

		if(ioctl(slave_fd, TIOCSCTTY, NULL)) {
			PRINT(stderr, "ERROR! (ioctl): %s\n", strerror(errno));
		}

		dup2(slave_fd, STDIN_FILENO);
		dup2(slave_fd, STDOUT_FILENO);
		dup2(slave_fd, STDERR_FILENO);

		ecma48_setenv();

		/* add in our private binary path */
		char* home = getenv("SANDBOX");
		char* path = getenv("PATH");
		char* root = "app/native/root/bin";
		char* newpath;
		int err = 0;
		int newpath_len = 0;
		if(home == NULL || path == NULL){
			fprintf(stderr, "Could not get $HOME or $PATH - not setting private bin dir.\n");
		} else {
			newpath_len = strlen(home) + strlen(path) + strlen(root) + 10;
			newpath = calloc(newpath_len, sizeof(char));
			if(newpath == NULL){
				fprintf(stderr, "Could not calloc new $PATH - not setting private bin dir..\n");
			} else {
				err = snprintf(newpath, newpath_len, "PATH=%s/%s:%s", home, root, path);
				if(err > 0){
					err = putenv(strdup(newpath));
					if(err < 0){
						fprintf(stderr, "Error in putenv: %d\n%s - private bin may not bein $PATH.\n", errno, newpath);
					}
				} else {
					fprintf(stderr, "Error snprintf setting $PATH: %d\n", errno);
				}
				free(newpath);
			}
		}

		/* Set LC_CTYPE=en_US.UTF-8
		 * Which can be overridden in .profile */
		setenv("LC_CTYPE", "en_US.UTF-8", 0);
		if (cjk_render_test) { execl("/bin/sh", "sh", "-c", "sleep 30", (char*)0); _exit(127); }
		if(execl("../app/native/root/bin/mksh", "mksh", "-l", (char*)0) == -1){
			execl("/bin/sh", "sh", "-l", (char*)0);
		}
	}
	if (child_pid == -1){
		PRINT(stderr, "fork returned: %s\n", strerror(errno));
		return TERM_FAILURE;
	}

	// close the slave_fd, not needed anymore
	close(slave_fd);
	return TERM_SUCCESS;
}

extern int SDL_PrivateQuit(void);
void sig_child(int signo){
	int status;

	int old_errno = errno;

	if(waitpid(child_pid, &status, WNOHANG)){
		if(WIFEXITED(status)){
			PRINT(stderr, "Child %d exited normally with status %d\n", child_pid, WEXITSTATUS(status));
		} else {
			PRINT(stderr, "Child %d exited abnormally\n", child_pid);
		}
		exit_application = 1;
		SDL_PrivateQuit();
	} else {
		PRINT(stderr, "Got SIGCHILD for process other than %d\n", child_pid);
	}
	errno = old_errno;
}

/* This function is run in an SDL_Thread, and will check
 * for either input event indication or data from the
 * shell, then run the render loop
 */
int run_render(void* data){

	fd_set fds;
	char ev_buf[100];
	int n = 0;
	UChar lbuf[READ_BUFFER_SIZE];
	ssize_t num_chars = 0;
	int master = io_get_master();
	while(!exit_application){
		FD_ZERO(&fds);
		FD_SET(master, &fds);
		FD_SET(event_pipe[0], &fds);
		n = select(1+max(master, event_pipe[0]), &fds, NULL, NULL, NULL);
		if(n < 0){
			printf("Error calling select on inputs: %d\n", errno);
		} else {
			if(FD_ISSET(master, &fds)){
				lock_input();
				// Read anything from the child
				while ((num_chars = io_read_master(lbuf, READ_BUFFER_SIZE)) > 0){
					ecma48_filter_text(lbuf, num_chars);
				}
				unlock_input();
			}
			if(FD_ISSET(event_pipe[0], &fds)){
				// Just read the stuff and throw it away
				read(event_pipe[0], (void*)ev_buf, 99);
			}
		}
		PRINT(stderr, "Render Loop\n");
		lock_input();
		render();
		unlock_input();
	}
	/* never reached */
	return 0;
}

static void scroll_feed(const char *s) {
    UChar out[2048];int n=io_read_utf8_string(s,strlen(s),out);ecma48_filter_text(out,n);
}
/* Drive the installed SDL library's actual TCO callback, then consume the SDL
 * events exactly as the application does (no synthetic press for a drag). */
extern int handleTouchScreen(int x,int y,int tap,int hold);
static int scroll_drain_mouse(void) {
    SDL_Event e;int count=0;
    while(SDL_PeepEvents(&e,1,SDL_GETEVENT,SDL_MOUSEEVENTMASK)>0){assert(scroll_mouse_event(&e));count++;}
    return count;
}
static void scroll_driver_test(void) {
    SDL_Event e;int at;
    while(SDL_PeepEvents(&e,1,SDL_GETEVENT,SDL_MOUSEEVENTMASK)>0){}
    history_live();touch_down=motion_only=0;
    handleTouchScreen(220,70,0,0);assert(scroll_drain_mouse()>0);assert(!history_active());
    handleTouchScreen(220,170,0,0);assert(scroll_drain_mouse()>0);assert(history_active());at=history_position();
    handleTouchScreen(220,300,0,0);assert(scroll_drain_mouse()>0);assert(history_position()<at);
    /* A pause must not join unrelated touches into one huge scroll. */
    at=history_position();scroll_motion_event(220,80,motion_time+500);assert(history_position()==at);
    /* Buttons go through real TCO tap -> SDL down/up, even while live. */
    handleTouchScreen(screen->w-80,scroll_bar_y()+10,1,0);assert(scroll_drain_mouse()>=2);assert(!history_active());
    handleTouchScreen(80,scroll_bar_y()+10,1,0);assert(scroll_drain_mouse()>=2);assert(history_active());at=history_position();
    handleTouchScreen(screen->w/2,scroll_bar_y()+10,1,0);assert(scroll_drain_mouse()>=2);assert(!history_active()||history_position()>at);
    handleTouchScreen(screen->w-80,scroll_bar_y()+10,1,0);scroll_drain_mouse();assert(!history_active());
    fprintf(stderr,"TCO_EVENT_TEST_OK: real SDL motion-only drag; gap reset; tap up/down/back buttons\n");
}

static void scroll_integration_test(void) {
    int i,r=rows;char line[180];
    buf_reset_text_buffer(buf);sr.top=1;sr.bottom=rows;
    for(i=0;i<4300;i++){
        snprintf(line,sizeof(line),"%04d 中文回看：你好，世界！ ABC 123\r\n",i);scroll_feed(line);
    }
    assert(history_count()==HISTORY_LIMIT);
    metamode=1;assert(scroll_key('u',0));assert(history_active());assert(!metamode);
    int at=history_position();UChar first=history_cell(0,0)->c;
    assert(scroll_key('x',0));assert(history_position()==at); // typing is consumed
    assert(scroll_key(KEYCODE_RETURN,0));assert(history_active());
    scroll_feed("streaming new output\r\n");assert(history_position()==at);assert(history_cell(0,0)->c==first);
    scroll_feed("\033[3J");assert(history_count()==0);assert(history_active());assert(history_cell(0,0)->c==first);
    rows=7;cols=9;assert(history_cell(0,8));rows=r;cols=51;history_invalidate();
    assert(scroll_key('b',0));assert(!history_active());assert(scroll_key('b',KEY_REPEAT));assert(!scroll_key('x',0));
    /* Full screen scroll captures; interior scrolling does not. */
    history_clear(buf);sr.top=1;sr.bottom=rows;buf_scroll_scroll_region();assert(history_count()==1);
    sr.top=2;sr.bottom=rows-1;buf_scroll_scroll_region();assert(history_count()==1);sr.top=1;sr.bottom=rows;
    /* Alternate-screen history remains isolated. */
    buf_save_text();assert(history_count()==0);scroll_feed("alternate\r\n");buf_restore_text();assert(history_count()==1);
    /* Touch down is deferred: swiping from the Meta hitbox cannot toggle it. */
    history_clear(buf);buf_reset_text_buffer(buf);
    for(i=0;i<65;i++){snprintf(line,sizeof(line),"%04d 中文对话记录 / conversation history\r\n",i);scroll_feed(line);}
    metamode=0;scroll_touch_down(30,30);scroll_touch_move(32,250);scroll_touch_up(32,250);
    assert(history_active());assert(!metamode);at=history_position();
    scroll_touch_down(300,400);scroll_touch_move(300,310);scroll_touch_up(300,310);assert(history_position()>at);
    scroll_touch_down(300,rows*text_height+10);scroll_touch_up(300,rows*text_height+10);assert(!history_active());
    scroll_driver_test();
    /* Pinyin preedit survives browsing; scroll keys don't become spelling. */
    ime_switch_mode();const char *py="nihao";while(*py)ime_key(*py++,0);
    assert(!strcmp(ime_spelling(),"nihao"));metamode=1;scroll_key('u',0);
    assert(history_active());scroll_key('x',0);assert(!strcmp(ime_spelling(),"nihao"));
    history_move(10000);assert(history_position()==0);
    render();assert(SDL_SaveBMP(screen,"/accounts/1000/shared/misc/term49-scroll/scroll-test.bmp")==0);
    history_live();assert(!strcmp(ime_spelling(),"nihao"));
    render();assert(SDL_SaveBMP(screen,"/accounts/1000/shared/misc/term49-scroll/pinyin-test.bmp")==0);
    ime_cancel();ime_switch_mode();
    render();assert(SDL_SaveBMP(screen,"/accounts/1000/shared/misc/term49-scroll/controls-test.bmp")==0);
    /* Pi full redraw clears stale saved lines and builds fresh history. */
    scroll_feed("\033[2J\033[H\033[3J");assert(history_count()==0);
    for(i=0;i<50;i++){snprintf(line,sizeof(line),"Pi redraw %d 中文\r\n",i);scroll_feed(line);}
    assert(history_count()==50-rows+1);history_move(10000);assert(history_active());
    assert(history_cell(0,0)->c=='P');assert(history_cell(0,10)->c=='0');
    history_live();
    fprintf(stderr,"SCROLL_TEST_OK: ring4096 wrap; frozen output/3J; resize; input shield; full/partial scroll; alternate screen; swipe directions; Pinyin preserved; Pi redraw; v2 controls/TCO event queue\n");
}

int main(int argc, char **argv) {
	int rc;
	cjk_render_test = argc > 1 && strcmp(argv[1], "--render-test") == 0;
	setenv("TERM49_CJK", "1", 1);

	/* Switch to our home directory */
	char* home = getenv("HOME");
	if(home != NULL){ chdir(home); }
	
	prefs = read_preferences(PREFS_FILE_PATH);
	if (is_passport()) {
		prefs->auto_show_vkb = 1;
	}

	/* set auto orientation */
	setenv("AUTO_ORIENTATION", "1", 0);

	/* Initialize IO */
	if (TERM_SUCCESS != io_init(prefs)) {
		PRINT(stderr, "Unable to initialize IO\n");
		uninit();
		return TERM_FAILURE;
	}
	
	/* Initialize pty */
	if (TERM_SUCCESS != pty_init()) {
		PRINT(stderr, "Unable to initialize pty/tty\n");
		uninit();
		return TERM_FAILURE;
	}

	/* Install signal handler for SIGCHILD */
	struct sigaction act;
	act.sa_handler = &sig_child;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &act, NULL) < 0) {
		PRINT(stderr, "sigaction failed\n");
		uninit();
		return TERM_FAILURE;
	}

    {
        char userdir[1024],userdict[1100];
        snprintf(userdir,sizeof(userdir),"%s/.term49-ime",getenv("HOME")?getenv("HOME"):".");
        mkdir(userdir,0700);chmod(userdir,0700);
        snprintf(userdict,sizeof(userdict),"%s/user.dat",userdir);
        mode_t oldmask=umask(0077);
        int ready=ime_init("/accounts/1000/shared/misc/term49-ime/dict_pinyin.dat",userdict,ime_send_text);
        umask(oldmask);
        fprintf(stderr,"PINYIN_READY=%d\n",ready);
    }
	/* initialize SDL video etc */
	if (TERM_SUCCESS != sdl_init()) {
		PRINT(stderr, "Unable to initialize SDL\n");
		uninit();
		return TERM_FAILURE;
	}


  fprintf(stderr,"TERM49_CJK_READY cols=%d rows=%d cell=%d line=%d\n",cols,rows,advance,text_height);
  fflush(stderr);
  if(argc>1 && !strcmp(argv[1],"--scroll-test")){
    scroll_integration_test();signal(SIGCHLD,SIG_IGN);kill(child_pid,SIGTERM);waitpid(child_pid,NULL,0);uninit();return 0;
  }
  if (cjk_render_test) {
    const char *sample="Term49 CJK - Andale + Noto\r\n"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\nabcdefghijklmnopqrstuvwxyz 0123456789\r\n"
      "\xe4\xb8\xad\xe6\x96\x87\xe6\x98\xbe\xe7\xa4\xba\xe6\xad\xa3\xe5\xb8\xb8 ABC 123\r\n"
      "GLM 5.3 Flash /help /exit\r\n"
      "\xe4\xbd\xa0\xe5\xa5\xbd\xef\xbc\x8c\xe4\xb8\x96\xe7\x95\x8c\xef\xbc\x81\r\n";
    UChar decoded[1024];
    int n=io_read_utf8_string(sample,strlen(sample),decoded);
    ecma48_filter_text(decoded,n);
    for (int k=0;k<cols/2+5;k++) { UChar h=0x4e2d; ecma48_filter_text(&h,1); }
    UChar eol[]={13,10};ecma48_filter_text(eol,2);
    ime_switch_mode();
    const char *py="nihao";while(*py)ime_key(*py++,0);
    render();
    int saved=SDL_SaveBMP(screen,"/accounts/1000/shared/misc/term49-ime/render-test.bmp");
    fprintf(stderr,"RENDER_TEST saved=%d cursor_col=%d\n",saved,buf->col);
    SDL_Delay(2000);
    signal(SIGCHLD,SIG_IGN);kill(child_pid,SIGTERM);waitpid(child_pid,NULL,0);
    uninit();return saved?1:0;
  }

    {
        const char *banner="\033[32mTerm49 Pinyin + Scroll v2 | Tap buttons\033[0m\r\n";
        UChar decoded[128];
        int n=io_read_utf8_string(banner,strlen(banner),decoded);
        ecma48_filter_text(decoded,n);
    }
	/* render the symmenus */
	prefs->main_symmenu->surface = render_symmenu(screen, prefs, prefs->main_symmenu);
	for (char c = 'a'; c <= 'z'; ++c) {
		size_t idx = (size_t)(c - 'a');

		// lowercase
		symmenu_t *m = prefs->accent_menus[idx][0];
		if (m->entries[1].to != NULL) {
			m->surface = render_symmenu(screen, prefs, m);
		}

		// uppercase
		m = prefs->accent_menus[idx][1];
		if (m->entries[1].to != NULL) {
			m->surface = render_symmenu(screen, prefs, m);
		}
	}

	if (prefs->auto_show_vkb) {
		virtualkeyboard_show();
	}

	/* start up main event loop */
	SDL_Thread *render_thread = SDL_CreateThread(run_render, NULL);
	while (!exit_application) {

		//Request and process all available events
		SDL_Event event;

		SDL_WaitEvent(&event);
		lock_input();
		switch (event.type) {
		case SDL_QUIT:
			exit_application = 1;
			break;
		case SDL_VIDEORESIZE:
			rescreen(event.resize.w, event.resize.h);
			break;
		case SDL_KEYDOWN:
			{
				fprintf(stderr, "SDL_KEYDOWN\n");
				UChar uc;
				char sdlkey = event.key.keysym.sym;
				uc = (UChar)sdlkey;
				if(!scroll_key(uc,0))io_write_master(&uc, 1);
			}
			break;
		case SDL_SYSWMEVENT:
			{
				bps_event_t* bps_event = event.syswm.msg->event;
				int screene_type;
				int domain = bps_event_get_domain(bps_event);
				PRINT(stderr, "Unhandled SYSWMEVENT: %d\n", domain);
			}
			break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEMOTION:
            scroll_mouse_event(&event);
            break;
		case SDL_ACTIVEEVENT:
			handle_activeevent(event.active.gain, event.active.state);
			break;
		default:
			PRINT(stderr, "Unknown Event: %d\n", event.type);
			break;
		}
		indicate_event_input();
		unlock_input();
	}

	PRINT(stderr, "Exiting run loop\n");
	SDL_KillThread(render_thread);
	virtualkeyboard_hide();
	uninit();

	return 0;
}
