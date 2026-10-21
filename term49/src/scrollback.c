/* Modified for BB10 Classic Toolkit, 2026-10-21; see ../MODIFICATIONS.md. */
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "buffer.h"
#include "scrollback.h"
extern buf_t *buf;
extern int rows, cols, TEXT_BUFFER_SIZE;
extern struct font_style default_text_style;
struct history_line { struct screenchar *cells; int width; };
struct history { struct history_line ring[HISTORY_LIMIT]; int start, count; };
/* Frozen copy: output and CSI 3J can continue without moving the reader. */
static struct history_line *snapshot;
static int total, position;
static struct screenchar *view;
static int view_rows,view_cols,view_position=-1;
static void release_line(struct history_line *l) { free(l->cells);l->cells=NULL;l->width=0; }
static int copy_line(struct history_line *dst,struct screenchar *src,int width) {
    int i;dst->cells=calloc(width,sizeof(*src));if(!dst->cells)return 0;
    dst->width=width;
    for(i=0;i<width;i++){dst->cells[i]=src[i];dst->cells[i].surface=NULL;}
    return 1;
}
void history_invalidate(void) {
    int i;if(view){for(i=0;i<view_rows*view_cols;i++)buf_free_char(view+i);free(view);}
    view=NULL;view_position=-1;
}
void history_live(void) {
    int i;history_invalidate();
    if(snapshot){for(i=0;i<total;i++)release_line(snapshot+i);free(snapshot);}
    snapshot=NULL;total=position=0;
}
void history_clear(buf_t *owner) {
    int i;struct history *h=owner->history;if(!h)return;
    for(i=0;i<HISTORY_LIMIT;i++)release_line(h->ring+i);
    h->start=h->count=0;
}
/* A taller viewport reintroduces lines from the terminal's internal buffer. */
void history_resize_top(int delta) {
    int i;struct history *h=buf->history;
    if(delta>0){for(i=buf->top_line-delta;i<buf->top_line;i++)if(i>=0 && i<TEXT_BUFFER_SIZE)history_append(buf->text[i],cols);}
    else if(h){while(delta++<0 && h->count){int slot=(h->start+h->count-1)%HISTORY_LIMIT;release_line(h->ring+slot);h->count--;}}
}
void history_destroy(buf_t *owner) {history_clear(owner);free(owner->history);owner->history=NULL;}
void history_append(struct screenchar *line,int width) {
    struct history_line next={0};struct history *h=buf->history;int slot;
    if(!h){h=calloc(1,sizeof(*h));if(!h)return;buf->history=h;}
    if(!copy_line(&next,line,width))return;
    slot=(h->start+h->count)%HISTORY_LIMIT;
    if(h->count==HISTORY_LIMIT){release_line(h->ring+slot);h->start=(h->start+1)%HISTORY_LIMIT;}
    else h->count++;
    h->ring[slot]=next;
}
int history_active(void){return snapshot!=NULL;}
int history_count(void){return buf->history?buf->history->count:0;}
int history_position(void){return position;}
int history_total(void){return total;}
void history_move(int older) {
    int i;struct history *h=buf->history;
    if(!snapshot){
        if(older<=0 || !h || !h->count)return;
        total=h->count+rows;snapshot=calloc(total,sizeof(*snapshot));
        if(!snapshot){total=0;return;}
        for(i=0;i<h->count;i++){
            struct history_line *l=h->ring+(h->start+i)%HISTORY_LIMIT;
            if(!copy_line(snapshot+i,l->cells,l->width)){history_live();return;}
        }
        for(i=0;i<rows;i++){
            int index=buf->top_line+i;
            if(index<TEXT_BUFFER_SIZE && !copy_line(snapshot+h->count+i,buf->text[index],cols)){history_live();return;}
        }
        position=h->count;
    }
    position-=older;if(position<0)position=0;
    if(position>=total-rows){history_live();return;}
    history_invalidate();
}
struct screenchar *history_cell(int row,int col) {
    int i,j;if(!snapshot)return NULL;
    if(!view || view_rows!=rows || view_cols!=cols || view_position!=position){
        history_invalidate();view_rows=rows;view_cols=cols;view_position=position;
        view=calloc(rows*cols,sizeof(*view));if(!view){history_live();return NULL;}
        for(i=0;i<rows;i++)for(j=0;j<cols;j++){
            struct screenchar *c=view+i*cols+j;
            struct history_line *l=position+i<total?snapshot+position+i:NULL;
            c->c=' ';c->style=default_text_style;
            if(l && j<l->width)*c=l->cells[j];
            /* A resize can clip the right half of a Chinese glyph. */
            if(j==cols-1 && buf_char_width(c->c)==2)c->c=' ';
        }
    }
    return view+row*cols+col;
}
