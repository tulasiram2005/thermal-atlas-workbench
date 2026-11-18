import {chromium} from '@playwright/test';
import {createServer} from 'node:http';
import {readFileSync,existsSync,mkdirSync,writeFileSync} from 'node:fs';
import {resolve,extname} from 'node:path';
const output='../results/browser-demo';mkdirSync(output,{recursive:true});
const config=JSON.parse(readFileSync('../vercel.json'));const root=resolve('dist-demo');
const server=createServer((req,res)=>{const p=resolve(root,'.'+new URL(req.url,'http://localhost').pathname);const file=p===root?resolve(root,'index.html'):p;if(!file.startsWith(root+'/')||!existsSync(file)){res.writeHead(404).end();return}for(const h of config.headers[0].headers)res.setHeader(h.key,h.value);res.setHeader('Content-Type',({'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.wasm':'application/wasm','.svg':'image/svg+xml','.json':'application/json'})[extname(file)]||'application/octet-stream');res.end(readFileSync(file))});
await new Promise(r=>server.listen(4186,'127.0.0.1',r));
let browser;const report={passed:false,checks:[],errors:[],externalRequests:[],axe:[]};
try{
 browser=await chromium.launch({channel:'chrome',headless:true});const context=await browser.newContext({acceptDownloads:true});const page=await context.newPage();
 const base=process.env.DEMO_URL||'http://127.0.0.1:4186';
 page.on('pageerror',e=>report.errors.push(String(e)));page.on('request',r=>{if(!r.url().startsWith(base)&&!r.url().startsWith('blob:')&&!r.url().startsWith('data:'))report.externalRequests.push(r.url())});
 await page.goto(base);await page.getByRole('button',{name:'Compare solvers'}).click();await page.getByText(/152 cells.*3 of 3 solvers converged/).waitFor();report.checks.push('Worker calculation under application CSP');
 await page.getByRole('button',{name:'Next node'}).click();
 await page.getByRole('button',{name:/Node 0 · x/}).waitFor();
 await page.getByRole('button',{name:'Next node'}).press('ArrowRight');
 await page.getByRole('button',{name:/Node 1 · x/}).waitFor();
 await page.getByRole('radio',{name:/Reference cube/}).check();
 await page.getByRole('radio',{name:'12',exact:true}).check();
 await page.getByText('Settings changed — compare again').waitFor();
 if(await page.getByRole('button',{name:/Export evidence/}).isEnabled())throw Error('Stale export enabled');
 report.checks.push('Keyboard node inspection and stale export guard');await page.getByRole('button',{name:'Compare solvers'}).click();await page.getByText(/1728 cells.*3 of 3 solvers converged/).waitFor();report.checks.push('Largest public cube grid');
 const [download]=await Promise.all([page.waitForEvent('download'),page.getByRole('button',{name:/Export evidence/}).click()]);await download.saveAs(output+'/export.json');report.checks.push('JSON export');
 for(const width of [360,768,1024,1440,1920])for(const theme of ['dark','light']){await page.setViewportSize({width,height:1000});const switcher=page.getByRole('button',{name:`Switch to ${theme} theme`});if(await switcher.count())await switcher.click();await page.screenshot({path:`${output}/${width}-${theme}.png`,fullPage:true});if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth))throw Error('Horizontal overflow');}
 report.checks.push('Five widths in both themes');
 // Accessibility injection is isolated from the real-CSP calculation context.
 const audit=await browser.newContext({bypassCSP:true});const a=await audit.newPage();await a.goto(base);await a.addScriptTag({path:'node_modules/axe-core/axe.min.js'});report.axe=await a.evaluate(async()=> (await window.axe.run()).violations.map(v=>({id:v.id,impact:v.impact,nodes:v.nodes.length})));
 report.passed=!report.errors.length&&!report.externalRequests.length&&!report.axe.length;
}catch(e){report.failure=String(e)}finally{if(browser)await browser.close();server.close();writeFileSync(output+'/report.json',JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));if(!report.passed)process.exitCode=1;}
