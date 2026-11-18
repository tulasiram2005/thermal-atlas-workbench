import {chromium} from '@playwright/test';
import lighthouse from 'lighthouse';
import {createServer} from 'node:http';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {extname, resolve} from 'node:path';

const stage = process.argv[2];
if (!['before','after'].includes(stage)) throw Error('Choose before or after');
const out = resolve('../results/ui-review',stage+'-evidence');
mkdirSync(out,{recursive:true});
const root = resolve(stage==='before'?'../results/ui-review/before':'dist-demo');
const headers = JSON.parse(readFileSync('../vercel.json')).headers[0].headers;
const server = createServer((req,res)=>{
  const path = resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
  try {
    if(path!==root && !path.startsWith(root+'/')) throw Error();
    const file=path===root?resolve(root,'index.html'):path;
    const body=readFileSync(file);
    for(const h of headers) res.setHeader(h.key,h.value);
    res.setHeader('Content-Type',({'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.wasm':'application/wasm'})[extname(file)]||'application/octet-stream');
    res.end(body);
  } catch {res.writeHead(404).end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const base='http://127.0.0.1:'+server.address().port;
let browser;
try {
  browser=await chromium.launch({channel:'chrome',args:['--remote-debugging-port=9338']});
  const page=await browser.newPage({viewport:{width:360,height:800},deviceScaleFactor:1});
  const errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  page.on('console',m=>{if(m.type()==='error')errors.push(m.text())});
  await page.goto(base);
  await page.getByRole('button',{name:'Compare solvers'}).click();
  await page.getByText(/152 cells.*3 of 3 solvers converged/i).first().waitFor();
  const canvas=page.locator('canvas').first();
  await canvas.scrollIntoViewIfNeeded();
  // Count instanced field draws during the same sequence of pointer movements.
  await page.evaluate(()=>{
    const proto=WebGL2RenderingContext.prototype, draw=proto.drawElementsInstanced;
    window.frameTimes=[];
    proto.drawElementsInstanced=function(...args){if(args[4]===152)window.frameTimes.push(performance.now());return draw.apply(this,args)};
  });
  const box=await canvas.boundingBox(), x=box.x+box.width/2,y=box.y+box.height/2;
  await page.mouse.move(x,y); await page.mouse.down();
  for(let i=0;i<180;i++){await page.mouse.move(x+Math.sin(i/18)*60,y); await page.waitForTimeout(16);}
  await page.mouse.up();
  const fps=await page.evaluate(()=>{const t=window.frameTimes;return t.length>1?(t.length-1)*1000/(t.at(-1)-t[0]):null});
  if(fps===null)throw Error('No field draws measured');
  for(const width of [360,768,1024,1440,1920]) {
    await page.setViewportSize({width,height:width===360?800:1000});
    for(const theme of ['dark','light']){
      const toggle=page.getByRole('button',{name:'Switch to '+theme+' theme'});
      if(await toggle.count())await toggle.click();
      if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth)) throw Error('Horizontal overflow at '+width);
      if([360,1440].includes(width))await page.screenshot({path:out+'/'+width+'-'+theme+'.png',fullPage:true});
    }
  }
  const lh=await lighthouse(base,{port:9338,logLevel:'error',onlyCategories:['performance','accessibility']});
  writeFileSync(out+'/lighthouse.json',JSON.stringify(lh.lhr));
  const report={browser:browser.version(),fps,method:'360px desktop Chrome, scripted drag, 152 instanced cells',performance:lh.lhr.categories.performance.score*100,accessibility:lh.lhr.categories.accessibility.score*100,errors};
  writeFileSync(out+'/summary.json',JSON.stringify(report,null,2));
  console.log(report);
} finally {await browser?.close();server.closeAllConnections();server.close();}
