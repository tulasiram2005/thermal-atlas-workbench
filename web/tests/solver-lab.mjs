import {chromium} from '@playwright/test';
import {spawn} from 'node:child_process';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const out=resolve('../results/extension');await mkdir(out,{recursive:true});
const host=spawn(resolve('../build-extension/browser_host'),[resolve('dist')]);
const report={auditScope:"Test browser bypasses CSP only to inject axe; application CSP unchanged",passed:false,checks:[],violations:[],externalRequests:[],errors:[]};let browser;
try{
 const url=await new Promise((yes,no)=>{let text='';const t=setTimeout(()=>no(Error('Host timeout')),10000);host.stdout.on('data',c=>{text+=c;if(text.includes('\n')){clearTimeout(t);yes(text.trim())}});host.on('error',no)});
 browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({bypassCSP:true});
 page.on('request',r=>{if(!r.url().startsWith('http://127.0.0.1:'))report.externalRequests.push(new URL(r.url()).origin)});
 page.on('pageerror',e=>report.errors.push(e.message));
 await page.goto(url);await page.getByRole('button',{name:'Compare solvers',exact:true}).click();
 await page.getByRole('table',{name:/Solver evidence/}).waitFor();
 if(!(await page.getByRole('status').filter({hasText:'of 3 solvers converged'}).innerText()).includes('3 of 3'))throw Error('Not all methods converged');
 report.checks.push('Three methods converge');
 for(const width of [1440,768])for(const theme of ['dark','light']){
  await page.setViewportSize({width,height:1000});
  if(theme==='light')await page.getByRole('button',{name:'Light theme',exact:true}).click();
  await page.addScriptTag({content:await readFile('node_modules/axe-core/axe.min.js','utf8')});
  report.violations.push({width,theme,violations:await page.evaluate(async()=> (await window.axe.run()).violations.map(v=>({id:v.id,impact:v.impact})))});
  if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth))throw Error('Overflow');
  await page.screenshot({path:resolve(out,`solver-${width}-${theme}.png`),fullPage:true});
  if(theme==='light')await page.getByRole('button',{name:'Dark theme',exact:true}).click();
 }
 const [download]=await Promise.all([page.waitForEvent('download'),page.getByRole('button',{name:'Export evidence'}).click()]);
 await download.saveAs(resolve(out,'browser-export.json'));report.checks.push('Export downloaded');
 report.passed=!report.errors.length&&!report.externalRequests.length&&report.violations.every(x=>!x.violations.length);
 if(!report.passed)process.exitCode=1;
}catch(e){report.error=String(e).replace(/token=[^\s&]+/g,'token=REDACTED');process.exitCode=1;}
finally{await browser?.close();host.stdin.end('\n');await writeFile(resolve(out,'browser-check.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2))}
