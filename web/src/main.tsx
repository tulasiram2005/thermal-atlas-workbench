import React, {useState,lazy,Suspense} from 'react';
import {createRoot} from 'react-dom/client';
import './style.css';
import {Devices, TrainingResult, BatchResult} from './ModuleResults';
import type {GeometryData} from './GeometryViewer';
const GeometryViewer=lazy(()=>import('./GeometryViewer'));
const views=['Geometry','Thermal','Performance','Hardware','Backprop','Verification','About'];
async function api(path:string,body?:string){
 const response=await fetch(path,{method:body===undefined?'GET':'POST',headers:body===undefined?{}:{'Content-Type':'application/json'},body});
 const data=await response.json();if(!response.ok)throw new Error(`${data.code}: ${data.message}. ${data.hint}`);return data;
}
async function watchJob(id:number, onProgress:(value:any)=>void){
 let terminal=false;
 await new Promise<void>(resolve=>{
  const source=new EventSource(`/api/jobs/${id}/events`);
  const timer=window.setTimeout(finish,6000);
  function finish(){window.clearTimeout(timer);source.close();resolve()}
  source.addEventListener('progress',event=>{
   try {const state=JSON.parse((event as MessageEvent).data);onProgress(state);
    if(!['queued','running'].includes(state.state)){terminal=true;finish()}
   } catch {finish()}
  });
  source.addEventListener('expired',finish);source.onerror=finish;
 });
 while(!terminal){const state=await api(`/api/jobs/${id}`);onProgress(state);
  terminal=!['queued','running'].includes(state.state);
  if(!terminal)await new Promise(resolve=>setTimeout(resolve,200));
 }
}
function TemperaturePlot({x,temperature}:{x:number[],temperature:number[]}){
 const low=Math.min(...temperature),high=Math.max(...temperature);
 const width=640,height=300,left=65,top=22,plotWidth=550,plotHeight=225;
 const x0=x[0],span=x[x.length-1]-x0||1,range=high-low||1;
 const points=x.map((value,i)=>`${left+(value-x0)/span*plotWidth},${top+(high-temperature[i])/range*plotHeight}`).join(' ');
 return <figure><figcaption>Computed temperature along the rod</figcaption><svg viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Temperature versus position. Exact values follow in the data table.">
  <line x1={left} y1={top} x2={left} y2={top+plotHeight} stroke="currentColor"/>
  <line x1={left} y1={top+plotHeight} x2={left+plotWidth} y2={top+plotHeight} stroke="currentColor"/>
  <polyline points={points} fill="none" stroke="#56B4E9" strokeWidth="2"/>
  <text x={left-8} y={top+5} textAnchor="end" fill="currentColor" fontSize="11">{high.toFixed(1)}</text>
  <text x={left-8} y={top+plotHeight} textAnchor="end" fill="currentColor" fontSize="11">{low.toFixed(1)}</text>
  <text x={left} y={top+plotHeight+18} fill="currentColor" fontSize="11">{x0}</text>
  <text x={left+plotWidth} y={top+plotHeight+18} textAnchor="end" fill="currentColor" fontSize="11">{x[x.length-1]}</text>
  <text x="340" y="289" textAnchor="middle" fill="currentColor" fontSize="12">Position (m)</text>
  <text x="14" y="140" transform="rotate(-90 14 140)" textAnchor="middle" fill="currentColor" fontSize="12">Temperature (°C)</text>
 </svg></figure>
}
type MeshResult={elements:number,h:number,x:number[],temperature:number[],midpoint_error:number,order:number|null};
function ComparisonPlots({data}:{data:{comparison:MeshResult[],exact_x:number[],exact_temperature:number[]}}){
 const colors=['#56B4E9','#009E73','#E69F00','#D55E00'];
 const all=[...data.exact_temperature,...data.comparison.flatMap(m=>m.temperature)];
 const low=Math.min(...all),range=Math.max(...all)-low||1;
 const xmax=data.exact_x[data.exact_x.length-1]||1;
 const line=(x:number[],y:number[])=>x.map((v,i)=>`${65+v/xmax*550},${245-(y[i]-low)/range*215}`).join(' ');
 const positive=data.comparison.filter(m=>m.midpoint_error>0);
 const logs=positive.map(m=>({x:Math.log10(m.h),y:Math.log10(m.midpoint_error)}));
 const minx=Math.min(...logs.map(p=>p.x)),maxx=Math.max(...logs.map(p=>p.x));
 const miny=Math.min(...logs.map(p=>p.y)),maxy=Math.max(...logs.map(p=>p.y));
 return <section aria-label="Mesh convergence comparison"><figure><figcaption>Mesh comparison · Temperature (°C) versus position (m)</figcaption>
 <svg viewBox="0 0 640 290" role="img" aria-label="Four finite element meshes and a dashed exact temperature curve">
  <path d="M65 20 V245 H615" fill="none" stroke="currentColor"/>
  {data.comparison.map((m,i)=><polyline key={m.elements} points={line(m.x,m.temperature)} fill="none" stroke={colors[i]} strokeWidth="2"/>)}
  <polyline points={line(data.exact_x,data.exact_temperature)} fill="none" stroke="currentColor" strokeDasharray="6 4" strokeWidth="1.5"/>
  <text x="340" y="280" textAnchor="middle" fill="currentColor" fontSize="12">Position (m): 0 – {xmax}</text>
  <text x="16" y="135" transform="rotate(-90 16 135)" textAnchor="middle" fill="currentColor" fontSize="12">Temperature (°C)</text>
  <text x="60" y="30" textAnchor="end" fill="currentColor" fontSize="10">{(low+range).toFixed(1)}</text>
  <text x="60" y="245" textAnchor="end" fill="currentColor" fontSize="10">{low.toFixed(1)}</text>
 </svg><p>Blue: N=10 · Green: N=20 · Amber: N=40 · Vermilion: N=80 · Dashed: exact</p></figure>
 <figure><figcaption>Midpoint error convergence · logarithmic axes</figcaption>{logs.length>1?<svg viewBox="0 0 640 290" role="img" aria-label="Log-log midpoint error versus element length; numerical values in following table">
 <path d="M65 20 V245 H615" fill="none" stroke="currentColor"/>
 <polyline points={logs.map(p=>`${65+(p.x-minx)/(maxx-minx||1)*550},${245-(p.y-miny)/(maxy-miny||1)*215}`).join(' ')} fill="none" stroke="#56B4E9" strokeWidth="2"/>
 <text x="340" y="280" textAnchor="middle" fill="currentColor" fontSize="12">Element length (m), log₁₀: {minx.toFixed(2)} – {maxx.toFixed(2)}</text>
 <text x="16" y="135" transform="rotate(-90 16 135)" textAnchor="middle" fill="currentColor" fontSize="12">Midpoint error (°C), log₁₀</text>
 <text x="60" y="30" textAnchor="end" fill="currentColor" fontSize="10">{maxy.toFixed(2)}</text>
 <text x="60" y="245" textAnchor="end" fill="currentColor" fontSize="10">{miny.toFixed(2)}</text>
 </svg>:<p>Logarithmic error plot requires positive errors.</p>}</figure>
 <table><caption>Computed mesh convergence evidence</caption><thead><tr><th>Elements</th><th>h (m)</th><th>Error (°C)</th><th>Observed order</th></tr></thead><tbody>{data.comparison.map(m=><tr key={m.elements}><td>{m.elements}</td><td>{m.h.toPrecision(4)}</td><td>{m.midpoint_error.toPrecision(6)}</td><td>{m.order===null?'—':m.order.toFixed(4)}</td></tr>)}</tbody></table></section>
}
function VerificationResults({job}:{job:{state:string,progress:number,result:string}}){
 if(job.state!=='complete')return <p role="status">Verification {job.state} · {job.progress} checks recorded</p>;
 try{
  const report=JSON.parse(job.result) as {checks:{name:string,passed:boolean,measured:number,units:string}[],scope:string};
  return <section><p>{report.scope}</p><table><caption>Measured core checks</caption><thead><tr><th>Check</th><th>Status</th><th>Measured value</th></tr></thead><tbody>{report.checks.map(check=><tr key={check.name}><td>{check.name}</td><td>{check.passed?'PASS':'FAIL'}</td><td>{check.measured.toPrecision(6)} {check.units}</td></tr>)}</tbody></table><p>Full specification acceptance remains pending.</p></section>;
 }catch{return <p role="alert">Verification result could not be read. Export details and retry.</p>}
}
function App(){
 const [geometry,setGeometry]=useState<GeometryData|null>(null);
 const [view,setView]=useState('Thermal'),[light,setLight]=useState(false),[busy,setBusy]=useState(false),[error,setError]=useState(''),[result,setResult]=useState<any>(null),[elements,setElements]=useState(10),[job,setJob]=useState<number|null>(null);
 async function run(){setBusy(true);setError('');setResult(null);try{
  if(view==='Thermal')setResult(await api('/api/fea',JSON.stringify({elements})));
  if(view==='Backprop')setResult(await api('/api/backprop'));
  if(view==='Hardware')setResult(await api('/api/hardware'));
  if(view==='Verification'){
   const submitted=await api('/api/jobs/verification','{}');setJob(submitted.id);
   await watchJob(submitted.id,setResult);setJob(null);
  }
  if(view==='Performance'){
   const submitted=await api('/api/jobs/batch','{}');setJob(submitted.id);
   await watchJob(submitted.id,setResult);
   setJob(null);
  }
 }catch(e){setError(String(e))}finally{setBusy(false)}}
 async function upload(file:File|undefined){
  if(!file)return;setBusy(true);setError('');setGeometry(null);setResult(null);
  try{
   const kind=file.name.toLowerCase().endsWith('.stl')?'stl':'csv';const body=await file.text();
   const [stats,response]=await Promise.all([api(`/api/${kind}`,body),fetch(`/api/${kind}/binary`,{method:'POST',body})]);
   if(!response.ok)throw new Error('Geometry transfer failed. Check file format and size.');
   const bytes=await response.arrayBuffer(),view=new DataView(bytes),values=new Float64Array(bytes.byteLength/8);
   for(let i=0;i<values.length;i++)values[i]=view.getFloat64(i*8,true);
   setResult(stats);setGeometry({kind,values,minimum:stats.minimum,maximum:stats.maximum});
  }catch(e){setError(String(e))}finally{setBusy(false)}
 }
 function download(){const url=URL.createObjectURL(new Blob([JSON.stringify(result,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='workbench-result.json';a.click();URL.revokeObjectURL(url)}
 return <div className={light?'app light':'app'}><header><strong><span className="mark">—●—</span> THERMAL ATLAS</strong><span className="subtitle">UNIFIED ENGINEERING WORKBENCH</span><button onClick={()=>setLight(!light)}>{light?'Dark':'Light'} theme</button></header><div className="heat"/><div className="shell"><nav aria-label="Workspace">{views.map((name,i)=><button key={name} aria-current={view===name?'page':undefined} disabled={busy} onClick={()=>{setView(name);setResult(null);setError('')}}><span>0{i+1}</span>{name}</button>)}<p>LOCAL COMPUTATION<br/>C++ numerical core<br/>Development build</p></nav><main><div className="heading"><div><p className="eyebrow">ENGINEERING / {view.toUpperCase()}</p><h1>{view==='Thermal'?'Thermal analysis':view}</h1></div><span className="badge">OFFLINE CAPABLE</span></div><div className="workspace"><section className="controls"><h2>{view==='About'?'Application details':'Experiment setup'}</h2>{view==='Thermal'?<><p>Steady-state heat conduction in a one-dimensional rod.</p><label>Mesh elements <input type="number" min="2" max="2000" value={elements} onChange={e=>setElements(Number(e.target.value))}/></label><dl><dt>Length</dt><dd>1 m</dd><dt>Conductivity</dt><dd>10 W/(m K)</dd><dt>Heat source</dt><dd>10,000 W/m³</dd><dt>Boundary temperatures</dt><dd>100 → 20 °C</dd></dl></>:null}
{view==='Geometry'?<><p>Upload ASCII STL or thermal CSV. Files are parsed locally by the C++ server.</p><label>Input file<input type="file" accept=".stl,.csv" disabled={busy} onChange={e=>upload(e.target.files?.[0])}/></label><p>Drag to orbit; scroll to zoom. Rendering stays on this computer.</p></>:null}
{view==='Hardware'?<p className="notice">SIMULATED · Three mock devices demonstrate connection timeout and recovery. No physical devices are accessed.</p>:null}
{view==='Performance'?<p>Run a background batch of 128 rods with 8,000 elements. This checks execution; the complete benchmark UI remains pending.</p>:null}
{view==='Backprop'?<p>One-sample tanh network. Analytic gradients checked with central finite differences.</p>:null}
{view==='Verification'?<p>Check FEA, backprop, parsers, batch agreement and simulated recovery. Browser and Windows acceptance remain pending.</p>:null}
{view==='About'?<p>Built with C++, React, TypeScript and Vite. AI-assisted development. Full licence inventory and final verification are in progress.</p>:null}
{!['Geometry','About'].includes(view)&&<button className="primary" disabled={busy||!Number.isInteger(elements)||elements<2||elements>2000} onClick={run}>{busy?'Running…':view==='Verification'?'Verify core modules':'Run experiment'}</button>}
{job!==null&&<button onClick={()=>api(`/api/jobs/${job}/cancel`,'{}').catch(e=>setError(String(e)))}>Cancel job</button>}</section><section className="canvas" aria-label="Results"><div className="panel-title"><h2>{view==='About'?'Getting started':'Results'}</h2>{result&&<button onClick={download}>Export JSON</button>}</div>{error&&<div role="alert" className="error">{error}</div>}<div role="status" aria-live="polite">{busy?'Computing locally…':''}</div>{view==='About'&&<section><h3>How this workbench runs</h3><p>Uploaded files are parsed by the local C++ service. Three.js renders geometry in your browser. Export results to keep a copy.</p><p>Sample data is synthetic. Hardware telemetry is simulated.</p></section>}{view!=='About'&&!result&&!error&&<div className="empty"><span className="rod">●━━━━●━━━━●━━━━●</span><h2>Your experiment starts here</h2><p>Choose inputs, then run. Results come directly from the C++ computation.</p></div>}{result&&view==='Thermal'&&<><div className="stats"><div><small>MIDPOINT ERROR · °C</small><strong>{result.midpoint_error.toPrecision(6)}</strong></div><div><small>NODAL ERROR · °C</small><strong>{result.nodal_error.toExponential(3)}</strong></div></div><TemperaturePlot x={result.x} temperature={result.temperature}/><ComparisonPlots data={result}/><table><caption>Computed nodal temperatures</caption><thead><tr><th>Position (m)</th><th>Temperature (°C)</th></tr></thead><tbody>{result.x.map((x:number,i:number)=><tr key={i}><td>{x.toFixed(4)}</td><td>{result.temperature[i].toFixed(4)}</td></tr>)}</tbody></table></>}{view==='Geometry'&&geometry&&<Suspense fallback={<p>Loading local viewer…</p>}><GeometryViewer data={geometry}/></Suspense>}{result&&view==='Verification'&&<VerificationResults job={result}/>} {result&&view==='Hardware'&&<Devices devices={result}/>} {result&&view==='Backprop'&&<TrainingResult data={result}/>} {result&&view==='Performance'&&<BatchResult job={result}/>} {result&&view==='Geometry'&&<table><caption>Parsed geometry statistics</caption><thead><tr><th>Property</th><th>Value</th></tr></thead><tbody>{Object.entries(result).map(([key,value])=><tr key={key}><td>{key.replaceAll('_',' ')}</td><td>{typeof value==='number'?Number.isInteger(value)?value:value.toPrecision(6):JSON.stringify(value)}</td></tr>)}</tbody></table>}</section></div><footer>DEVELOPMENT BUILD · Numerical checks are available in Verification. Final desktop and submission acceptance remain pending.</footer></main></div></div>
}
createRoot(document.getElementById('root')!).render(<App/>);
