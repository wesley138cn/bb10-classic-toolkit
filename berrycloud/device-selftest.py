# -*- coding: utf-8 -*-
from __future__ import print_function
import os,sys,json,tempfile,shutil,subprocess,io
sys.path.insert(0,'/accounts/1000/shared/misc/berrycloud')
import berrycloud as b
root=tempfile.mkdtemp(prefix='berrycloud-test-',dir=os.path.expanduser('~'))
results=[]
def record(name,ok):
 if not ok:raise AssertionError(name)
 results.append({'test':name,'passed':True})
try:
 cfg=b.default_config();cfg['api_key']='TEST-ONLY-NOT-A-REAL-KEY'
 b.save_json(root+'/config.json',cfg)
 fake='''from __future__ import print_function
import json,sys,os
args=sys.argv[1:]; conf=sys.stdin.read()
assert 'TEST-ONLY-NOT-A-REAL-KEY' not in ' '.join(args)
assert 'Authorization: Bearer TEST-ONLY-NOT-A-REAL-KEY' in conf
with open(args[args.index('--dump-header')+1],'w') as f:f.write('HTTP/1.1 200 OK\\r\\n\\r\\n')
with open(args[args.index('--data-binary')+1][1:],'r') as f:d=json.load(f)
assert d['thinking']['type']=='enabled' and d['reasoning_effort']=='low'
for text in ['你好','，黑莓。']:
 print('data: '+json.dumps({'choices':[{'index':0,'delta':{'content':text}}]}));print('');sys.stdout.flush()
print('data: '+json.dumps({'choices':[{'index':0,'delta':{},'finish_reason':'stop'}]}));print('')
print('data: [DONE]');print('')
'''
 with io.open(root+'/fake.py','w',encoding='utf-8') as f:f.write('# -*- coding: utf-8 -*-\n'+fake)
 with open(root+'/fake-curl','w') as f:f.write('#!/bin/sh\nexport LD_LIBRARY_PATH=/base/lib:/base/usr/lib\nexec /base/usr/bin/python3.2 '+root+'/fake.py "$@"\n')
 os.chmod(root+'/fake-curl',0o700)
 seen=[];r=b.request(cfg,[{'role':'user','content':'中文与引号 " $()'}],seen.append,root,root+'/fake-curl')
 record('Python 3.2 subprocess + UTF-8 SSE parser',r['content']=='你好，黑莓。' and len(seen)==2)
 record('Request temporary files removed',not [n for n in os.listdir(root) if n.startswith('.request-')])
 record('Private config permissions',os.stat(root+'/config.json').st_mode & 0o777==0o600)
 c=b.Chat(root,cfg)
 original=b.request
 b.request=lambda *args,**kw: {'content':'模拟回答','seconds':0.1,'finish_reason':'stop'}
 c.ask('记住测试编号 42');sid=c.session['id'];c.ask('编号是多少？')
 c.new();c.load(sid);record('Save and reload two-turn history',len(c.session['messages'])==4)
 c.command('/export');record('Export saved conversation',os.path.isfile(root+'/exports/'+sid+'.txt'))
 def fail(*args,**kw):raise b.CloudError('模拟断线')
 b.request=fail
 try:c.ask('重试问题')
 except b.CloudError:pass
 record('Failed answer does not enter history',len(c.session['messages'])==4 and c.pending=='重试问题')
 b.request=original
 env=os.environ.copy();env['BERRYCLOUD_HOME']=root;env['BERRYCLOUD_CURL']=root+'/fake-curl';env['LD_LIBRARY_PATH']='/base/lib:/base/usr/lib'
 p=subprocess.Popen(['/accounts/1000/shared/misc/berrycore/bin/ai','cloud'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
 out=p.communicate('/status\n你好\n/history\n/new\n/sessions\n/exit\n'.encode('utf-8'))[0].decode('utf-8')
 record('Installed ai cloud interactive command dispatch',p.returncode==0 and '你好，黑莓。' in out and '已退出' in out)
 record('No credential in terminal output',cfg['api_key'] not in out)
 p=subprocess.Popen(['/accounts/1000/shared/misc/berrycore/bin/ai','cloud','--resume'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
 out=p.communicate('/history\n/exit\n'.encode('utf-8'))[0].decode('utf-8')
 record('Exit and fresh-process resume',p.returncode==0 and '已恢复' in out and '你好，黑莓。' in out)
 report={'mode':'PHONE SIMULATED RESPONSES; NOT REAL CLOUD ANSWERS','python':sys.version,'tests':results}
 print(json.dumps(report,ensure_ascii=True,indent=2))
 with open('/accounts/1000/shared/documents/berrycloud-install/selftest-result.json','w') as f:json.dump(report,f,indent=2)
finally:shutil.rmtree(root)
