/* Term49 offline Pinyin adapter. Apache-2.0. */
#include "ime_engine.h"
#include "libgooglepinyin-0.1.2/include/pinyinime.h"
#include <string.h>
using namespace ime_pinyin;
static int ready_,enabled_,selected_;
static size_t count_,page_;
static char spelling_[IME_MAX_INPUT+1];
static const char *notice_="";
static ime_commit_fn commit_;
static void clear_search() {
    spelling_[0]=0; count_=page_=0; selected_=0; notice_="";
    if(ready_) im_reset_search();
}
int ime_init(const char *dictionary,const char *user_dictionary,ime_commit_fn cb) {
    commit_=cb; ready_=im_open_decoder(dictionary,user_dictionary)?1:0;
    if(ready_) im_set_max_lens(IME_MAX_INPUT,32);
    enabled_=0; clear_search();return ready_;
}
void ime_close() { if(ready_) { im_flush_cache(); im_close_decoder(); } ready_=enabled_=0; }
int ime_ready(){return ready_;}
int ime_enabled(){return enabled_;}
void ime_cancel(){clear_search();}
static void raw_commit(){
    uint16_t s[IME_MAX_INPUT+1];size_t n=strlen(spelling_);
    for(size_t i=0;i<n;i++)s[i]=(unsigned char)spelling_[i];
    if(n && commit_)commit_(s,n);
    clear_search();
}
void ime_toggle(){if(!ready_)return;if(enabled_ && spelling_[0])raw_commit();enabled_=!enabled_;}
const char *ime_spelling(){return spelling_;}
size_t ime_candidate_count(){return count_;}
size_t ime_page_number(){return page_/IME_PAGE;}
int ime_selected(){return selected_;}
const char *ime_notice(){return notice_;}
int ime_candidate(int i,uint16_t *out,size_t cap){
    if(!cap)return 0;out[0]=0;
    if(!ready_||i<0||i>=IME_PAGE||page_+i>=count_)return 0;
    return im_get_candidate(page_+i,out,cap)!=0;
}
int ime_fixed(uint16_t *out,size_t cap){
    if(!cap)return 0;out[0]=0;
    if(!ready_||!count_)return 0;
    uint16_t tmp[IME_MAX_TEXT]={0};if(!im_get_candidate(0,tmp,IME_MAX_TEXT))return 0;
    size_t n=im_get_fixed_len();if(n>=cap)n=cap-1;
    memcpy(out,tmp,n*sizeof(uint16_t));out[n]=0;return (int)n;
}
static int choose(size_t index){
    if(index>=count_)return 0;
    size_t decoded=0;im_get_sps_str(&decoded);
    if(decoded<strlen(spelling_)){notice_="Incomplete spelling: Enter keeps raw text";return 0;}
    uint16_t out[IME_MAX_TEXT]={0};
    if(!im_get_candidate(index,out,IME_MAX_TEXT))return 0;
    size_t n=0;while(n<IME_MAX_TEXT && out[n])n++;
    const uint16 *starts=0;size_t syllables=im_get_spl_start_pos(starts);
    if(!n||!starts||n>syllables)return 0;
    size_t consumed=starts[n],len=strlen(spelling_);
    if(!consumed||consumed>len)return 0;
    if(commit_)commit_(out,n);
    // Commit only the chosen prefix, then decode remaining syllables afresh.
    // Do not reuse the upstream decoder's fragile fixed-prefix editing state.
    memmove(spelling_,spelling_+consumed,len-consumed+1);
    while(spelling_[0]=='\'')memmove(spelling_,spelling_+1,strlen(spelling_));
    im_reset_search();count_=*spelling_?im_search(spelling_,strlen(spelling_)):0;
    page_=0;selected_=0;notice_="";
    return !*spelling_;
}
int ime_choose_visible(int i){if(i<0||i>=IME_PAGE)return 0;return choose(page_+i);}
void ime_page(int d){
    if(d<0 && page_>=IME_PAGE)page_-=IME_PAGE;
    if(d>0 && page_+IME_PAGE<count_)page_+=IME_PAGE;
    selected_=0;
}
int ime_key(int key,int shortcut_modifier){
    if(!ready_||!enabled_)return 0;
    if(shortcut_modifier){clear_search();return 0;}
    size_t n=strlen(spelling_);
    if((key>='a'&&key<='z')||(key=='\''&&n)){
        if(n>=IME_MAX_INPUT){notice_="Input limit: select or edit first";return 1;}
        spelling_[n]=(char)key;spelling_[n+1]=0;
        im_reset_search();count_=im_search(spelling_,n+1);page_=0;selected_=0;notice_="";return 1;
    }
    if(!n)return 0;
    if(key==IME_BACKSPACE){
        spelling_[n-1]=0;im_reset_search();count_=n>1?im_search(spelling_,n-1):0;
        page_=0;selected_=0;notice_="";return 1;
    }
    if(key==IME_ESCAPE){clear_search();return 1;}
    if(key==IME_ENTER){raw_commit();return 1;}
    if(key==IME_LEFT){ime_page(-1);return 1;}
    if(key==IME_RIGHT||key==IME_TAB){ime_page(1);return 1;}
    if(key==IME_DOWN){
        if(selected_+1<IME_PAGE && page_+selected_+1<count_)selected_++;
        else ime_page(1);return 1;
    }
    if(key==IME_UP){if(selected_>0)selected_--;else ime_page(-1);return 1;}
    if(key==' '){if(count_)choose(page_+selected_);else raw_commit();return 1;}
    if(key>='1' && key<='0'+IME_PAGE){ime_choose_visible(key-'1');return 1;}
    // Punctuation follows a fully committed sentence; never forwards an Enter.
    if(key==','||key=='.'||key=='?'||key=='!'||key==';'||key==':'){
        if(!count_){raw_commit();return 0;}
        if(choose(0)){
            uint16_t c=key==','?0xff0c:key=='.'?0x3002:key=='?'?0xff1f:key=='!'?0xff01:key==';'?0xff1b:0xff1a;
            if(commit_)commit_(&c,1);
        }
        return 1;
    }
    // Preserve ordering on uppercase, symbols, or other non-composition keys.
    raw_commit();return 0;
}
