import {chromium,webkit} from '@playwright/test';
import {spawn} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const root=resolve('..');
const child=spawn(resolve(root,'build-release/browser_host'),[resolve('dist')],{stdio:['pipe','pipe','pipe']});
let browser;
const report={checks:[],externalRequests:[],consoleErrors:[],failedResponses:[],passed:false};
try {
 const url=await new Promise((ok,fail)=>{let text='';const timeout=setTimeout(()=>fail(new Error('Host startup timeout')),10000);child.stdout.on('data',chunk=>{text+=chunk;if(text.includes('\n')){clearTimeout(timeout);ok(text.trim())}});child.once('error',fail)});
 browser=process.env.WB_BROWSER==='webkit' ? await webkit.launch() : await chromium.launch({channel:process.env.WB_BROWSER==='bundled'?undefined:'chrome',headless:true});
 const page=await browser.newPage({viewport:{width:1440,height:900}});
 page.on('request',req=>{if(!req.url().startsWith('http://127.0.0.1:'))report.externalRequests.push(new URL(req.url()).origin)});
 page.on('response',response=>{if(response.status()>=400)report.failedResponses.push({path:new URL(response.url()).pathname,status:response.status()})});
 page.on('pageerror',err=>report.consoleErrors.push(err.message));
 page.on('console',msg=>{if(msg.type()==='error')report.consoleErrors.push(msg.text().replace(/token=[^&\s]+/g,'token=REDACTED'))});
 await mkdir(resolve(root,'results/screenshots'),{recursive:true});
 await page.goto(url);
 await page.getByRole('heading',{name:'Thermal analysis',exact:true}).waitFor();
 if(new URL(page.url()).search)throw new Error('Session token remained in URL');
 report.checks.push('Authenticated shell loaded; URL cleaned');
 await page.getByRole('button',{name:'Run experiment',exact:true}).click();
 await page.getByRole('table',{name:'Computed nodal temperatures'}).waitFor();
 if(!(await page.getByRole('table',{name:'Computed nodal temperatures'}).innerText()).includes('100.0000'))throw new Error('Thermal result missing');
 report.checks.push('Thermal calculation and table');
 await page.screenshot({path:resolve(root,'results/screenshots/thermal-dark.png'),fullPage:true});
 await page.getByRole('button',{name:'Light theme'}).click();
 await page.screenshot({path:resolve(root,'results/screenshots/thermal-light.png'),fullPage:true});
 for(const name of ['Geometry','Performance','Hardware','Backprop','Verification','About']){
  await page.getByRole('navigation').getByRole('button',{name:new RegExp(name)}).click();
  if(name==='Geometry'){
   await page.getByLabel('Input file').setInputFiles(resolve(root,'data/thermal_rod.csv'));
   await page.getByRole('table',{name:'Parsed geometry statistics'}).waitFor();
  } else if(['Hardware','Backprop','Performance'].includes(name)){
   await page.getByRole('button',{name:'Run experiment',exact:true}).click();
   await page.getByRole('button',{name:'Run experiment',exact:true}).waitFor();
   await page.waitForFunction(()=>!document.querySelector('button.primary')?.disabled);
   if(name==='Performance')await page.getByRole('heading',{name:'Batch complete'}).waitFor();
  } else if(name==='Verification'){
   await page.getByRole('button',{name:'Verify core modules'}).click();
   await page.getByRole('table',{name:'Measured core checks'}).waitFor();
  }
  await page.screenshot({path:resolve(root,`results/screenshots/${name.toLowerCase()}.png`),fullPage:true});
  report.checks.push(name+' navigation/action');
 }
 await page.setViewportSize({width:768,height:1024});
 await page.getByRole('navigation').getByRole('button',{name:/Thermal/}).click();
 await page.screenshot({path:resolve(root,'results/screenshots/tablet.png'),fullPage:true});
 const overflow=await page.evaluate(()=>document.documentElement.scrollWidth>window.innerWidth);
 if(overflow)throw new Error('Tablet horizontal overflow');
 report.checks.push('Tablet viewport without horizontal overflow');
 if(report.externalRequests.length||report.consoleErrors.length)throw new Error('Network or console check failed');
 report.passed=true;
} catch(error){report.failure=error.message.replace(/token=[^&\s]+/g,'token=REDACTED');process.exitCode=1;}
finally{if(browser)await browser.close();child.stdin.end('\n');await writeFile(resolve(root,'results/browser-smoke.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));}
