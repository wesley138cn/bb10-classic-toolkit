// QNX Node --jitless has no WebAssembly. Use verified BerryCore curl for HTTPS.
// Credentials and JSON are supplied on stdin, never in argv or shared files.
import { spawn } from 'node:child_process';
import { Readable } from 'node:stream';
import { Request, Response, Headers } from './package/node_modules/node-fetch/src/index.js';
const CORE = '/accounts/1000/shared/misc/berrycore';
const quote = value => '"' + String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n').replace(/\t/g, '\\t') + '"';
export async function curlFetch(input, init = {}) {
  const request = new Request(input, init);
  const url = new URL(request.url);
  if (!['http:', 'https:'].includes(url.protocol)) throw new TypeError('Only HTTP(S) is supported');
  const body = request.body ? await request.text() : null;
  const signal = request.signal || new AbortController().signal;
  if (signal.aborted) throw signal.reason ?? new DOMException('Aborted', 'AbortError');
  let config = `url = ${quote(url.href)}\nrequest = ${quote(request.method)}\nheader = "Expect:"\n`;
  for (const [key, value] of request.headers) config += `header = ${quote(key + ': ' + value)}\n`;
  if (body !== null) config += `data-binary = ${quote(body)}\n`;
  return new Promise((resolve, reject) => {
    let header = Buffer.alloc(0), resolved = false, ended = false, streamController;
    const stream = new ReadableStream({start(c) { streamController = c; }, cancel() { child.kill(); }});
    const child = spawn(process.env.PI_CLASSIC_CURL || CORE + '/bin/curl',
      ['--silent', '--show-error', '--no-buffer', '--compressed', '--include', '--connect-timeout', '20', '--max-time', '180', '--config', '-'],
      {env: {...process.env, LD_LIBRARY_PATH: process.env.PI_CLASSIC_CURL_LIB || CORE + '/lib:/base/lib:/base/usr/lib'}, stdio: ['pipe','pipe','pipe']});
    let stderr = '';
    const fail = e => { if (ended) return; ended = true; signal.removeEventListener('abort', abort); if (!resolved) reject(e); try { streamController.error(e); } catch {} };
    const abort = () => { child.kill(); fail(signal.reason ?? new DOMException('Aborted', 'AbortError')); };
    signal.addEventListener('abort', abort, {once:true});
    child.on('error', fail);
    child.stdin.on('error', () => {});
    child.stderr.on('data', b => { stderr = (stderr + b.toString()).slice(-2048); });
    child.stdout.on('data', chunk => {
      if (ended) return;
      if (resolved) { try { streamController.enqueue(chunk); } catch { child.kill(); } return; }
      header = Buffer.concat([header,chunk]);
      while (!resolved) {
        const pos = header.indexOf('\r\n\r\n');
        if (pos < 0) { if (header.length > 65536) { child.kill(); fail(new Error('HTTP headers too large')); } return; }
        const lines = header.subarray(0,pos).toString('latin1').split('\r\n');
        header = header.subarray(pos+4);
        const match = /^HTTP\/\S+\s+(\d{3})(?:\s+(.*))?$/.exec(lines.shift());
        if (!match) {child.kill();fail(new Error('Invalid HTTP response'));return;}
        const status=Number(match[1]);
        if (status < 200 || match[2]?.toLowerCase() === 'connection established') continue;
        const headers=new Headers();
        for (const line of lines) {const i=line.indexOf(':');if(i>0)headers.append(line.slice(0,i),line.slice(i+1).trim());}
        // curl has already decompressed response bytes.
        headers.delete('content-encoding');headers.delete('content-length');
        resolved=true;
        const response=new Response([204,205,304].includes(status)?null:Readable.fromWeb(stream),{status,statusText:match[2]||'',headers});
        Object.defineProperty(response,'url',{value:url.href});
        resolve(response);
        if (header.length) streamController.enqueue(header);
        header=Buffer.alloc(0);
      }
    });
    child.on('close',code=>{
      signal.removeEventListener('abort',abort);
      if(ended)return;
      if(code!==0){fail(new Error(`curl transport failed (${code}): ${stderr.replace(/Bearer\s+\S+/gi,'Bearer [REDACTED]')}`));return;}
      if(!resolved){fail(new Error('No HTTP response headers'));return;}
      ended=true;try{streamController.close();}catch{}
    });
    child.stdin.end(config);config='';
  });
}
