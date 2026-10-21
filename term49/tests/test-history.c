#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "SDL.h"
#include "buffer.h"
#include "scrollback.h"
buf_t owner,*buf=&owner;
int rows=12,cols=51,TEXT_BUFFER_SIZE=360;
struct font_style default_text_style;
void buf_free_char(struct screenchar *c){if(c->surface)free(c->surface);c->surface=NULL;}
int buf_char_width(UChar c){return c>=0x4e00?2:c?1:0;}
int main(void){
 struct screenchar line[180]={0};int i,k;buf->text=calloc(360,sizeof(*buf->text));
 for(i=0;i<360;i++)buf->text[i]=calloc(180,sizeof(**buf->text));
 for(i=0;i<6000;i++){
  line[0].c=i;line[1].c=0x4e2d;line[2].c=0;line[1].surface=(void*)1;
  history_append(line,51);
 }
 assert(history_count()==4096);history_move(5000);assert(history_position()==0);
 assert(history_cell(0,0)->c==1904);assert(history_cell(0,1)->c==0x4e2d);assert(!history_cell(0,1)->surface);
 for(k=0;k<150;k++){
  rows=2+k%178;cols=2+k%178;
  assert(history_cell(rows-1,cols-1));history_cell(0,0)->surface=malloc(sizeof(SDL_Surface));
  history_move(1);assert(history_position()==0);
 }
 cols=2;assert(history_cell(0,1)->c==' ');cols=51;
 history_clear(buf);assert(history_count()==0);assert(history_cell(0,0)->c==1904);
 for(i=0;i<10000;i++)history_append(line,1+i%180);
 assert(history_cell(0,0)->c==1904);history_move(-99999);assert(!history_active());
 history_move(10);assert(history_active());
 history_live();int before=history_count();buf->top_line=5;history_resize_top(5);assert(history_count()==4096);
 buf->top_line=1;history_resize_top(-4);assert(history_count()==4092);
 history_move(10);assert(history_active());history_live();history_destroy(buf);
 assert(!history_count());history_move(1);assert(!history_active());
 for(i=0;i<360;i++)free(buf->text[i]);free(buf->text);
 puts("HISTORY_ASAN_OK: wrap, bounded storage, frozen snapshot, width clipping, resize cache, clear, repeated allocations, teardown");
}
