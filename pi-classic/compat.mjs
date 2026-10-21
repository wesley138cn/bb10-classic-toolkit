import { Segmenter } from './package/node_modules/unicode-segmenter/intl-adapter.js';
if (!globalThis.Intl) globalThis.Intl = {};
if (!globalThis.Intl.Segmenter) globalThis.Intl.Segmenter = Segmenter;
