require('node:fs').mkdirSync(require('node:path').join(__dirname,'build'),{recursive:true});
const esbuild=require('./build-tools/node_modules/esbuild');
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'package');
const phone='/accounts/1000/shared/misc/pi-classic/package';
const report=path.resolve(__dirname,'./build/bundle.json');
esbuild.build({entryPoints:[path.join(root,'node_modules/@earendil-works/pi-coding-agent/dist/main.js')],outfile:path.resolve(__dirname,'main-bundle.mjs'),bundle:true,platform:'node',format:'esm',target:'node22',minify:false,metafile:true,external:['jiti','@silvia-odwyer/photon-node','@mariozechner/clipboard'],banner:{js:'import {createRequire as __bbCreateRequire} from "node:module";const require=__bbCreateRequire("file:///accounts/1000/shared/misc/pi-classic/package/node_modules/@earendil-works/pi-coding-agent/dist/main.js");'},plugins:[{name:'preserve-source-paths',setup(build){build.onLoad({filter:/\.(m?js|cjs)$/},args=>{
 if(!args.path.startsWith(root+path.sep))return;
 let source=fs.readFileSync(args.path,'utf8');
 if(source.includes('import.meta.url'))source=source.replace(/import\.meta\.url/g,JSON.stringify('file://'+phone+'/'+path.relative(root,args.path).split(path.sep).join('/')));
 return {contents:source,loader:'js',resolveDir:path.dirname(args.path)};
});}}]}).then(result=>{fs.writeFileSync(report,JSON.stringify(result.metafile));console.log('Bundle bytes',fs.statSync(path.resolve(__dirname,'main-bundle.mjs')).size,'input modules',Object.keys(result.metafile.inputs).length)}).catch(e=>{console.error(e);process.exit(1)});
