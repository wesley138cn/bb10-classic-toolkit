from __future__ import print_function
import os,zipfile,hashlib,json
root='/accounts/1000/shared/misc/pi-classic/package'
if os.path.exists(root):raise RuntimeError('Package destination exists')
os.makedirs(root)
with zipfile.ZipFile('/accounts/1000/shared/documents/pi-package.zip') as z:
 for item in z.infolist():
  if item.filename.startswith('/') or '..' in item.filename.split('/'):raise RuntimeError('Unsafe archive path')
 z.extractall(root)
print(json.dumps({'installed':root,'version':'0.74.2'}))
