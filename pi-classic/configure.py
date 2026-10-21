from __future__ import print_function
import os,json,shutil
home=os.path.expanduser('~');root=home+'/.pi/agent';app='/accounts/1000/shared/misc/pi-classic'
with open(home+'/.berrycloud/config.json') as f:cfg=json.load(f)
endpoint=cfg['endpoint'];assert endpoint.startswith('https://open.bigmodel.cn/') and endpoint.endswith('/chat/completions')
if not os.path.isdir(root):os.makedirs(root,0o700)
os.chmod(home+'/.pi',0o700);os.chmod(root,0o700)
def save(name,data):
 p=root+'/'+name
 if os.path.exists(p):raise RuntimeError('Existing Pi config: '+name)
 fd=os.open(p,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
 with os.fdopen(fd,'w') as f:json.dump(data,f,ensure_ascii=True,indent=2)
save('models.json',{'providers':{'berrycloud':{'baseUrl':endpoint[:-len('/chat/completions')],'api':'openai-completions','apiKey':cfg['api_key'],'models':[{'id':cfg['model'],'name':'GLM-5.3-Flash (Classic)','reasoning':True,'input':['text'],'contextWindow':32000,'maxTokens':8192,'cost':{'input':0,'output':0,'cacheRead':0,'cacheWrite':0},'compat':{'supportsStore':False,'supportsDeveloperRole':False,'supportsUsageInStreaming':False,'maxTokensField':'max_tokens','thinkingFormat':'deepseek','supportsReasoningEffort':True,'supportsStrictMode':False,'requiresReasoningContentOnAssistantMessages':True}}]}}})
save('settings.json',{'defaultProvider':'berrycloud','defaultModel':cfg['model'],'defaultThinkingLevel':'low','hideThinkingBlock':True,'shellPath':'/accounts/1000/shared/misc/berrycore/bin/bash','retry':{'enabled':False,'provider':{'maxRetries':0,'timeoutMs':180000}},'images':{'blockImages':True},'quietStartup':True})
print(json.dumps({'configured':True,'provider':'berrycloud','model':cfg['model'],'private_config':root,'key_copied_on_device_only':True}))
