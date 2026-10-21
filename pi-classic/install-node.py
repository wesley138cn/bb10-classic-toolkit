from __future__ import print_function
import os,zipfile,hashlib,json,subprocess
root='/accounts/1000/shared/misc/pi-classic';dest=root+'/node';archive='/accounts/1000/shared/documents/pi-node-term49.zip'
with open(archive,'rb') as f:assert hashlib.sha256(f.read()).hexdigest()=='6b6ca11c70eaf110ec1d305fda762979d48702a581328455404c7444279d4744'
if os.path.exists(dest):raise RuntimeError('Node destination exists; inspect before replacing')
os.makedirs(dest)
with zipfile.ZipFile(archive) as z:
 for item in z.infolist():
  if item.filename.startswith('/') or '..' in item.filename.split('/'):raise RuntimeError('Unsafe archive path')
 z.extractall(dest)
for n in ('node','node.bin','npm'):
 os.chmod(dest+'/bin/'+n,0o755)
env=os.environ.copy();env['LD_LIBRARY_PATH']=dest+'/bin:/base/lib:/base/usr/lib'
p=subprocess.Popen([dest+'/bin/node.bin','--jitless','-e','console.log(JSON.stringify({version:process.version,platform:process.platform,arch:process.arch,wasm:typeof WebAssembly,icu:process.versions.icu,home:process.env.HOME}))'],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
print(json.dumps({'code':p.wait(),'output':p.stdout.read().decode('utf8','replace'),'root':root}))
