import {useState} from 'react';
import {createRoot} from 'react-dom/client';
import SolverLab from './SolverLab';
import {calculateInBrowser} from './browser-solver';
import './style.css';
function Demo(){
 const [light,setLight]=useState(false);
 return <div className={light?'app light':'app'} style={{minHeight:'100vh',padding:'clamp(16px,4vw,48px)'}}><header style={{display:'flex',justifyContent:'space-between',gap:16,flexWrap:'wrap',height:'auto',padding:24,marginBottom:24}}><div><h1>Thermal Atlas</h1><p>Interactive 3D heat-conduction solver comparison</p></div><div><button onClick={()=>{setLight(!light);document.documentElement.dataset.theme=light?'dark':'light'}}>Switch to {light?'dark':'light'} theme</button> <a href="https://github.com/tulasiram2005/thermal-atlas-workbench">Source & desktop app</a></div></header><main><SolverLab calculate={calculateInBrowser}/></main><footer><p>This browser edition contains Solver Lab. Geometry viewing, thermal calculation and evidence export run on your device. The full desktop workbench includes the additional rod, batch, hardware-simulation and verification modules.</p><p>Browser timings depend on your device and are not the native benchmark results. No uploaded data, account, database or server-side calculation is required.</p></footer></div>
}
createRoot(document.getElementById('root')!).render(<Demo/>);
