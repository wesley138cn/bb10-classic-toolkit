require('node:fs').mkdirSync(require('node:path').join(__dirname,'build'),{recursive:true});
const fs=require('node:fs'),path=require('node:path');
const babel=require('./build-tools/node_modules/@babel/core');
const unicode=require('./build-tools/node_modules/@babel/plugin-transform-unicode-property-regex');
const sets=require('./build-tools/node_modules/@babel/plugin-transform-unicode-sets-regex');
const root=path.resolve(__dirname,'package'), backup=path.resolve(__dirname,'upstream');
const changed=[];
function visit(dir){for(const d of fs.readdirSync(dir,{withFileTypes:true})){
 const f=path.join(dir,d.name);if(d.isDirectory()){visit(f);continue;}if(!/\.(js|mjs|cjs)$/.test(f))continue;
 const saved=path.join(backup,path.relative(root,f));const original=fs.readFileSync(fs.existsSync(saved)?saved:f,'utf8');if(!/\\[pP]\{/.test(original))continue;
 const out=babel.transformSync(original,{filename:f,configFile:false,babelrc:false,sourceType:'unambiguous',retainLines:true,plugins:[[sets,{useUnicodeFlag:false}],[unicode,{useUnicodeFlag:false}]]}).code;
 const rel=path.relative(root,f);fs.mkdirSync(path.dirname(path.join(backup,rel)),{recursive:true});fs.writeFileSync(path.join(backup,rel),original);fs.writeFileSync(f,out);changed.push(rel);
}}
visit(path.join(root,'node_modules'));
fs.writeFileSync(path.resolve(__dirname,'./build/regex-patches.json'),JSON.stringify(changed,null,2));console.log('Transformed',changed.length,'files');
