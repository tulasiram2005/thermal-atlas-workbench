import createModule from './thermal.mjs';
const modulePromise=createModule();
self.onmessage=async ({data})=>{
  try {
    if(!Number.isInteger(data.n)||![3,6,9,12].includes(data.n)||typeof data.neck!=='boolean')throw Error('Invalid mesh or geometry');
    const module=await modulePromise;
    const result=JSON.parse(module.ccall('thermal_compare','string',['number','number'],[data.n,Number(data.neck)]));
    if(result.error)throw Error(result.error);
    self.postMessage({result});
  }catch(error){self.postMessage({error:String(error)});}
};
