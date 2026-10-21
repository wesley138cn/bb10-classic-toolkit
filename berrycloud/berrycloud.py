#!/base/usr/bin/python3.2
# -*- coding: utf-8 -*-
"""BerryCloud: plain-terminal HTTPS chat for BB10 / Python 3.2+."""
from __future__ import print_function
import argparse
import getpass
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from urllib.parse import urlsplit

VERSION = '1.0.0'
CORE = '/accounts/1000/shared/misc/berrycore'
SYSTEM = '你是用户的中文助手。回答自然、简洁、准确；不确定的内容请说明。'
HELP = '''直接输入问题，回车发送。会话自动保存在手机。
/new                 新对话（旧会话保留）
/sessions            列出已保存会话
/load 会话编号        恢复会话（编号可用前几位）
/history             查看本次完整对话
/retry               重试刚才失败的问题，不自动重复扣费
/multi               多行输入，单独一行 . 发送
/model 模型名         切换模型并新建会话
/think low|high|max   调整推理程度
/stream on|off       切换逐步显示或整段显示
/tokens 数字          设置回答预算（含思考，256–16384）
/export              导出当前会话为文本
/status              查看模型、连接地址和配置路径（不显示密钥）
/help                帮助
/exit                退出；下次 ai cloud --resume 可继续
Ctrl+C 取消本次生成或输入，Ctrl+D 退出。'''

class CloudError(Exception):
    pass

def clean(text):
    return ''.join(c for c in str(text) if c in '\n\t' or (ord(c) >= 32 and not 127 <= ord(c) <= 159))

def say(text='', end='\n'):
    sys.stdout.write(clean(text) + end)
    sys.stdout.flush()

def private_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path, 0o700)
    os.chmod(path, 0o700)

def save_json(path, value):
    private_dir(os.path.dirname(path))
    fd, temp = tempfile.mkstemp(prefix='.write-', dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, 'w') as f:
            json.dump(value, f, ensure_ascii=True, indent=2)
            f.flush()
            os.fsync(f.fileno())
        os.rename(temp, path)
    finally:
        if os.path.exists(temp):
            os.remove(temp)

def read_json(path):
    with open(path, 'r') as f:
        return json.load(f)

def endpoint(value):
    parts = urlsplit(value)
    if parts.scheme != 'https' or not parts.hostname or parts.username or parts.password or parts.query or parts.fragment or any(ord(c) < 33 for c in value):
        raise CloudError('接口必须是 HTTPS 地址，不能包含账号、查询参数或控制字符。')
    return value.rstrip('/') if value.rstrip('/').endswith('/chat/completions') else value.rstrip('/') + '/chat/completions'

def curl_quote(s):
    if '\r' in s or '\n' in s:
        raise CloudError('配置含非法换行。')
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def validate_config(cfg):
    cfg['endpoint'] = endpoint(cfg.get('endpoint', ''))
    if not isinstance(cfg.get('api_key'), str) or not cfg['api_key'].strip():
        raise CloudError('尚未配置 API 密钥，请运行 ai cloud setup。')
    if any(ord(c) < 33 or ord(c) > 126 for c in cfg['api_key']):
        raise CloudError('API 密钥格式不正确。')
    if not re.match(r'^[a-zA-Z0-9_.:/-]{1,150}$', cfg.get('model', '')):
        raise CloudError('模型名称不正确。')
    if cfg.get('reasoning_effort', 'low') not in ('low', 'high', 'max'):
        raise CloudError('推理程度必须为 low、high 或 max。')
    if not 256 <= int(cfg.get('max_tokens',4096)) <= 16384:
        raise CloudError('回答预算须在 256–16384 之间。')
    return cfg

def default_config():
    return {'endpoint':'https://open.bigmodel.cn/api/coding/paas/v4/chat/completions',
            'model':'glm-5.3-flash', 'api_key':'', 'stream':True,
            'max_tokens':4096, 'reasoning_effort':'low'}

def error_message(obj, key):
    err = obj.get('error', {}) if isinstance(obj, dict) else {}
    if not isinstance(err, dict):
        err = {'message':str(err)}
    text = 'API 错误 %s：%s' % (err.get('code',''), err.get('message','服务未返回正常回答'))
    return clean(text.replace(key,'[REDACTED]'))[:600]

class Events:
    def __init__(self, emit, key):
        self.emit = emit
        self.key = key
        self.text = []
        self.done = False
        self.finish = None
        self.usage = {}
        self.reasoning_seen = False
        self.response_model = None
    def accept(self, obj):
        if not isinstance(obj, dict):
            raise CloudError('服务返回了不支持的响应格式。')
        if 'error' in obj:
            raise CloudError(error_message(obj,self.key))
        self.usage = obj.get('usage') or self.usage
        self.response_model = obj.get('model') or self.response_model
        for choice in obj.get('choices',[]):
            if choice.get('index',0) != 0:
                continue
            delta = choice.get('delta', choice.get('message',{}))
            self.reasoning_seen = self.reasoning_seen or bool(delta.get('reasoning_content'))
            content = delta.get('content') or ''
            if not isinstance(content,str):
                raise CloudError('服务未返回文本内容。')
            if content:
                self.text.append(content)
                self.emit(content)
            if choice.get('finish_reason'):
                self.finish = choice['finish_reason']
    def event(self, data):
        if data.strip() == '[DONE]':
            self.done = True
        elif data.strip():
            try:
                self.accept(json.loads(data))
            except (ValueError,TypeError,AttributeError):
                raise CloudError('流式响应格式错误；本次未存入会话。')
    def result(self, streaming):
        if streaming and not (self.done or self.finish):
            raise CloudError('连接提前结束，回答可能不完整；本次未存入会话，可 /retry。')
        text = ''.join(self.text)
        if not text.strip():
            hint = '思考消耗了输出预算，请用 /tokens 8192 后 /retry。' if self.reasoning_seen else '请稍后 /retry。'
            raise CloudError('没有收到正文。' + hint)
        return {'content':text,'finish_reason':self.finish,'usage':self.usage,'model':self.response_model}

def request(cfg, messages, emit, root, curl=None):
    validate_config(cfg)
    curl = curl or os.environ.get('BERRYCLOUD_CURL') or (CORE+'/bin/curl' if os.path.isfile(CORE+'/bin/curl') else 'curl')
    payload = {'model':cfg['model'], 'messages':messages, 'stream':bool(cfg.get('stream',True)), 'max_tokens':int(cfg.get('max_tokens',4096))}
    if cfg['model'].lower().startswith('glm-'):
        payload['thinking'] = {'type':'enabled'}
        payload['reasoning_effort'] = cfg.get('reasoning_effort','low')
    private_dir(root)
    work = tempfile.mkdtemp(prefix='.request-',dir=root)
    proc = None
    started = time.time()
    first = [None]
    def show(t):
        if first[0] is None:
            first[0] = round(time.time()-started,3)
        emit(t)
    parser = Events(show,cfg['api_key'])
    try:
        body = os.path.join(work,'body.json')
        save_json(body,payload)
        headers = os.path.join(work,'headers')
        errpath = os.path.join(work,'stderr')
        command = [curl,'--disable','--silent','--show-error','--no-buffer','--connect-timeout','20','--max-time','180',
                   '--proto','=https','--request','POST','--dump-header',headers,'--data-binary','@'+body,'--config','-']
        config = 'url = %s\nheader = %s\nheader = "Content-Type: application/json"\n' % (curl_quote(cfg['endpoint']),curl_quote('Authorization: Bearer '+cfg['api_key']))
        env = os.environ.copy()
        if os.path.isdir(CORE):
            env['LD_LIBRARY_PATH'] = CORE+'/lib:/base/lib:/base/usr/lib'
        # Secret supplied on stdin, never as a command argument or in diagnostics.
        with open(errpath,'wb') as err:
            proc = subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=err,env=env)
            proc.stdin.write(config.encode('utf-8'));proc.stdin.close()
            normal=[]; event=[]; size=0
            while True:
                raw=proc.stdout.readline(1048577)
                if not raw:
                    break
                size += len(raw)
                if len(raw)>1048576 or size>4*1048576:
                    raise CloudError('响应超过客户端限制，已停止。')
                line=raw.decode('utf-8','replace').rstrip('\r\n')
                if line.startswith('data:'):
                    event.append(line[5:].lstrip(' '))
                elif not line and event:
                    parser.event('\n'.join(event));event=[]
                elif not line or line.startswith(':') or line.startswith(('event:','id:','retry:')):
                    continue
                else:
                    normal.append(line)
            if event:
                parser.event('\n'.join(event))
            code=proc.wait()
        if code:
            labels={6:'域名解析失败，请检查手机网络。',7:'连接失败，请检查手机网络。',28:'请求超时，请稍后 /retry。',60:'HTTPS 证书校验失败，请检查手机日期与 CA 证书。',35:'TLS 握手失败，请检查网络和系统时间。'}
            raise CloudError(labels.get(code,'网络请求失败（curl %s），可 /retry。'%code))
        with open(headers,'r') as f:
            statuses=re.findall(r'^HTTP/\S+\s+(\d+)',f.read(),re.M)
        status=int(statuses[-1]) if statuses else 0
        if normal:
            try:
                obj=json.loads('\n'.join(normal))
            except ValueError:
                raise CloudError('HTTP %s：服务返回了非 JSON 内容。'%status)
            parser.accept(obj)
        if status!=200:
            raise CloudError('HTTP %s：请求失败，请检查密钥、余额、模型权限或网络。'%status)
        result=parser.result(bool(payload['stream']) and not normal)
        result.update({'seconds':round(time.time()-started,3),'first_text_seconds':first[0],'http_status':status})
        return result
    finally:
        if proc is not None:
            if proc.poll() is None:
                proc.kill();proc.wait()
            if proc.stdout:
                proc.stdout.close()
            if proc.stdin and not proc.stdin.closed:
                proc.stdin.close()
        shutil.rmtree(work)

class Chat:
    def __init__(self, root, cfg):
        self.root=root;self.cfg=cfg;self.pending=None
        private_dir(os.path.join(root,'sessions'))
        self.new()
    def new(self):
        self.session={'id':uuid.uuid4().hex[:12],'model':self.cfg['model'],'endpoint':self.cfg['endpoint'],'messages':[],'title':'新对话'}
        self.pending=None
    def save(self):
        self.session['updated']=time.time()
        save_json(os.path.join(self.root,'sessions',self.session['id']+'.json'),self.session)
        save_json(os.path.join(self.root,'last.json'),{'id':self.session['id']})
    def load(self, name):
        if not re.match(r'^[a-f0-9]{1,12}$',name):
            raise CloudError('会话编号应为列表中的十六进制编号。')
        names=[n for n in os.listdir(os.path.join(self.root,'sessions')) if n.startswith(name) and n.endswith('.json')]
        if len(names)!=1:
            raise CloudError('会话不存在或编号不唯一。')
        data=read_json(os.path.join(self.root,'sessions',names[0]))
        if data.get('endpoint')!=self.cfg['endpoint']:
            raise CloudError('会话来自其他 API 地址，请先切回对应服务。')
        for i,m in enumerate(data['messages']):
            if m.get('role')!=('user' if i%2==0 else 'assistant') or not isinstance(m.get('content'),str):
                raise CloudError('会话文件格式错误。')
        if len(data['messages'])%2:
            raise CloudError('会话文件不完整。')
        candidate=dict(self.cfg);candidate['model']=data['model'];validate_config(candidate)
        self.cfg=candidate;self.session=data;self.pending=None
        say('已恢复：'+data['id']+' · '+data['title'])
    def ask(self, question):
        if not question.strip():
            return
        if len(question)>12000:
            raise CloudError('单次输入最多 12000 字。')
        self.pending=question
        history=self.session['messages'][-40:]
        removed=len(self.session['messages'])-len(history)
        while history and sum(len(m['content']) for m in history)+len(question)>24000:
            history=history[2:];removed+=2
        if removed:
            say('提示：此次发送最近的 %s 轮；完整记录仍保存在本机。'%(len(history)//2))
        say('\nAI（'+self.cfg['model']+'，等待回复；Ctrl+C 可取消）\n',end='')
        result=request(self.cfg,[{'role':'system','content':SYSTEM}]+history+[{'role':'user','content':question}],lambda s:say(s,end=''),self.root)
        self.session['messages'].extend([{'role':'user','content':question},{'role':'assistant','content':result['content']}])
        if len(self.session['messages'])==2:
            self.session['title']=clean(question).replace('\n',' ')[:40]
        self.session['model']=self.cfg['model'];self.session['last_result']={k:v for k,v in result.items() if k!='content'}
        self.pending=None
        try:
            self.save()
        except (IOError,OSError) as e:
            say('\n回答已生成，但保存失败：'+str(e)+'；当前进程内仍保留对话。')
        else:
            say('\n[%s 秒 · 已保存 %s]'%(result['seconds'],self.session['id']))
        if result['finish_reason']=='length':
            say('回答达到输出预算而截断；可提高 /tokens 或要求继续。')
    def status(self):
        say('BerryCloud '+VERSION+'\n模型：'+self.cfg['model']+'\n接口：'+self.cfg['endpoint'])
        say('推理：%s · 流式：%s · 输出预算：%s'%(self.cfg.get('reasoning_effort','low'),self.cfg.get('stream',True),self.cfg.get('max_tokens',4096)))
        say('密钥：已配置（不显示）\n数据目录：'+self.root)
        if time.localtime().tm_year<2026:
            say('注意：手机日期不正确，HTTPS 验证可能失败，请在系统设置中校正。')
    def command(self, text):
        parts=text.split(None,1);cmd=parts[0];arg=parts[1].strip() if len(parts)>1 else ''
        if cmd in ('/exit','/quit'):
            return False
        if cmd=='/help':say(HELP)
        elif cmd=='/new':self.new();say('新会话：'+self.session['id'])
        elif cmd=='/status':self.status()
        elif cmd=='/sessions':
            rows=[]
            for n in os.listdir(os.path.join(self.root,'sessions')):
                if n.endswith('.json'):
                    d=read_json(os.path.join(self.root,'sessions',n));rows.append(d)
            for d in sorted(rows,key=lambda x:x.get('updated',0),reverse=True)[:50]:
                say('%s  %s轮  %s'%(d['id'],len(d['messages'])//2,d['title']))
            if not rows:say('尚无已保存会话。')
        elif cmd=='/load':self.load(arg)
        elif cmd=='/history':
            for m in self.session['messages']:say(('你：' if m['role']=='user' else 'AI：')+m['content']+'\n')
        elif cmd=='/retry':
            if not self.pending:raise CloudError('没有待重试的问题。')
            self.ask(self.pending)
        elif cmd=='/multi':
            say('逐行输入；单独一行 . 发送，Ctrl+C 取消。');lines=[]
            while True:
                line=input('... ')
                if line=='.':break
                lines.append(line)
                if sum(len(x)+1 for x in lines)>12000:raise CloudError('单次输入最多 12000 字。')
            self.ask('\n'.join(lines))
        elif cmd in ('/model','/think','/stream','/tokens'):
            candidate=dict(self.cfg)
            if cmd=='/model':candidate['model']=arg
            elif cmd=='/think':candidate['reasoning_effort']=arg
            elif cmd=='/stream':
                if arg not in ('on','off'):raise CloudError('用法：/stream on 或 /stream off')
                candidate['stream']=arg=='on'
            else:candidate['max_tokens']=int(arg)
            validate_config(candidate);save_json(os.path.join(self.root,'config.json'),candidate);self.cfg=candidate
            if cmd=='/model':self.new()
            self.status()
        elif cmd=='/export':
            folder=os.path.join(self.root,'exports');private_dir(folder)
            path=os.path.join(folder,self.session['id']+'.txt')
            with io.open(path,'w',encoding='utf-8') as f:
                for m in self.session['messages']:f.write(('你：' if m['role']=='user' else 'AI：')+clean(m['content'])+'\n\n')
            os.chmod(path,0o600);say('已导出：'+path)
        else:raise CloudError('未知命令；输入 /help 查看帮助。')
        return True

def setup(root):
    path=os.path.join(root,'config.json')
    cfg=read_json(path) if os.path.isfile(path) else default_config()
    say('配置云端 API；回车保留当前值。密钥不会显示。')
    cfg['endpoint']=input('接口 [%s]: '%cfg['endpoint']).strip() or cfg['endpoint']
    cfg['model']=input('模型 [%s]: '%cfg['model']).strip() or cfg['model']
    if not sys.stdin.isatty():
        raise CloudError('密钥配置需要交互式终端。')
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter('error',getpass.GetPassWarning)
        try:key=getpass.getpass('API Key（隐藏输入，回车保留）: ').strip()
        except getpass.GetPassWarning:raise CloudError('当前终端无法隐藏输入，已取消。')
    cfg['api_key']=key or cfg.get('api_key','');validate_config(cfg);save_json(path,cfg);say('配置已保存。')

def main():
    p=argparse.ArgumentParser(description='BerryCloud 云端终端对话（BB10）')
    p.add_argument('words',nargs='*',help='setup / status / doctor，或直接提问')
    p.add_argument('--resume',nargs='?',const='last',help='恢复上次会话或指定编号')
    p.add_argument('--version',action='version',version=VERSION)
    a=p.parse_args()
    root=os.environ.get('BERRYCLOUD_HOME',os.path.join(os.path.expanduser('~'),'.berrycloud'))
    private_dir(root)
    if a.words==['setup']:setup(root);return 0
    path=os.path.join(root,'config.json')
    if not os.path.isfile(path):raise CloudError('尚未配置。请运行 ai cloud setup。')
    cfg=validate_config(read_json(path));chat=Chat(root,cfg)
    if a.words==['status']:chat.status();return 0
    if a.words==['doctor']:
        chat.status();chat.ask('只回复两个字：连通');return 0
    if a.resume:
        name=read_json(os.path.join(root,'last.json'))['id'] if a.resume=='last' else a.resume
        chat.load(name)
    if a.words:
        chat.ask(' '.join(a.words));return 0
    say('BerryCloud '+VERSION+' · '+cfg['model']+'\n直接输入问题。/help 帮助 · /exit 退出 · 会话自动保存')
    while True:
        try:
            line=input('\n你 > ').strip()
            if not line:continue
            if line.startswith('/'):
                if not chat.command(line):break
            else:chat.ask(line)
        except EOFError:break
        except KeyboardInterrupt:say('\n已取消。本次未存入会话，可 /retry。')
        except (CloudError,ValueError,IOError,OSError,KeyError) as e:
            say('\n'+str(e).replace(cfg['api_key'],'[REDACTED]'))
    say('已退出。用 ai cloud --resume 继续。');return 0

if __name__=='__main__':
    try:sys.exit(main())
    except KeyboardInterrupt:say('\n已取消。');sys.exit(130)
    except (CloudError,ValueError,IOError,OSError,KeyError) as e:
        say('错误：'+str(e));sys.exit(1)
