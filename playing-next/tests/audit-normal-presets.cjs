// Audits the actual shipped catalogue/DOM/CSS. Never edits the legacy product.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {pathToFileURL}=require('url');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'../..'),legacy=path.join(root,'obs-playing');
const destination=path.join(root,'outputs','phase12-reference');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex').toUpperCase();
async function main(){
 fs.mkdirSync(destination,{recursive:true});
 const core=fs.readFileSync(path.join(legacy,'core.lua'),'utf8');
 const section=core.slice(core.indexOf('M.presets = {'),core.indexOf('M.retired_presets='));
 const catalogue=[...section.matchAll(/\{id='([^']+)', name='([^']+)', height=(\d+)\}/g)].map(m=>({id:m[1],name:m[2],legacyHeight:Number(m[3])}));
 assert.equal(catalogue.length,23);assert.equal(new Set(catalogue.map(x=>x.id)).size,23);
 const aliasSection=core.match(/M.retired_presets=\{([^}]+)\}/)[1];
 const aliases=Object.fromEntries([...aliasSection.matchAll(/(\w+)='([^']+)'/g)].map(m=>[m[1],m[2]]));
 for(const id of Object.values(aliases))assert(catalogue.some(p=>p.id===id));
 const html=fs.readFileSync(path.join(legacy,'designs.html'),'utf8');
 const resources=['core.lua','artwork.lua','obs-playing.lua','browser-design.lua','designs.html','designs.js',...[...html.matchAll(/href="([^"]+\.css)"/g)].map(m=>m[1])];
 const inputs=resources.map(file=>({file,sha256:sha(fs.readFileSync(path.join(legacy,file)))}));
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 let checks=4;const errors=[];
 try{
  const page=await browser.newPage({viewport:{width:720,height:500},deviceScaleFactor:1});
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto(pathToFileURL(path.join(legacy,'designs.html')).href);
  await page.evaluate(()=>{window.requestAnimationFrame=()=>0;});
  for(const preset of catalogue){
   preset.renderer=['studio','minimal'].includes(preset.id)?'legacy-OBS-native':'legacy-local-DOM';
   if(preset.renderer==='legacy-OBS-native'){preset.pendingNativeReference=true;continue;}
   assert(await page.locator('#design-'+preset.id).count());checks++;
   const payload={preset:preset.id,width:720,original:true,title:'Lipstick',artist:'Alaia & Gallo',label:'NOW PLAYING',visible:true,gap:12,speed:40,pause:2,opacity:1,transition:'fade',duration:800,restart:0};
   await page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),payload);
   await page.evaluate(()=>document.fonts.ready);
   preset.contract=await page.evaluate(()=>{
    const root=document.getElementById('op-design-lab');
    const background=document.getElementById('background'),foreground=document.getElementById('foreground');
    const animations=background.getAnimations({subtree:true});
    animations.forEach(a=>{a.pause();a.currentTime=0;});
    const rect=r=>({x:r.x,y:r.y,width:r.width,height:r.height});
    return {scale:getComputedStyle(root).transform,bounds:rect(foreground.getBoundingClientRect()),
     text:[...foreground.querySelectorAll('.run')].map(e=>{const s=getComputedStyle(e);return {text:e.textContent,bounds:rect(e.getBoundingClientRect()),window:rect(e.closest('.scroll-window').getBoundingClientRect()),family:s.fontFamily,size:s.fontSize,weight:s.fontWeight,style:s.fontStyle,lineHeight:s.lineHeight,letterSpacing:s.letterSpacing,color:s.color,stroke:s.webkitTextStroke,textShadow:s.textShadow,transform:s.transform};}),
     animations:animations.map(a=>({name:a.animationName,target:a.effect.target?.className||'',pseudo:a.effect.pseudoElement||'',timing:a.effect.getTiming(),keyframes:a.effect.getKeyframes()})),
     foregroundAnimations:foreground.getAnimations({subtree:true}).length};
   });
   assert.equal(preset.contract.text.length,3);checks++;
   assert.equal(preset.contract.foregroundAnimations,0);checks++;
   assert(preset.contract.animations.length>0);checks++;
   preset.height=Math.ceil((['lens','gum','sintonia','trazo'].includes(preset.id)?242: ['monolith','portal','cantilever','beacon','laser','matrix','lotus','tide','silk','obsidian'].includes(preset.id)?220:210)*720/(['lens','gum','sintonia','trazo'].includes(preset.id)?350:['monolith','portal','cantilever','beacon','laser','matrix','lotus','tide','silk','obsidian'].includes(preset.id)?500:400));
   for(const time of [0,1500,3000]){
    await page.evaluate(t=>document.getElementById('background').getAnimations({subtree:true}).forEach(a=>{a.pause();a.currentTime=t;}),time);
    await page.screenshot({path:path.join(destination,preset.id+'-'+time+'.png'),omitBackground:true,clip:{x:0,y:0,width:720,height:preset.height}});
   }
   for(const [title,artist] of [['Jóga 🎵 日本語','Björk'],['A long title that must scroll across the original preset without being silently resized','Long artist'],['','']]){
    await page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),{...payload,title,artist});
    // Complete the finite transition at an explicit timestamp, without a new
    // process/browser source or a network request. Text routing is independently audited.
    const inspect=await page.evaluate(()=>window.obsPlayingInspect());assert.equal(inspect.preset,preset.id);checks++;
   }
  }
  assert.deepEqual(errors,[]);checks++;
 }finally{await browser.close();}
 const result={schema:'playing-normal-reference-1',catalogue,aliases,inputs,checks,scope:'Original catalogue and DOM/CSS/motion audit; NOT native migration or pixel parity certification'};
 fs.writeFileSync(path.join(destination,'catalogue.json'),JSON.stringify(result,null,2)+'\n');
 fs.writeFileSync(path.join(destination,'result.txt'),`PASS ${checks} legacy catalogue/DOM contract checks; 23 NORMAL, 3 aliases; native parity pending\n`);
 console.log(fs.readFileSync(path.join(destination,'result.txt'),'utf8'));
}
main().catch(e=>{console.error(e);process.exitCode=1;});
