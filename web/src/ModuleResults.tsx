type Device = {kind:string,id:string,metric:string,value:number,unit:string,log:string[]};
type Training = {initial_loss:number,losses:number[],gradients:number[],gradient_errors:number[]};
export function Devices({devices}:{devices:Device[]}) {
 return <section><h3>Simulated devices</h3>{devices.map(d=><article className="device" key={d.id}><h4>{d.kind} · {d.id}</h4><p>{d.metric} <strong>{d.value} {d.unit}</strong></p><details><summary>Connection history</summary><ol>{d.log.map((entry,i)=><li key={i}>{entry}</li>)}</ol></details></article>)}</section>;
}
export function TrainingResult({data}:{data:Training}){
 const logs=data.losses.map(v=>Math.log10(Math.max(Number.MIN_VALUE,v)));
 const ymin=Math.min(...logs),ymax=Math.max(...logs);
 return <section><h3>Gradient check</h3><table><caption>Initial parameter derivatives</caption><thead><tr><th>Parameter</th><th>Gradient</th><th>Absolute error</th></tr></thead><tbody>{['w','b','v','c'].map((name,i)=><tr key={name}><td>{name}</td><td>{data.gradients[i].toPrecision(5)}</td><td>{data.gradient_errors[i].toExponential(2)}</td></tr>)}</tbody></table><figure><figcaption>Training loss by update · logarithmic scale</figcaption><svg viewBox="0 0 500 150" role="img" aria-label={'Loss drops from '+data.initial_loss+' to '+data.losses.at(-1)}><path d="M40 8 V125 H490" stroke="currentColor" fill="none"/><polyline stroke="#009E73" fill="none" strokeWidth="2" points={logs.map((v,i)=>[40+450*i/(logs.length-1),125-112*(v-ymin)/(ymax-ymin||1)].join(',')).join(' ')}/><text x="230" y="145" fill="currentColor" fontSize="12">Update (0–101)</text></svg></figure><p>Final single-sample loss: {data.losses.at(-1)?.toExponential(3)}</p></section>
}
export function BatchResult({job}:{job:{state:string,progress:number,result:string}}) {
 const measurement=job.result?JSON.parse(job.result):null;
 return <section><h3>Batch {job.state}</h3><p>{job.progress} rods processed</p>{measurement&&<dl><dt>Mesh size per rod</dt><dd>{measurement.elements.toLocaleString()} elements</dd><dt>Cases</dt><dd>{measurement.cases}</dd><dt>Seed</dt><dd>{measurement.seed}</dd></dl>}</section>
}
