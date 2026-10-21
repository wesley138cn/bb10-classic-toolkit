import fs from 'node:fs';
import assert from 'node:assert/strict';
import {Segmenter} from './package/node_modules/unicode-segmenter/intl-adapter.js';
const p='./package/node_modules/@earendil-works/pi-tui/dist/utils.js';
const original=new URL(p.replace(/\.js$/,'.original-test.js'),import.meta.url);
fs.copyFileSync(new URL('./upstream/node_modules/@earendil-works/pi-tui/dist/utils.js',import.meta.url),original);
try{
 const a=await import(original.href),b=await import(new URL(p,import.meta.url));
 const samples=['黑莓 Classic','中文 English','é','👩‍💻','👨‍👩‍👧‍👦','🇨🇳','\u001b[31m中文\u001b[0m','👍🏽'];
 for(let c=0;c<0x110000;c+=61){if(c>=0xd800&&c<=0xdfff)continue;samples.push(String.fromCodePoint(c));}
 for(const s of samples)assert.equal(b.visibleWidth(s),a.visibleWidth(s),JSON.stringify(s));
 for(const s of samples.slice(0,8))assert.deepEqual([...new Segmenter().segment(s)].map(x=>x.segment),[...new Intl.Segmenter().segment(s)].map(x=>x.segment));
 console.log('PASS:',samples.length,'width comparisons and Chinese/emoji/grapheme segmentation');
}finally{fs.unlinkSync(original);}
