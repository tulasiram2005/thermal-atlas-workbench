import {useEffect,useRef,useState} from 'react';
import * as THREE from 'three';
import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';
import {interpolateInferno} from 'd3-scale-chromatic';
import type {GeometryData} from './GeometryViewer';

const poses={Fit:[1.5,1,1.5],Front:[0,0,1.9],Top:[0,1.9,.001],Side:[1.9,0,0],Constriction:[.93,.63,.93]} as const;
type Pose=keyof typeof poses;
const axes=['x','y','z'];
export default function ThermalField({data,paused}:{data:GeometryData,paused:boolean}){
 const host=useRef<HTMLDivElement>(null),cameraTo=useRef<(name:Pose)=>void>(()=>{}),wake=useRef(()=>{});
 const [axis,setAxis]=useState(0),[layer,setLayer]=useState(0),[wire,setWire]=useState(false);
 const [node,setNode]=useState<number|null>(null),[hint,setHint]=useState(true),[fallback,setFallback]=useState(false),[copied,setCopied]=useState('');
 const size=data.voxelSize!, n=Math.round(1/size),count=data.values.length/5;
 const displayed=Array.from({length:count},(_,i)=>i).filter(i=>!layer||data.values[i*5+1+axis]<layer*size);
 const state=useRef({axis,layer,wire,node,paused});
 useEffect(()=>{state.current={axis,layer,wire,node,paused};wake.current()},[axis,layer,wire,node,paused]);
 useEffect(()=>{
  const target=host.current;if(!target)return;
  let renderer:THREE.WebGLRenderer;
  try{renderer=new THREE.WebGLRenderer({alpha:true,antialias:true})}catch{setFallback(true);return}
  renderer.setPixelRatio(Math.min(window.devicePixelRatio,2));renderer.localClippingEnabled=true;
  const canvas=renderer.domElement;target.appendChild(canvas);canvas.setAttribute('role','img');canvas.setAttribute('aria-label','Temperature field. Use the camera buttons and node stepper below to inspect cells.');
  const scene=new THREE.Scene(),camera=new THREE.PerspectiveCamera(42,1,.01,20);
  camera.position.set(...poses.Fit);const orbit=new OrbitControls(camera,canvas);orbit.enablePan=false;orbit.minDistance=.65;orbit.maxDistance=5;
  const plane=new THREE.Plane(),box=new THREE.BoxGeometry(size*.98,size*.98,size*.98),material=new THREE.MeshBasicMaterial();
  // Reveal the final measured field. This is a display effect, not a thermal time step.
  const reveal={value:0},revealStart=performance.now();
  material.onBeforeCompile=shader=>{
   shader.uniforms.fieldReveal=reveal;
   shader.uniforms.coldColor={value:new THREE.Color(interpolateInferno(0))};
   shader.fragmentShader='uniform float fieldReveal; uniform vec3 coldColor;\n'+shader.fragmentShader.replace('#include <color_fragment>','#include <color_fragment>\n diffuseColor.rgb=mix(coldColor,diffuseColor.rgb,fieldReveal);');
  };
  const cells=new THREE.InstancedMesh(box,material,count),matrix=new THREE.Matrix4(),color=new THREE.Color();
  for(let i=0;i<count;i++){
   const j=i*5;cells.setMatrixAt(i,matrix.makeTranslation(data.values[j+1]-.5,data.values[j+2]-.5,data.values[j+3]-.5));
   cells.setColorAt(i,color.set(interpolateInferno((data.values[j+4]-data.minimum!)/(data.maximum!-data.minimum!||1))));
  }
  scene.add(cells);
  const outlineGeo=new THREE.EdgesGeometry(box),outlineMat=new THREE.LineBasicMaterial({color:0xffffff});
  const outline=new THREE.LineSegments(outlineGeo,outlineMat);outline.scale.setScalar(1.05);scene.add(outline);
  const planeGeo=new THREE.PlaneGeometry(1,1),planeMat=new THREE.MeshBasicMaterial({color:0x5ab0ff,transparent:true,opacity:.14,depthWrite:false,side:THREE.DoubleSide});
  const indicator=new THREE.Mesh(planeGeo,planeMat);scene.add(indicator);
  let raf=0,last=0,interacting=false,lastInput=performance.now(),poseStart=0,animating=false;
  const from=new THREE.Vector3(),to=new THREE.Vector3(),up=new THREE.Vector3(0,1,0);
  function draw(){
   const s=state.current;material.wireframe=s.wire;
   if(!!material.clippingPlanes?.length!==!!s.layer){material.clippingPlanes=s.layer?[plane]:[];material.needsUpdate=true}
   plane.normal.set(0,0,0).setComponent(s.axis,-1);plane.constant=s.layer*size-.5;
   indicator.visible=!!s.layer;indicator.position.set(0,0,0).setComponent(s.axis,plane.constant);
   indicator.rotation.set(s.axis===1?-Math.PI/2:0,s.axis===0?Math.PI/2:0,0);
   outline.visible=s.node!==null;if(s.node!==null){const j=s.node*5;outline.position.set(data.values[j+1]-.5,data.values[j+2]-.5,data.values[j+3]-.5)}
   renderer.render(scene,camera);
  }
  const reduced=window.matchMedia('(prefers-reduced-motion: reduce)');
  function frame(now:number){
   raf=0;if(document.hidden)return;
   const s=state.current,allow=!s.paused&&!reduced.matches;
   const revealing=reveal.value<1;
   if(revealing){const t=allow?Math.min(1,(now-revealStart)/1200):1;reveal.value=1-(1-t)**3;draw()}
   if(animating){
    const t=allow?Math.min(1,(now-poseStart)/600):1;
    camera.position.lerpVectors(from,to,t*t*(3-2*t));orbit.update();draw();animating=t<1;
   }else if(allow&&!interacting&&s.node===null&&!s.layer&&now-lastInput>4000){
    camera.position.applyAxisAngle(up,Math.min(50,now-last)*Math.PI/18000);orbit.update();draw();
   }
   last=now;if(reveal.value<1||animating||(allow&&!interacting&&s.node===null&&!s.layer))raf=requestAnimationFrame(frame);
  }
  function schedule(){if(!raf&&!document.hidden)raf=requestAnimationFrame(frame)}
  wake.current=()=>{lastInput=performance.now();draw();schedule()};
  function start(){interacting=true;animating=false;setHint(false)}
  function stop(){interacting=false;lastInput=performance.now();schedule()}
  orbit.addEventListener('start',start);orbit.addEventListener('end',stop);orbit.addEventListener('change',draw);
  cameraTo.current=name=>{setHint(false);lastInput=poseStart=performance.now();from.copy(camera.position);to.fromArray(poses[name]);animating=true;schedule()};
  let startX=0,startY=0;
  function down(e:PointerEvent){startX=e.clientX;startY=e.clientY}
  function pick(e:PointerEvent){if(Math.hypot(e.clientX-startX,e.clientY-startY)>5)return;
   const r=canvas.getBoundingClientRect(),ray=new THREE.Raycaster();ray.setFromCamera(new THREE.Vector2((e.clientX-r.left)/r.width*2-1,1-(e.clientY-r.top)/r.height*2),camera);
   const s=state.current;const hit=ray.intersectObject(cells).find(h=>h.instanceId!==undefined&&(!s.layer||data.values[h.instanceId*5+1+s.axis]<s.layer*size));
   if(hit?.instanceId!==undefined){setNode(hit.instanceId);setCopied('')}
  }
  const resize=new ResizeObserver(()=>{const w=target.clientWidth,h=target.clientHeight;if(!w||!h)return;renderer.setSize(w,h,false);camera.aspect=w/h;camera.updateProjectionMatrix();draw()});resize.observe(target);
  const visibility=()=>{if(document.hidden){cancelAnimationFrame(raf);raf=0}else{last=performance.now();schedule()}};
  canvas.addEventListener('pointerdown',down);canvas.addEventListener('pointerup',pick);
  document.addEventListener('visibilitychange',visibility);reduced.addEventListener('change',schedule);orbit.update();schedule();
  return ()=>{cancelAnimationFrame(raf);resize.disconnect();document.removeEventListener('visibilitychange',visibility);reduced.removeEventListener('change',schedule);
   canvas.removeEventListener('pointerdown',down);canvas.removeEventListener('pointerup',pick);orbit.dispose();cells.dispose();box.dispose();material.dispose();outlineGeo.dispose();outlineMat.dispose();planeGeo.dispose();planeMat.dispose();renderer.dispose();canvas.remove();wake.current=()=>{};cameraTo.current=()=>{}};
 },[data,count,size]);
 const j=(node??0)*5;
 const reading=node===null?'Select a cell or use Next node':('Node '+data.values[j]+' · '+axes.map((a,i)=>a+' '+data.values[j+i+1].toFixed(3)).join(' · ')+' m · '+data.values[j+4].toFixed(2)+' °C');
 function step(direction:number){const index=node===null?(direction>0?-1:0):displayed.indexOf(node);setNode(displayed[(index+direction+displayed.length)%displayed.length]);setCopied('');setHint(false)}
 return <div className="field-tool"><div className="view-actions">{(Object.keys(poses) as Pose[]).map(name=><button key={name} onClick={()=>cameraTo.current(name)} disabled={fallback}>{name}</button>)}<button aria-pressed={wire} disabled={fallback} onClick={()=>setWire(!wire)}>Wireframe</button></div>
 <div className="field-stage" ref={host}>{hint&&!fallback&&<small>Drag to orbit · tap a cell</small>}{fallback&&<svg viewBox="0 0 100 100" role="img" aria-label="Static field projection with computed cell temperatures">{displayed.map(i=><rect key={i} x={(data.values[i*5+1]-size/2)*100} y={(1-data.values[i*5+2]-size/2)*100} width={size*98} height={size*98} fill={interpolateInferno((data.values[i*5+4]-data.minimum!)/(data.maximum!-data.minimum!||1))}/>)}</svg>}</div>
 {fallback&&<p role="status">WebGL unavailable. Showing the field in projection.</p>}
 <div className="scale"><div style={{background:'linear-gradient(to right,'+Array.from({length:17},(_,i)=>interpolateInferno(i/16)).join(',')+')'}}/>{Array.from({length:5},(_,i)=><span key={i}>{(data.minimum!+(data.maximum!-data.minimum!)*i/4).toFixed(1)}{i===4?' °C':''}</span>)}</div>
 <div className="slice"><fieldset><legend>Slice axis</legend><div className="segments">{axes.map((a,i)=><label key={a}><input type="radio" name="slice-axis" checked={axis===i} onChange={()=>{setAxis(i);setNode(null);setCopied('')}}/><span>{a.toUpperCase()}</span></label>)}</div></fieldset><label>Slice position <output>{layer?(layer*size).toFixed(3)+' m':'Full field'}</output><input type="range" min="0" max={n} value={layer} onChange={e=>{setLayer(+e.target.value);setNode(null)}}/></label></div>
 <div className="inspector" onKeyDown={e=>{if(e.key==='ArrowRight'||e.key==='ArrowLeft'){e.preventDefault();step(e.key==='ArrowRight'?1:-1)}}}>{node===null&&<p>Tap a cell or step through the visible nodes.</p>}<div className="view-actions"><button aria-label="Previous node" onClick={()=>step(-1)}>← Previous node</button><button aria-label="Next node" onClick={()=>step(1)}>Next node →</button>{node!==null&&<button onClick={()=>{setNode(null);setCopied('')}}>Clear</button>}</div>{node!==null&&<button className="readout" title="Copy node reading" onClick={async()=>{try{await navigator.clipboard.writeText(reading);setCopied('Copied')}catch{setCopied('Copy unavailable')}}}>{reading}</button>}<small role="status">{copied}</small></div></div>
}
