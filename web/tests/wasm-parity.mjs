import createModule from '../public/wasm/thermal.mjs';
import {execFileSync} from 'node:child_process';
import {writeFileSync,mkdirSync} from 'node:fs';
import {resolve} from 'node:path';
const m=await createModule();const cases=[];
const nativeBridge=process.env.THERMAL_NATIVE_BRIDGE||resolve('../build/thermal_native_bridge'+(process.platform==='win32'?'.exe':''));
for(const n of [3,6,9,12])for(const neck of [false,true]){
 const wasm=JSON.parse(m.ccall('thermal_compare','string',['number','number'],[n,Number(neck)]));
 const native=JSON.parse(execFileSync(nativeBridge,[String(n),String(Number(neck))]));
 if(JSON.stringify(wasm.cells)!==JSON.stringify(native.cells))throw Error('Cell mismatch');
 for(let i=0;i<3;i++){
  const a=wasm.runs[i],b=native.runs[i];const max=Math.max(...a.temperature.map((t,j)=>Math.abs(t-b.temperature[j])));
  if(!a.converged||max>1e-7||a.iterations!==b.iterations)throw Error(`Mismatch ${n} ${neck} ${a.method}: ${max}`);
  cases.push({n,neck,method:a.method,max_temperature_difference_C:max,iterations:a.iterations});
 }
}
for(const args of [[0,0],[13,0],[6,2],[4,0]])if(!JSON.parse(m.ccall('thermal_compare','string',['number','number'],args)).error)throw Error('Invalid input accepted');
mkdirSync('../results',{recursive:true});writeFileSync('../results/wasm-parity.json',JSON.stringify({passed:true,cases,invalid_cases:4},null,2));
console.log(JSON.stringify({passed:true,solver_comparisons:cases.length,invalid_cases:4,max_difference_C:Math.max(...cases.map(c=>c.max_temperature_difference_C))}));
