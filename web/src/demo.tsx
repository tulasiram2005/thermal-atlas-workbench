import {lazy,Suspense,useEffect,useMemo,useState} from 'react';
import {createRoot} from 'react-dom/client';
import {calculateInBrowser} from './browser-solver';
import type {Study} from './SolverLab';
import {ResidualChart,inks,residualY} from './ResidualChart';
import './instrument.css';

const Field=lazy(()=>import('./ThermalField'));
const repo='https://github.com/tulasiram2005/thermal-atlas-workbench';
const geometry=(neck:boolean)=>neck?'Constricted block':'Reference cube';
const grid=(n:number)=>[n,n,n].join(' × ');
function Choices<T extends number|boolean>({label,choices,value,onChange,disabled}:{label:string,choices:{value:T,label:string}[],value:T,onChange:(v:T)=>void,disabled:boolean}){
  return <fieldset disabled={disabled}><legend>{label}</legend><div className="segments">{choices.map(c=><label key={String(c.value)}><input type="radio" name={label} checked={value===c.value} onChange={()=>onChange(c.value)}/><span>{c.label}</span></label>)}</div></fieldset>;
}
function Count({total,paused}:{total:number,paused:boolean}){
 const [value,setValue]=useState(total);
 useEffect(()=>{if(paused){setValue(total);return}
  let frame=0;const start=performance.now();
  const tick=(now:number)=>{const t=Math.min(1,(now-start)/480);setValue(Math.round(total*t));if(t<1)frame=requestAnimationFrame(tick)};
  frame=requestAnimationFrame(tick);return ()=>cancelAnimationFrame(frame);
 },[total,paused]);return <strong aria-label={String(total)}>{value}</strong>;
}
function Demo(){
 const [manualPause,setManualPause]=useState(false),[reduced,setReduced]=useState(matchMedia('(prefers-reduced-motion: reduce)').matches);
 const [revision,setRevision]=useState(0);
 useEffect(()=>{const q=matchMedia('(prefers-reduced-motion: reduce)'),sync=()=>setReduced(q.matches);q.addEventListener('change',sync);return ()=>q.removeEventListener('change',sync)},[]);
 const paused=reduced||manualPause;
 const [light,setLight]=useState(document.documentElement.dataset.theme==='light');
 const [neck,setNeck]=useState(true),[n,setN]=useState(6),[study,setStudy]=useState<Study|null>(null),[selected,setSelected]=useState(2);
 const [busy,setBusy]=useState(false),[error,setError]=useState(''),[notice,setNotice]=useState('');
 const stale=!!study&&(study.n!==n||study.neck!==neck);
 const run=study?.runs[selected];
 const field=useMemo(()=>study&&run?{kind:'csv' as const,voxelSize:1/study.n,values:Float64Array.from(study.cells.flatMap((c,i)=>[i,...c.map(v=>(v+.5)/study.n),run.temperature[i]])),minimum:Math.min(...run.temperature),maximum:Math.max(...run.temperature)}:null,[study,run]);
 async function compare(){setBusy(true);setError('');setNotice('');try{setStudy(await calculateInBrowser(n,neck));setRevision(v=>v+1)}catch(e){setError(typeof WebAssembly==='undefined'?'WebAssembly unavailable. Open the desktop app or use a browser with WebAssembly enabled.':String(e))}finally{setBusy(false)}}
 function download(){if(!study||stale||busy)return;const url=URL.createObjectURL(new Blob([JSON.stringify(study,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='thermal-3d-comparison.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);setNotice('Evidence download started.')}
 function theme(){const next=light?'dark':'light';document.documentElement.dataset.theme=next;setLight(!light);try{localStorage.setItem('thermal-theme',next)}catch{/* Storage is optional. */}}
 const best=Math.min(...(study?.runs.filter(r=>r.converged).map(r=>r.iterations)??[]));
 return <><header className="top"><div className="bar"><strong className="word"><b aria-hidden="true">∿</b> Thermal Atlas</strong><span className="prototype">Research prototype</span><small className="slot">Logo placeholder · IIT Madras Research Park</small><button aria-label={'Switch to '+(light?'dark':'light')+' theme'} onClick={theme}>{light?'☾':'☀'}</button><a href={repo}>Source &amp; desktop app ↗</a></div></header>
 <main data-paused={paused}><section className="hero"><p className="kicker">THERMAL ATLAS / SOLVER LAB</p><h1>Same physics. Three paths to a solution.</h1><p>See how heat moves through a solid and how many steps each solver takes to describe it.</p><div className="trust" tabIndex={0} title="Runs locally. No upload, account, or server calculation.">▣ C++ WebAssembly · on your device</div><ol><li>Choose geometry</li><li>Compare solvers</li><li>Inspect &amp; export</li></ol></section>
 <section className="setup" aria-label="Experiment setup"><div className="toolbar"><Choices label="Geometry" value={neck} disabled={busy} onChange={setNeck} choices={[{value:false,label:'◇ Reference cube'},{value:true,label:'⋈ Constricted block'}]}/><Choices label="Grid resolution" value={n} disabled={busy} onChange={setN} choices={[3,6,9,12].map(v=>({value:v,label:String(v)}))}/><button className="primary" disabled={busy} onClick={compare}>{busy?'Solving…':'Compare solvers'}</button><button disabled={!study||stale||busy} onClick={download}>↓ Export evidence</button></div>
 <details><summary>Model assumptions</summary><p>Steady conduction · k = 10 W/(m·K) · q = 10,000 W/m³ · end faces 100 / 20 °C · all other exposed faces insulated. Cell-centered finite volume, not a tetrahedral FEM model.</p></details></section>
 <div className="status" role="status">{study?<><strong>Last completed run · {geometry(study.neck)} · {grid(study.n)}</strong><span>{study.runs.every(r=>r.converged)?'✓':'⚠'} {study.cells.length} cells · {study.runs.filter(r=>r.converged).length} of 3 solvers converged</span></>:'Ready to compare'}{stale&&<strong className="warning">Settings changed — compare again</strong>}{busy&&' · Calculating locally…'}{notice&&<span>{notice}</span>}</div>{error&&<p role="alert">⚠ {error}</p>}
 <div className="motion-setting"><button aria-pressed={paused} disabled={reduced} onClick={()=>setManualPause(!manualPause)}>{reduced?'Reduced motion enabled':paused?'Resume motion':'Pause motion'}</button></div><div className="panels"><section className="panel"><p className="kicker">SPATIAL RESULT</p><h2>3D temperature field</h2><p>{run?.method??'Computed cell temperatures'} · °C</p><div className="field-space">{field?<Suspense fallback={<div className="placeholder">Loading field…</div>}><Field key={revision+'-'+selected} data={field} paused={paused}/></Suspense>:<div className="placeholder"><span aria-hidden="true">◇</span>Run Compare solvers to reveal the temperature field</div>}</div></section>
 <section className="panel"><p className="kicker">NUMERICAL RESULT</p><h2>Convergence history</h2><p>True relative residual ‖b − AT‖₂ / ‖b‖₂</p>{study?<ResidualChart key={revision} study={study}/>:<div className="placeholder">Residual histories appear after a solve.</div>}</section></div>
 {study&&<><fieldset className="cards" key={revision}><legend>Inspect a solver</legend>{study.runs.map((r,i)=>{const history=r.residual_history.map(residualY),max=Math.max(1,...history);return <label className="card" key={r.method}><input type="radio" name="method" checked={i===selected} onChange={()=>setSelected(i)}/><b><i style={{background:inks[i]}}/>{r.method}</b><Count total={r.iterations} paused={paused}/><small>iterations · {r.converged?'✓ Converged':'⚠ Limit reached'}</small><svg viewBox="0 0 140 28" aria-label="Residual history"><polyline fill="none" stroke={inks[i]} strokeWidth="2" points={history.map((v,k)=>k*140/Math.max(1,history.length-1)+','+(2+v/max*24)).join(' ')}/></svg>{r.converged&&r.iterations===best&&<small>Fewest iterations · {study.runs.filter(other=>other.iterations>best&&other.converged).map(other=>other.method+' used '+(other.iterations/best).toFixed(1)+'× as many').join(' · ')}</small>}</label>})}</fieldset>
 <section className="panel"><h2>Solver evidence</h2><p>Timings below are single interactive observations. Use the repeated CLI study for report comparisons.</p><div className="table-wrap" tabIndex={0} role="region" aria-label="Scrollable solver evidence"><table><thead><tr>{['Method','Iterations','Solve time','True residual','Heat imbalance'].map((h,i)=><th key={h}>{i>=3?<abbr tabIndex={0} title={i===3?'Norm of b − AT divided by the norm of b':'Absolute difference between outward and generated heat, in watts'}>{h}</abbr>:h}</th>)}</tr></thead><tbody>{study.runs.map((r,i)=>{const metrics=(v:typeof r)=>[v.iterations,v.elapsed_ms,v.relative_residual,Math.abs(v.outward_heat-v.generated_heat)];return <tr key={r.method}><th><i style={{background:inks[i]}}/>{r.method}</th>{[String(r.iterations),r.elapsed_ms<1?'<1 ms':r.elapsed_ms.toFixed(2)+' ms',r.relative_residual.toExponential(3),metrics(r)[3].toExponential(3)+' W'].map((s,k)=><td key={k} className={metrics(r)[k]===Math.min(...study.runs.map(v=>metrics(v)[k]))?'best':''}>{s}</td>)}</tr>})}</tbody></table></div></section></>}</main>
 <footer><strong>Thermal Atlas · v0.1.0</strong><p>Prepared for presentation to IIT Madras Research Park visitors · <a href={repo}>Project source ↗</a></p><p>This browser edition contains Solver Lab. Geometry viewing, thermal calculation and evidence export run on your device. The full desktop workbench includes the additional rod, batch, hardware-simulation and verification modules.</p><p>Browser timings depend on your device and are not the native benchmark results. No uploaded data, account, database or server-side calculation is required.</p></footer></>;
}
createRoot(document.getElementById('root')!).render(<Demo/>);
