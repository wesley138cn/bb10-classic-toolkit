import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import guard from './classic-guard.js';
const handlers={};guard({on:(name,fn)=>handlers[name]=fn});
const root=fs.mkdtempSync(path.join(os.tmpdir(),'pi-classic-guard-'));
try{
 const ctx={cwd:root,hasUI:false};
 assert.equal((await handlers.tool_call({toolName:'read',input:{path:root+'/../outside.txt'}},ctx)).block,true);
 assert.equal((await handlers.tool_call({toolName:'bash',input:{command:'rm -rf notes'}},ctx)).block,true);
 assert.equal((await handlers.tool_call({toolName:'read',input:{path:os.homedir()+'/.pi/agent/models.json'}},ctx)).block,true);
 assert.equal(await handlers.tool_call({toolName:'bash',input:{command:'python calc.py 7 8'}},ctx),undefined);
 fs.symlinkSync(os.tmpdir(),root+'/escape');
 assert.equal((await handlers.tool_call({toolName:'write',input:{path:'escape/outside.txt'}},ctx)).block,true);
 assert.ok((await handlers.before_agent_start({systemPrompt:'base'})).systemPrompt.includes('QNX'));
 console.log('PASS: outside paths, symlink escape, credential files and destructive command confirmation; ordinary commands allowed');
}finally{fs.rmSync(root,{recursive:true,force:true});}
