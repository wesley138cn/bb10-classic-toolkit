/* Exercise the actual terminal UTF-8 converter and PTY, without model calls. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <unix.h>
#include <termios.h>
#include <fcntl.h>
#include <assert.h>
#include "preferences.h"
#include "io.h"
#include "ime_engine.h"
static int calls;
static void send_text(const uint16_t *s,size_t n){
 while(n){size_t part=n>8?8:n;if(part<n&&s[part-1]>=0xd800&&s[part-1]<=0xdbff)--part;
 assert(io_write_master(s,part)>0);s+=part;n-=part;}calls++;
}
int main(int argc,char **argv){
 assert(argc==3);int master,slave;struct termios term;
 assert(openpty(&master,&slave,NULL,NULL,NULL)==0);
 assert(tcgetattr(slave,&term)==0);cfmakeraw(&term);assert(tcsetattr(slave,TCSANOW,&term)==0);
 fcntl(slave,F_SETFL,O_NONBLOCK);
 pref_t prefs;memset(&prefs,0,sizeof(prefs));prefs.tty_encoding="UTF-8";
 assert(io_init(&prefs)==0);io_set_master(master);assert(ime_init(argv[1],argv[2],send_text));ime_toggle();
 char buf[1024];const char *py="nihao";while(*py)assert(ime_key(*py++,0));
 assert(read(slave,buf,sizeof(buf))<0);assert(calls==0); // preedit stays local
 ime_key(' ',0);usleep(20000);int n=read(slave,buf,sizeof(buf));assert(n==6);assert(!memcmp(buf,"你好",6));
 assert(read(slave,buf,sizeof(buf))<0); // choosing did not submit/newline
 uint16_t long_text[32];for(int i=0;i<32;i++)long_text[i]=0x4e2d;
 send_text(long_text,32);usleep(20000);n=read(slave,buf,sizeof(buf));assert(n==96);
 for(int i=0;i<32;i++)assert(!memcmp(buf+3*i,"中",3));
 py="zhongwen";while(*py)ime_key(*py++,0);ime_key(IME_ESCAPE,0);assert(read(slave,buf,sizeof(buf))<0);
 ime_toggle();assert(!ime_key('x',0));
 ime_close();io_uninit();close(slave);
 puts("PTY_TEST_OK: preedit isolated; Chinese commit exact; no auto-submit; 32 Chinese characters intact; cancel/English pass-through");return 0;
}
