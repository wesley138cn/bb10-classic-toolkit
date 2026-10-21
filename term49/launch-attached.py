from __future__ import print_function
import os, re, subprocess, sys, time

root='/accounts/1000/shared/misc/term49-cjk'
for attempt in range(12):
 with open('/pps/services/navigator/state') as f: state=f.read()
 app=re.search(r'curr_window::([^\r\n]+)',state)
 pid=re.search(r'fullscreen::([0-9]+)',state)
 if app and app.group(1).startswith('com.example.Term49.') and pid and pid.group(1)!='0':break
 time.sleep(0.25)
if not app or not app.group(1).startswith('com.example.Term49.') or not pid or pid.group(1)=='0':
 print('Keep the original Term49 in the foreground, then run term-cjk again.')
 sys.exit(1)
env=os.environ.copy()
env['TERM49_PARENT_GROUP']='SDL-window-'+pid.group(1)
env['LD_LIBRARY_PATH']=root+'/lib:/base/lib:/base/usr/lib'
env.setdefault('WIDTH','720');env.setdefault('HEIGHT','720')
ime_root='/accounts/1000/shared/misc/term49-ime'
use_ime=os.path.isfile(ime_root+'/enabled') and os.path.isfile(ime_root+'/Term49-IME') and os.path.isfile(ime_root+'/dict_pinyin.dat')
binary=ime_root+'/Term49-IME' if use_ime else root+'/Term49-CJK'
env['TERM49_IME']='1' if use_ime else '0'
with open(root+'/terminal.log','w') as log:
 p=subprocess.Popen([binary],env=env,cwd=os.path.expanduser('~'),
                    stdout=log,stderr=log)
with open(root+'/terminal.pid','w') as f:f.write(str(p.pid))
if '--background' in sys.argv:
 print('Started pid='+str(p.pid)+' group='+env['TERM49_PARENT_GROUP'])
else:
 rc=p.wait()
 if rc:
  print('CJK terminal could not start (status '+str(rc)+').')
  with open(root+'/terminal.log') as f:
   for line in f:
    if 'CJK_ATTACH' in line or 'Cannot open' in line:print(line.rstrip())
 sys.exit(rc)
