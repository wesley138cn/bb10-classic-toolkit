import contextlib
import importlib.util
import io
import json
import os
import pathlib
import tempfile
import unittest
from unittest import mock

spec=importlib.util.spec_from_file_location('bc',str(pathlib.Path(__file__).with_name('berrycloud.py')))
bc=importlib.util.module_from_spec(spec);spec.loader.exec_module(bc)

class CloudTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=self.tmp.name
  self.cfg=bc.default_config();self.cfg['api_key']='test-secret-only'
  self.fake=pathlib.Path(self.root)/'fakecurl'
  self.fake.write_text('''#!/usr/bin/env python3
import sys,json,pathlib,time,os
args=sys.argv[1:]; conf=sys.stdin.read()
assert 'test-secret-only' not in ' '.join(args)
assert 'Authorization: Bearer test-secret-only' in conf
assert '--disable' in args and '--proto' in args and '=https' in args
assert '-k' not in args and '--insecure' not in args
root=pathlib.Path(__file__).parent
payload=json.loads(pathlib.Path(args[args.index('--data-binary')+1][1:]).read_text())
(root/'payload.json').write_text(json.dumps(payload))
fixture=json.loads((root/'fixture.json').read_text())
pathlib.Path(args[args.index('--dump-header')+1]).write_text('HTTP/1.1 '+str(fixture['status'])+' OK\\r\\n\\r\\n')
for chunk in fixture['chunks']:
 sys.stdout.buffer.write(chunk.encode('utf-8'));sys.stdout.buffer.flush()
 time.sleep(fixture.get('delay',0))
sys.exit(fixture.get('code',0))
''');self.fake.chmod(0o700)
 def tearDown(self):self.tmp.cleanup()
 def fixture(self,chunks,status=200,code=0,delay=0):
  pathlib.Path(self.root,'fixture.json').write_text(json.dumps(dict(chunks=chunks,status=status,code=code,delay=delay)))
 def event(self,content=None,finish=None,**kw):
  obj={'choices':[{'index':0,'delta':{'content':content},'finish_reason':finish}]};obj.update(kw)
  return 'data: '+json.dumps(obj,ensure_ascii=False)+'\n\n'
 def call(self):
  seen=[];result=bc.request(self.cfg,[{'role':'user','content':'你好'}],seen.append,self.root,str(self.fake));return result,seen
 def test_stream_utf8_and_payload(self):
  self.fixture([self.event('你好'),': heartbeat\n\n',self.event('黑莓'),self.event(finish='stop',usage={'total_tokens':25}),'data: [DONE]\n\n'])
  r,seen=self.call();self.assertEqual(r['content'],'你好黑莓');self.assertEqual(seen,['你好','黑莓'])
  p=json.loads(pathlib.Path(self.root,'payload.json').read_text());self.assertEqual(p['reasoning_effort'],'low');self.assertEqual(p['thinking'],{'type':'enabled'})
  self.assertEqual(list(pathlib.Path(self.root).glob('.request-*')),[])
 def test_json_nonstream(self):
  self.cfg['stream']=False;self.fixture([json.dumps({'choices':[{'message':{'content':'完整回复'},'finish_reason':'stop'}]})])
  self.assertEqual(self.call()[0]['content'],'完整回复')
 def test_balance_error_redacted(self):
  self.fixture([json.dumps({'error':{'code':'1113','message':'余额不足 test-secret-only'}})],429)
  with self.assertRaises(bc.CloudError) as cm:self.call()
  self.assertIn('1113',str(cm.exception));self.assertNotIn(self.cfg['api_key'],str(cm.exception))
 def test_truncated_stream(self):
  self.fixture([self.event('部分')])
  with self.assertRaisesRegex(bc.CloudError,'提前结束'):self.call()
 def test_empty_reasoning_budget(self):
  self.fixture(['data: '+json.dumps({'choices':[{'delta':{'reasoning_content':'not shown'},'finish_reason':'length'}]})+'\n\n'])
  with self.assertRaisesRegex(bc.CloudError,'8192'):self.call()
 def test_tls_never_bypassed(self):
  self.fixture([],0,60)
  with self.assertRaisesRegex(bc.CloudError,'证书'):self.call()
 def test_server_sse_error(self):
  self.fixture([self.event('部分'),'data: {"error":{"code":"oops","message":"broken"}}\n\n'])
  with self.assertRaisesRegex(bc.CloudError,'oops'):self.call()
 def test_interrupt_cleans_request(self):
  self.fixture([self.event('start')])
  def stop(s):raise KeyboardInterrupt()
  with self.assertRaises(KeyboardInterrupt):bc.request(self.cfg,[],stop,self.root,str(self.fake))
  self.assertEqual(list(pathlib.Path(self.root).glob('.request-*')),[])
 def test_endpoint_security(self):
  for value in ('http://example.com','https://key@example.com','https://example.com?key=x','https://example.com\nheader=x'):
   with self.assertRaises(bc.CloudError):bc.endpoint(value)
  self.assertEqual(bc.endpoint('https://open.bigmodel.cn/api/paas/v4'),'https://open.bigmodel.cn/api/paas/v4/chat/completions')
 def test_sessions_resume_export_and_failure(self):
  c=bc.Chat(self.root,self.cfg)
  ok={'content':'回答','seconds':1,'finish_reason':'stop','usage':{}}
  with contextlib.redirect_stdout(io.StringIO()),mock.patch.object(bc,'request',return_value=ok):c.ask('问题')
  ident=c.session['id'];c.new()
  with contextlib.redirect_stdout(io.StringIO()):c.load(ident);c.command('/export')
  self.assertEqual(c.session['messages'][0]['content'],'问题')
  self.assertTrue(pathlib.Path(self.root,'exports',ident+'.txt').exists())
  with contextlib.redirect_stdout(io.StringIO()),mock.patch.object(bc,'request',side_effect=bc.CloudError('quota')):
   with self.assertRaises(bc.CloudError):c.ask('失败')
  self.assertEqual(len(c.session['messages']),2);self.assertEqual(c.pending,'失败')
  self.assertNotIn('api_key',pathlib.Path(self.root,'sessions',ident+'.json').read_text())
 def test_context_limits_keep_pairs(self):
  c=bc.Chat(self.root,self.cfg);c.session['messages']=[{'role':r,'content':'x'*2000} for i in range(30) for r in ('user','assistant')]
  ok={'content':'答','seconds':1,'finish_reason':'stop'}
  with contextlib.redirect_stdout(io.StringIO()),mock.patch.object(bc,'request',return_value=ok) as m:c.ask('新问')
  msgs=m.call_args[0][1];self.assertEqual(msgs[1]['role'],'user');self.assertLessEqual(sum(len(x['content']) for x in msgs[1:]),24000)
  self.assertEqual(len(c.session['messages']),62)
 def test_terminal_control_stripped(self):
  self.assertNotIn('\x1b',bc.clean('\x1b[2Jtext\x07'));self.assertEqual(bc.clean('中文\n换行'),'中文\n换行')
 def test_atomic_private_config(self):
  p=os.path.join(self.root,'config.json');bc.save_json(p,self.cfg)
  self.assertEqual(os.stat(p).st_mode & 0o777,0o600)
  self.assertEqual(bc.read_json(p),self.cfg)
 def test_model_validation(self):
  self.cfg['model']='x\nheader = malicious'
  with self.assertRaises(bc.CloudError):bc.validate_config(self.cfg)

if __name__=='__main__':unittest.main(verbosity=2)
