import type {Study} from './SolverLab';
export function calculateInBrowser(n:number,neck:boolean):Promise<Study>{
 return new Promise((resolve,reject)=>{
  const worker=new Worker('/wasm/worker.mjs',{type:'module'});
  const timer=setTimeout(()=>{worker.terminate();reject(Error('Calculation stopped after 30 seconds. Try a smaller grid.'))},30000);
  const finish=()=>{clearTimeout(timer);worker.terminate()};
  worker.onmessage=({data})=>{finish();data.error?reject(Error(data.error)):resolve(data.result)};
  worker.onerror=()=>{finish();reject(Error('Unable to load the local solver. Reload or try a current browser.'))};
  worker.postMessage({n,neck});
 });
}
