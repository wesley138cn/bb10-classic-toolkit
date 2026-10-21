from __future__ import print_function
import os,shutil,json,hashlib
root='/accounts/1000/shared/misc/pi-classic';entry='/accounts/1000/shared/misc/berrycore/bin/pi';project='/accounts/1000/shared/documents/pi-projects/my-project'
if os.path.exists(entry):
 with open(entry,'rb') as a,open(root+'/pi-launch.sh','rb') as b:assert a.read()==b.read(),'Unrelated pi entry exists'
if not os.path.isdir(project):os.makedirs(project)
shutil.copyfile(root+'/pi-launch.sh',entry);os.chmod(entry,0o755)
with open(entry,'rb') as f:digest=hashlib.sha256(f.read()).hexdigest()
print(json.dumps({'entry':entry,'sha256':digest,'default_project':project}))
