import {chromium} from 'playwright';
import lighthouse from 'lighthouse';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {once} from 'node:events';
import {mkdir,writeFile,mkdtemp,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {resolve} from 'node:path';
import {createRequire} from 'node:module';

const label=process.argv[2] || 'baseline';
if(!/^[a-z0-9-]+$/.test(label)) throw Error('Invalid iteration label');
const root=resolve('..');
const out=resolve(root,'results/design',label);
await mkdir(out,{recursive:true});
const profile=await mkdtemp(resolve(tmpdir(),'wb-chrome-'));
const host=spawn(resolve(root,'build-release/browser_host'),[resolve('dist')],{stdio:['pipe','pipe','inherit']});
const lines=createInterface({input:host.stdout});
const report={views:[],requests:[],external:[],complete:false};
let context;
try {
 const [url]=await Promise.race([once(lines,'line'),once(host,'exit').then(()=>{throw Error('Host exited')})]);
 const origin=new URL(url).origin;
 context=await chromium.launchPersistentContext(profile,{channel:'chrome',headless:true,args:['--remote-debugging-port=0']});
 const page=await context.newPage();
 page.on('request',r=>{
   const parsed=new URL(r.url());
   if(parsed.protocol==='http:'||parsed.protocol==='https:'){
     report.requests.push(parsed.pathname);
     if(parsed.origin!==origin)report.external.push(parsed.origin);
   }
 });
 await page.goto(url);
 for(const [width,height] of [[1920,1080],[1440,900],[1280,720],[768,1024]]){
  await page.setViewportSize({width,height});
  for(const theme of ['dark','light']){
   if(await page.locator('.app.light').count()!==Number(theme==='light')) await page.getByRole('button',{name:/theme$/}).click();
   for(const view of ['Geometry','Thermal','Performance','Hardware','Backprop','Verification','About']){
    await page.getByRole('navigation').getByRole('button',{name:new RegExp(view)}).click();
    if(view==='Geometry'){
      await page.getByLabel('Input file').setInputFiles(resolve(root,'data/thermal_rod.csv'));
      await page.locator('.geometry-viewport canvas').waitFor();
    } else if(view!=='About'){
      await page.locator('button.primary').click();
      const ready={Thermal:'table',Verification:'table',Performance:'section',Hardware:'article',Backprop:'table'};
      await page.locator('.canvas '+ready[view]).first().waitFor();
      await page.waitForFunction(()=>!document.querySelector('button.primary')?.disabled);
    }
    await page.screenshot({path:resolve(out,`${view}-${theme}-${width}x${height}.png`)});
    await page.evaluate(createRequire(import.meta.url)('axe-core').source);
    const violations=await page.evaluate(async()=> (await window.axe.run(document,{runOnly:['wcag2a','wcag2aa','wcag21aa']})).violations.map(v=>({id:v.id,impact:v.impact,targets:v.nodes.map(n=>n.target)})));
    report.views.push({view,theme,width,height,violations});
   }
  }
 }
 const {readFile}=await import('node:fs/promises');
 const port=Number((await readFile(resolve(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);
 const lh=await lighthouse(origin+'/',{port,disableStorageReset:true,onlyCategories:['accessibility','performance'],output:'json',logLevel:'error',formFactor:'desktop',screenEmulation:{mobile:false,width:1440,height:900,deviceScaleFactor:1,disabled:false}});
 if(lh.lhr.runtimeError||lh.lhr.finalDisplayedUrl!==origin+'/')throw Error('Lighthouse failed to audit the authenticated shell');
 report.lighthouse=Object.fromEntries(Object.entries(lh.lhr.categories).map(([k,v])=>[k,v.score]));
 report.complete=true;
} catch(e){report.error=e.message.split('\n')[0].replace(/token=[^&\s]+/g,'token=redacted');process.exitCode=1}
finally{
 await writeFile(resolve(out,'audit.json'),JSON.stringify(report,null,2));
 console.log(out,report.views.length,'screens captured',report.error||'');
 await context?.close();lines.close();host.stdin.end('\n');await rm(profile,{recursive:true,force:true});
}
