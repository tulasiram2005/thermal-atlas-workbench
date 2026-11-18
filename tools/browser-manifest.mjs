import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync} from 'node:fs';
const files=['src/core/thermal3d.cpp','src/core/thermal3d.hpp','src/cli/browser_bridge.cpp','web/public/wasm/thermal.mjs','web/public/wasm/thermal.wasm'];
writeFileSync('web/public/wasm/manifest.json',JSON.stringify({compiler:'Emscripten 4.0.10',files:Object.fromEntries(files.map(f=>[f,createHash('sha256').update(readFileSync(f)).digest('hex')]))},null,2)+'\n');
