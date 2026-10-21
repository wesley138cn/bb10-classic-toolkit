#include "ime_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
static std::string output;
static std::string utf8(const uint16_t *s,size_t n){std::string r;for(size_t i=0;i<n;i++){unsigned c=s[i];if(c<128)r+=(char)c;else if(c<2048){r+=(char)(0xc0|(c>>6));r+=(char)(0x80|(c&63));}else{r+=(char)(0xe0|(c>>12));r+=(char)(0x80|((c>>6)&63));r+=(char)(0x80|(c&63));}}return r;}
static void commit(const uint16_t *s,size_t n){output+=utf8(s,n);}
static void type(const char *s){for(;*s;s++)assert(ime_key(*s,0));}
static bool contains(const char *s){
 uint16_t out[IME_MAX_TEXT];
 for(int p=0;p<100;p++){
  for(int i=0;i<IME_PAGE;i++)if(ime_candidate(i,out,IME_MAX_TEXT)&&utf8(out,[] (const uint16_t *s){size_t n=0;while(s[n])n++;return n;}(out))==s)return true;
  size_t old=ime_page_number();ime_page(1);if(old==ime_page_number())break;
 }return false;
}
int main(int argc,char **argv){
 assert(argc==3);unlink(argv[2]);assert(ime_init(argv[1],argv[2],commit));assert(!ime_enabled());
 assert(!ime_key('n',0));ime_toggle();assert(ime_enabled());
 type("nihao");assert(contains("你好"));ime_cancel();type("nihao");assert(ime_key(' ',0));assert(output=="你好");assert(!*ime_spelling());
 output.clear();type("zhongwen");assert(contains("中文"));ime_cancel();type("zhongwen");ime_key(' ',0);assert(output=="中文");
 output.clear();type("nihao");ime_key('.',0);assert(output=="你好。");
 output.clear();type("nihao");assert(contains("你"));
 uint16_t cand[IME_MAX_TEXT];int chosen=0;
 for(int i=0;i<IME_PAGE;i++){if(ime_candidate(i,cand,IME_MAX_TEXT)&&cand[0]==0x4f60&&cand[1]==0){ime_choose_visible(i);chosen=1;break;}}
 assert(chosen);assert(output=="你");assert(!strcmp(ime_spelling(),"hao"));ime_key(' ',0);assert(output=="你好");
 output.clear();type("nihao");assert(!ime_key('c',1));assert(!*ime_spelling());assert(output.empty());
 output.clear();type("xian");assert(ime_candidate_count()>3);ime_page(1);assert(ime_page_number()==1);ime_page(-1);assert(ime_page_number()==0);
 ime_key(IME_DOWN,0);assert(ime_selected()==1);ime_key(IME_UP,0);assert(ime_selected()==0);ime_key(IME_ESCAPE,0);assert(!*ime_spelling());assert(output.empty());
 type("nihaox");ime_key(IME_BACKSPACE,0);assert(!strcmp(ime_spelling(),"nihao"));ime_key(IME_ENTER,0);assert(output=="nihao");assert(!ime_key(IME_ENTER,0));
 output.clear();type("zzzzzz");ime_key(IME_ENTER,0);assert(output=="zzzzzz");
 output.clear();type("nihao");ime_toggle();assert(output=="nihao");assert(!ime_enabled());ime_toggle();
 assert(!ime_key('c',1));assert(!*ime_spelling());
 type("xi'an");assert(contains("西安"));ime_cancel();
 for(int i=0;i<100;i++)ime_key('n',0);assert(strlen(ime_spelling())==IME_MAX_INPUT);ime_key(IME_ESCAPE,0);
 for(int i=0;i<1000;i++){type("zhongwen");ime_key(' ',0);type("nihao");ime_key(IME_ESCAPE,0);}

 // Vary edits, segmentation, selection and incomplete spellings under sanitizers.
 unsigned int rng=72421;
 const char alphabet[]="abcdefghijklmnopqrstuvwxyz''";
 for(int trial=0;trial<300;trial++) {
  ime_cancel();output.clear();
  for(int j=0;j<80;j++) {
   rng=rng*1664525u+1013904223u;
   unsigned int action=rng%40;
   if(action<28)ime_key(alphabet[action],0);
   else if(action==28)ime_key(IME_BACKSPACE,0);
   else if(action==29)ime_key(IME_ESCAPE,0);
   else if(action==30)ime_key(IME_ENTER,0);
   else if(action==31)ime_key(' ',0);
   else if(action<35)ime_key('1'+action-32,0);
   else if(action==35)ime_key(IME_LEFT,0);
   else if(action==36)ime_key(IME_RIGHT,0);
   else if(action==37)ime_key(IME_DOWN,0);
   else if(action==38)ime_key('.',0);
   else ime_key('A',0);
   assert(strlen(ime_spelling())<=IME_MAX_INPUT);
  }
 }
 ime_close();assert(!ime_ready());assert(ime_init(argv[1],argv[2],commit));ime_toggle();type("nihao");assert(contains("你好"));ime_close();
 puts("ENGINE_TEST_OK: candidate/phrase/apostrophe/paging/selection/delete/cancel/raw/shortcut/limit/1000 cycles/reopen");
 return 0;
}
