import assert from 'node:assert/strict';
import http from 'node:http';
import {curlFetch} from './curl-fetch.mjs';
process.env.PI_CLASSIC_CURL='/usr/bin/curl';process.env.PI_CLASSIC_CURL_LIB='';
const server=http.createServer(async(req,res)=>{
 let body='';for await(const b of req)body+=b;
 if(req.url==='/slow'){res.writeHead(200);res.write('first');setTimeout(()=>res.end('last'),1000);return;}
 if(req.url==='/fail'){res.writeHead(429);res.end('quota');return;}
 res.writeHead(200,{'content-type':'application/json'});res.end(JSON.stringify({body,authorization:req.headers.authorization}));
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
try{
 const u=`http://127.0.0.1:${server.address().port}`;
 const payload=JSON.stringify({text:'中文 "quote" \\ slash\n换行'});
 const response=await curlFetch(u,{method:'POST',headers:{Authorization:'Bearer dummy-test'},body:payload});
 assert.equal(response.status,200);assert.deepEqual(await response.json(),{body:payload,authorization:'Bearer dummy-test'});
 const bad=await curlFetch(u+'/fail');assert.equal(bad.status,429);assert.equal(await bad.text(),'quota');
 const controller=new AbortController();const slow=await curlFetch(u+'/slow',{signal:controller.signal});controller.abort();await assert.rejects(slow.text());
 console.log('PASS: Unicode/body/header integrity, HTTP error preservation, streaming cancellation');
}finally{server.closeAllConnections();server.close();}
