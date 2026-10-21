// Pi 0.74.2 QNX entry: preserve upstream main while replacing unavailable host facilities.
import './compat.mjs';
import { curlFetch } from './curl-fetch.mjs';
import { Request, Response, Headers, FormData, Blob, File } from './package/node_modules/node-fetch/src/index.js';
for (const [name,value] of Object.entries({Request,Response,Headers,FormData,Blob,File,fetch:curlFetch})) {
 Object.defineProperty(globalThis,name,{value,writable:true,configurable:true});
}
process.env.PI_CODING_AGENT='true';
const { main }=await import('./package/node_modules/@earendil-works/pi-coding-agent/dist/bb10-main.mjs');
await main(process.argv.slice(2));
