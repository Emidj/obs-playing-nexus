// Deterministic execution of the preserved DOM renderer with its real frame()
// callback. This tests legacy behavior, not native rendering certification.
const fs=require('fs'),path=require('path'),assert=require('assert');
const {pathToFileURL}=require('url'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../..'),folder=path.join(root,'outputs/phase12-reference');
const catalogue=JSON.parse(fs.readFileSync(path.join(folder,'catalogue.json'),'utf8'));
async function main(){
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const results=[];let checks=0;
 try{
  const page=await browser.newPage({viewport:{width:720,height:500}});
  await page.addInitScript(()=>{
   let now=0,id=0;const callbacks=new Map();
   Object.defineProperty(performance,'now',{value:()=>now});
   window.requestAnimationFrame=callback=>{callbacks.set(++id,callback);return id;};
   window.cancelAnimationFrame=key=>callbacks.delete(key);
   window.auditTick=time=>{if(time<now)throw Error('Non-monotonic test clock');now=time;const pending=[...callbacks.values()];callbacks.clear();pending.forEach(callback=>callback(now));};
  });
  await page.goto(pathToFileURL(path.join(root,'obs-playing/designs.html')).href);
  await page.evaluate(()=>document.fonts.ready);
  let start=0;
  const originals=catalogue.catalogue.filter(p=>p.renderer==='legacy-local-DOM');
  assert.equal(originals.length,21);checks++;
  for(const preset of originals){
   start+=20000;
   const config={preset:preset.id,width:720,original:true,title:'First Track',artist:'First Artist',label:'NOW PLAYING',visible:true,gap:12,speed:40,pause:2,opacity:1,transition:'fade',duration:800,restart:0};
   const tick=async time=>page.evaluate(t=>window.auditTick(t),time);
   const send=async payload=>page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),payload);
   const inspect=()=>page.evaluate(()=>window.obsPlayingInspect());
   await tick(start);await send(config);await tick(start);
   assert.equal((await inspect()).shown.title,'First Track');checks++;
   await send({...config,title:'Jóga 🎵 日本語',artist:'Björk'});await tick(start+399);
   assert.equal((await inspect()).shown.title,'First Track');checks++;
   await tick(start+400);const midpoint=await inspect();
   assert.equal(midpoint.shown.title,'Jóga 🎵 日本語');checks++;
   assert.equal(midpoint.rows[1].opacity,'0');checks++;
   assert.equal(midpoint.rows[0].opacity,'1');checks++;
   await tick(start+800);assert.equal((await inspect()).transition,false);checks++;
   const longTitle='Long track '.repeat(40);
   await send({...config,title:longTitle});await tick(start+1200);await tick(start+1600);await tick(start+4600);
   const scrolling=await inspect();assert(scrolling.rows[1].full>scrolling.rows[1].width);checks++;
   assert(!scrolling.rows[1].transform.startsWith('translateX(0px)'));checks++;
   await send({...config,title:'',artist:''});await tick(start+5000);await tick(start+5400);
   assert.equal((await inspect()).shown.title,'');checks++;
   await send({...config,visible:false});
   const hidden=await page.evaluate(()=>({visibility:getComputedStyle(document.getElementById('op-design-lab')).visibility,states:document.getElementById('background').getAnimations({subtree:true}).map(a=>a.playState)}));
   assert.equal(hidden.visibility,'hidden');checks++;
   assert(hidden.states.length>0&&hidden.states.every(s=>s==='paused'));checks++;
   await send({...config,title:'Resumed',visible:true});await tick(start+5500);
   assert.equal((await inspect()).shown.title,'Resumed');checks++;
   await send({...config,title:'Intermediate'});await tick(start+5700);
   await send({...config,title:'Latest'});await tick(start+5900);
   assert.equal((await inspect()).shown.title,'Latest');checks++;
   await tick(start+6300);assert.equal((await inspect()).transition,false);checks++;
   await send({...config,title:'Incoming'});await tick(start+6700);
   await send({...config,title:'Queued after swap'});await tick(start+7100);
   assert.equal((await inspect()).shown.title,'Incoming');checks++;
   assert.equal((await inspect()).transition,true);checks++;
   await tick(start+7500);assert.equal((await inspect()).shown.title,'Queued after swap');checks++;
   await tick(start+7900);assert.equal((await inspect()).transition,false);checks++;
   results.push({id:preset.id,unicode:true,midpointSwap:true,labelUnchanged:true,scroll:true,empty:true,hiddenCssPaused:true,resume:true,latestBeforeSwap:true,queuedAfterSwap:true});
  }
 }finally{await browser.close();}
 fs.writeFileSync(path.join(folder,'runtime.json'),JSON.stringify({checks,presets:results,scope:'Original DOM runtime only; native renderer integration pending'},null,2)+'\n');
 console.log(`PASS ${checks} original runtime checks`);
}
main().catch(e=>{console.error(e);process.exitCode=1;});
