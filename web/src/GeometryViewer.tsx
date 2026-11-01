import {useEffect,useRef,useState} from 'react';
import * as THREE from 'three';
import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';
import {interpolateInferno} from 'd3-scale-chromatic';
export type GeometryData={kind:'stl'|'csv',values:Float64Array,minimum?:number,maximum?:number};
export default function GeometryViewer({data}:{data:GeometryData}){
 const container=useRef<HTMLDivElement>(null),fit=useRef(()=>{});
 const [error,setError]=useState(''),[selected,setSelected]=useState('');
 useEffect(()=>{
  const host=container.current;if(!host)return;
  setError('');setSelected('');
  let renderer:THREE.WebGLRenderer;
  try{renderer=new THREE.WebGLRenderer({antialias:true})}catch{setError('WebGL is unavailable. Parsed file statistics remain available below.');return}
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));host.appendChild(renderer.domElement);
  renderer.domElement.setAttribute('aria-label','3D geometry: drag to rotate, scroll to zoom. Use Fit to view to reset.');
  const scene=new THREE.Scene();scene.background=new THREE.Color('#0d1424');
  const camera=new THREE.PerspectiveCamera(45,1,.001,1000);
  const controls=new OrbitControls(camera,renderer.domElement);
  const geometry=new THREE.BufferGeometry();
  const stride=data.kind==='stl'?3:5,count=data.values.length/stride;
  const positions=new Float32Array(count*3),colors=new Float32Array(count*3);
  for(let i=0;i<count;i++){
   const offset=i*stride+(data.kind==='csv'?1:0);
   positions.set(data.values.subarray(offset,offset+3),i*3);
   if(data.kind==='csv'){
    const t=(data.values[i*5+4]-(data.minimum??0))/((data.maximum??1)-(data.minimum??0)||1);
    const color=new THREE.Color(interpolateInferno(Math.max(0,Math.min(1,t))));color.toArray(colors,i*3);
   }
  }
  geometry.setAttribute('position',new THREE.BufferAttribute(positions,3));geometry.computeBoundingBox();
  const center=new THREE.Vector3(),size=new THREE.Vector3();geometry.boundingBox!.getCenter(center);geometry.boundingBox!.getSize(size);
  const scale=Math.max(size.x,size.y,size.z)||1;
  geometry.translate(-center.x,-center.y,-center.z);geometry.scale(1/scale,1/scale,1/scale);
  let object:THREE.Mesh|THREE.Points;let material:THREE.Material;
  if(data.kind==='stl'){
   geometry.computeVertexNormals();material=new THREE.MeshStandardMaterial({color:0x91b9d6,side:THREE.DoubleSide,roughness:.65});object=new THREE.Mesh(geometry,material);
   scene.add(new THREE.HemisphereLight(0xffffff,0x334466,2));const light=new THREE.DirectionalLight(0xffffff,3);light.position.set(2,3,4);scene.add(light);
  }else{geometry.setAttribute('color',new THREE.BufferAttribute(colors,3));material=new THREE.PointsMaterial({size:.018,vertexColors:true});object=new THREE.Points(geometry,material)}
  scene.add(object);
  const render=()=>renderer.render(scene,camera);
  fit.current=()=>{data.kind==='csv'?camera.position.set(0,0,1.4):camera.position.set(1.5,1,1.5);controls.target.set(0,0,0);controls.update();render()};fit.current();
  const resize=new ResizeObserver(()=>{const width=host.clientWidth;renderer.setSize(width,360);camera.aspect=width/360;camera.updateProjectionMatrix();render()});resize.observe(host);
  controls.addEventListener('change',render);
  const pick=(event:PointerEvent)=>{
   if(data.kind!=='csv')return;
   const rect=renderer.domElement.getBoundingClientRect();const ray=new THREE.Raycaster();ray.params.Points.threshold=.025;
   ray.setFromCamera(new THREE.Vector2((event.clientX-rect.left)/rect.width*2-1,-(event.clientY-rect.top)/rect.height*2+1),camera);
   const hit=ray.intersectObject(object)[0];if(hit?.index!==undefined){const i=hit.index*5;setSelected(`Node ${data.values[i]} · (${data.values[i+1]}, ${data.values[i+2]}, ${data.values[i+3]}) m · ${data.values[i+4]} °C`)}
  };
  renderer.domElement.addEventListener('pointerdown',pick);
  return()=>{resize.disconnect();controls.removeEventListener('change',render);controls.dispose();renderer.domElement.removeEventListener('pointerdown',pick);geometry.dispose();material.dispose();renderer.dispose();renderer.domElement.remove();fit.current=()=>{}};
 },[data]);
 return <section aria-label="Geometry viewer"><button onClick={()=>fit.current()}>Fit to view</button><p>{data.kind==='stl'?'STL is unitless; display uses source coordinates.':'Coordinates in metres; click a point to inspect its node.'}</p>{error&&<p role="alert">{error}</p>}<div ref={container} className="geometry-viewport"/>{data.kind==='csv'&&<><svg className="geometry-legend" viewBox="0 0 400 40" role="img" aria-label={`Inferno temperature legend from ${data.minimum} to ${data.maximum} degrees Celsius`}>{Array.from({length:100},(_,i)=><rect key={i} x={i*4} y="0" width="4" height="16" fill={interpolateInferno(i/99)}/>)}<text x="0" y="35" fill="currentColor" fontSize="12">{data.minimum?.toFixed(2)} °C</text><text x="400" y="35" textAnchor="end" fill="currentColor" fontSize="12">{data.maximum?.toFixed(2)} °C</text></svg><p role="status">{selected||'No node selected.'}</p></>}</section>
}
