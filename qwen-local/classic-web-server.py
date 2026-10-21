#!/base/usr/bin/python3.2
"""Local Qwen HTTP adapter for BB10's working single-thread completion runner."""
from __future__ import print_function
import argparse, json, os, signal, subprocess, threading, time, sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
ROOT='/accounts/1000/shared/misc/bcllm'
LOCK=threading.Lock()
ACTIVE=[None]
class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(fmt % args); sys.stdout.flush()
    def respond(self, code, obj, kind='application/json; charset=utf-8'):
        body=json.dumps(obj,ensure_ascii=False).encode('utf-8') if isinstance(obj,dict) else obj
        self.send_response(code)
        self.send_header('Content-Type',kind)
        self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store')
        self.end_headers()
        try:self.wfile.write(body)
        except (IOError,OSError):pass
    def do_GET(self):
        path=self.path.split('?')[0]
        if path=='/health':
            self.respond(200,{'status':'ok','backend':'classic-completion','model_loading':'on-demand','pid':os.getpid()})
        elif path in ('/','/index.html'):
            with open(ROOT+'/web/index.html','rb') as f:self.respond(200,f.read(),'text/html; charset=utf-8')
        else:self.respond(404,{'error':'Not found'})
    def do_POST(self):
        if self.path!='/v1/chat/completions':return self.respond(404,{'error':'Not found'})
        try:
            size=int(self.headers.get('Content-Length','0'))
            if size<1 or size>8192:raise ValueError('Request too large')
            data=json.loads(self.rfile.read(size).decode('utf-8'))
            messages=data.get('messages',[])
            users=[m.get('content','') for m in messages if m.get('role')=='user']
            if not users or not isinstance(users[-1],str) or not users[-1].strip():raise ValueError('Missing prompt')
            text=users[-1].strip()
            if len(text)>160:raise ValueError('Please keep the question within 160 characters')
            if data.get('stream'):raise ValueError('Streaming is not supported')
            n=max(1,min(int(data.get('max_tokens',32)),64))
            temp=max(0,min(float(data.get('temperature',0.2)),2))
        except (ValueError,TypeError,AttributeError) as e:return self.respond(400,{'error':str(e)})
        if not LOCK.acquire(False):return self.respond(429,{'error':'A question is already running'})
        timer=None
        try:
            prompt='<|im_start|>system\n请用简洁的中文回答。<|im_end|>\n<|im_start|>user\n'+text+'<|im_end|>\n<|im_start|>assistant\n'
            cmd=[ROOT+'/bin/llama-completion','-m',ROOT+'/models/qwen2.5-0.5b-instruct-q4_k_m.gguf','-t','1','-c','512','-b','512','-ub','16','-fa','off','-fit','off','--no-warmup','--simple-io','-st','-no-cnv','--reasoning','off','--temp',str(temp),'-n',str(n),'--no-display-prompt','-p',prompt]
            env=os.environ.copy();env['LD_LIBRARY_PATH']=ROOT+'/lib:'+env.get('LD_LIBRARY_PATH','')
            started=time.time();expired=[False]
            with open(ROOT+'/var/last-answer.txt','wb') as out, open(ROOT+'/var/last-inference.log','wb') as err:
                p=subprocess.Popen(cmd,env=env,stdin=open('/dev/null','rb'),stdout=out,stderr=err)
                ACTIVE[0]=p
                def stop():
                    expired[0]=True
                    try:p.kill()
                    except OSError:pass
                timer=threading.Timer(600,stop);timer.start();p.wait();timer.cancel();ACTIVE[0]=None
            if expired[0]:return self.respond(504,{'error':'Inference exceeded 10 minutes'})
            if p.returncode:return self.respond(500,{'error':'Runner failed; see bcllm/var/last-inference.log'})
            with open(ROOT+'/var/last-answer.txt','rb') as f:raw=f.read().decode('utf-8','replace')
            ended='[end of text]' in raw
            reply=raw.replace('[end of text]','').strip()
            if not reply:raise ValueError('Empty model response; see last-inference.log')
            response={'id':'classic-'+str(int(started)),'object':'chat.completion','model':'qwen2.5-0.5b-instruct-q4_k_m','choices':[{'index':0,'message':{'role':'assistant','content':reply},'finish_reason':'stop' if ended else 'length'}],'timings':{'total_seconds':round(time.time()-started,3)},'backend':'classic-completion'}
            with open(ROOT+'/var/last-response.json','w') as f:json.dump(response,f,ensure_ascii=True,indent=2)
            self.respond(200,response)
        except Exception as e:self.respond(500,{'error':str(e)})
        finally:
            if timer:timer.cancel()
            p=ACTIVE[0]
            if p is not None and p.poll() is None:
                try:p.kill()
                except OSError:pass
            ACTIVE[0]=None;LOCK.release()
class Server(ThreadingMixIn,HTTPServer):
    daemon_threads=True
    allow_reuse_address=True

def shutdown(signum,frame):
    p=ACTIVE[0]
    if p is not None:
        try:p.kill()
        except OSError:pass
    raise SystemExit(0)
signal.signal(signal.SIGTERM,shutdown)
signal.signal(signal.SIGINT,shutdown)
parser=argparse.ArgumentParser()
parser.add_argument('--port',type=int,default=8080)
args=parser.parse_args()
server=Server(('127.0.0.1',args.port),Handler)
with open(ROOT+'/var/web.pid','w') as f:f.write(str(os.getpid()))
print('Classic Qwen ready at http://127.0.0.1:%d/; model loads for each question.' % args.port)
sys.stdout.flush()
try:server.serve_forever()
finally:
    server.server_close()
    try:
        with open(ROOT+'/var/web.pid') as f:current=f.read().strip()
        if current==str(os.getpid()):os.remove(ROOT+'/var/web.pid')
    except (OSError,IOError):pass
