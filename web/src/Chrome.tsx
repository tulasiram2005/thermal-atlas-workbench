import {useEffect,useState} from 'react';
// Vite embeds this project-owned SVG so its slabs can respond to hover.
import mark from '../public/brand/block.svg?raw';

export const source='https://github.com/tulasiram2005/thermal-atlas-workbench';
type IconName='link'|'github'|'linkedin'|'portfolio'|'pause'|'play';
export function Icon({name}:{name:IconName}){
 const paths:Record<IconName,string>={
  link:'M14 4h6v6 M20 4l-9 9 M10 5H5v14h14v-5',
  github:'M9 20v-3c-3 .8-4-1-5-3 M15 20v-3c0-1-.3-1.6-.8-2 3-.3 5.1-1.7 5.1-5.1a4 4 0 0 0-1.1-2.8c.2-.8.2-1.8-.2-2.8-1-.2-2.3.5-3.1 1a11 11 0 0 0-5.9 0c-.9-.6-2-1.1-3.1-1-.4 1-.4 2-.2 2.8a4 4 0 0 0-1.1 2.8c0 3.4 2.1 4.8 5.2 5.1-.6.4-.8 1.1-.8 2',
  linkedin:'M4 9v11 M4 4v1 M10 20V9 M10 14c0-6 9-7 9 0v6',
  portfolio:'M3 8h18v12H3z M8 8V5h8v3 M3 13q9 4 18 0',
  pause:'M8 5v14 M16 5v14',play:'M7 4l13 8-13 8z'
 };
 return <svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d={paths[name]}/></svg>;
}
export function Wordmark(){return <strong className="word"><span className="brand-mark" dangerouslySetInnerHTML={{__html:mark}}/>Thermal Atlas</strong>}
export function FooterBar(){
 const [scrolled,setScrolled]=useState(false);
 useEffect(()=>{const sync=()=>setScrolled(scrollY>300);sync();addEventListener('scroll',sync,{passive:true});return ()=>removeEventListener('scroll',sync)},[]);
 return <div className="footer-bar"><div className="social">{([
  ['linkedin','LinkedIn','https://www.linkedin.com/in/tulasi-ram-bc-6787a1287/'],
  ['github','GitHub','https://github.com/tulasiram2005'],
  ['portfolio','Portfolio','https://tulasi-ram-portfolio.vercel.app']
 ] as const).map(([name,label,url])=><a key={name} href={url} aria-label={label} title={label}><Icon name={name}/></a>)}</div>
 {scrolled&&<button onClick={()=>{document.querySelector<HTMLElement>('h1')?.focus({preventScroll:true});window.scrollTo({top:0,behavior:matchMedia('(prefers-reduced-motion: reduce)').matches?'instant':'smooth'})}}>↑ Back to top</button>}
 <small>©2026 Bajantri Chikulaguri Tulasi Ram, all rights reserved</small></div>;
}
