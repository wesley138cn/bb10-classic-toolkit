from __future__ import print_function
import os,shutil,json
home=os.path.expanduser('~');root='/accounts/1000/shared/misc/pi-classic';agent=home+'/.pi/agent'
ext=agent+'/extensions'
if not os.path.isdir(ext):os.makedirs(ext,0o700)
for src,dest in [(root+'/classic-guard.js',ext+'/classic-guard.js')]:
 if os.path.exists(dest):raise RuntimeError('Existing extension; inspect before replacing')
 shutil.copyfile(src,dest);os.chmod(dest,0o600)
project='/accounts/1000/shared/documents/pi-projects'
if not os.path.isdir(project):os.makedirs(project)
if not os.path.exists(project+'/AGENTS.md'):shutil.copyfile(root+'/AGENTS-phone.md',project+'/AGENTS.md')
if not os.path.isdir(project+'/agent-validation'):os.makedirs(project+'/agent-validation')
print(json.dumps({'extension':True,'project':project}))
