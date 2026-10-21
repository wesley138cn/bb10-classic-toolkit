// Convenience safeguards, NOT an operating-system sandbox.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
export default function (pi) {
 pi.on('before_agent_start', async event => ({systemPrompt:event.systemPrompt+'\nYou are running on a BlackBerry Classic with QNX, not Linux. Work only in the current project. Preserve existing user data and never read credential files. Python is 3.2; use LD_LIBRARY_PATH=/base/lib:/base/usr/lib /base/usr/bin/python3.2 for it. Ask before destructive changes or installing packages. Prefer concise Chinese replies.'}));
 pi.on('tool_call',async(event,ctx)=>{
  let reason;
  const input=event.input||{};
  if(['read','edit','write','ls','find','grep'].includes(event.toolName)) {
   const target=path.resolve(ctx.cwd,input.path||'.');
   if(/\/(?:\.berrycloud|\.pi)\//.test(target) && /(?:config|models|auth)\.json$/.test(target))return {block:true,reason:'Credential files are not available to AI tools.'};
   let physical=target;
   try{physical=fs.realpathSync(target);}catch{try{physical=path.join(fs.realpathSync(path.dirname(target)),path.basename(target));}catch{}}
   const cwd=fs.realpathSync(ctx.cwd);
   if(physical!==cwd&&!physical.startsWith(cwd+path.sep))reason='访问项目以外的路径：'+target;
  }
  if(event.toolName==='bash' && /(^|[\s;|&])(rm|slay|dd|mkfs|shutdown|reboot|qpkg|npm|sudo)(\s|$)|\.berrycloud|\.pi\/agent\/(models|auth)\.json|\/dev\//i.test(input.command||''))reason='执行需要核对的命令：\n'+input.command;
  if(reason && (!ctx.hasUI || !await ctx.ui.confirm('Classic 操作确认',reason)))return {block:true,reason:'This operation needs user confirmation in interactive mode.'};
  if(['write','edit'].includes(event.toolName)) {
   const target=path.resolve(ctx.cwd,input.path);
   if(fs.existsSync(target)&&fs.statSync(target).isFile()){
    const dir=path.join(os.homedir(),'.pi','agent','classic-backups');fs.mkdirSync(dir,{recursive:true,mode:0o700});
    const stamp=Date.now()+'-'+Math.random().toString(16).slice(2);
    fs.copyFileSync(target,path.join(dir,stamp+'.bak'));fs.chmodSync(path.join(dir,stamp+'.bak'),0o600);
    fs.writeFileSync(path.join(dir,stamp+'.json'),JSON.stringify({original:target,date:new Date().toISOString()}),{mode:0o600});
   }
  }
 });
}
