import {useState} from 'react';
import type {Study} from './SolverLab';
export const inks=['#5ab0ff','#f5a524','#3dd6a0'];
export function residualY(value:number){
  return Math.max(0,-Math.log10(Math.max(value,1e-16)));
}
export function ResidualChart({study}:{study:Study}){
  const [hidden,setHidden]=useState<number[]>([]), [cursor,setCursor]=useState<number|null>(null);
  const end=Math.max(1,...study.runs.map(r=>r.residual_history.length-1));
  const depth=Math.max(9,...study.runs.map(r=>Math.ceil(Math.max(...r.residual_history.map(residualY)))));
  const x=(i:number)=>46+i/end*420, y=(v:number)=>18+v/depth*178;
  return <><div className="legend">{study.runs.map((r,i)=><button key={r.method} aria-pressed={!hidden.includes(i)} onClick={()=>setHidden(hidden.includes(i)?hidden.filter(h=>h!==i):[...hidden,i])}><i style={{background:inks[i]}}/>{r.method}</button>)}</div>
  <svg className="plot" viewBox="0 0 480 230" role="img" aria-label={'Relative residual by iteration. Target ten to the minus eight. '+study.runs.map(r=>r.method+' ended at '+r.relative_residual.toExponential(2)).join('. ')}
    onPointerMove={e=>{const svg=e.currentTarget,m=svg.getScreenCTM();if(m){const p=new DOMPoint(e.clientX,e.clientY).matrixTransform(m.inverse());setCursor(Math.min(end,Math.max(0,Math.round((p.x-46)*end/420))))}}} onPointerLeave={()=>setCursor(null)}>
    {Array.from({length:depth+1},(_,i)=><line key={i} x1="46" x2="466" y1={y(i)} y2={y(i)} className={i===8?'target':'gridline'}/>)}
    {[0,4,8].map((d,i)=><text key={d} x="39" y={y(d)+4} textAnchor="end">{['10⁰','10⁻⁴','10⁻⁸'][i]}</text>)}
    <text x="463" y={y(8)-5} textAnchor="end">tolerance</text>
    {study.runs.map((r,i)=>!hidden.includes(i)&&<polyline key={r.method} pathLength="1" fill="none" stroke={inks[i]} strokeWidth="2.5" strokeDasharray={['none','7 2','2 2'][i]} points={r.residual_history.map((v,j)=>x(j)+','+y(residualY(v))).join(' ')}/>)}
    {cursor!==null&&<line x1={x(cursor)} x2={x(cursor)} y1="18" y2="196" stroke="currentColor"/>}
    <text x="46" y="220">0</text><text x="256" y="220" textAnchor="middle">Iteration</text><text x="466" y="220" textAnchor="end">{end}</text>
  </svg><label className="chart-control">Inspect iteration <input type="range" min="0" max={end} value={cursor??0} onChange={e=>setCursor(+e.target.value)}/></label>
  <div className="readings">{cursor===null?'Hover or use the slider to inspect residuals.':<>Iteration {cursor} · {study.runs.map((r,i)=>!hidden.includes(i)&&<span key={i}>{r.method} {r.residual_history[cursor]?.toExponential(2)??'finished'} </span>)}</>}</div></>;
}
