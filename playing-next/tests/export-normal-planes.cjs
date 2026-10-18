// Build-time extraction of authored planes. Chromium is never a product dependency.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert');
const {pathToFileURL}=require('url'),{chromium}=require('playwright'),sharp=require('sharp');
const root=path.resolve(__dirname,'../..'),destination=path.join(root,'work','phase12-native-planes');
const reference=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12-reference/catalogue.json'),'utf8'));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex').toUpperCase();
const gcd=(a,b)=>b?gcd(b,a%b):a;
const period=values=>values.reduce((a,b)=>a/gcd(a,b)*b,1);
async function crop(buffer){
 const {data,info}=await sharp(buffer).ensureAlpha().raw().toBuffer({resolveWithObject:true});
 let left=info.width,top=info.height,right=-1,bottom=-1;
 for(let y=0;y<info.height;y++)for(let x=0;x<info.width;x++)if(data[(y*info.width+x)*4+3]){left=Math.min(left,x);right=Math.max(right,x);top=Math.min(top,y);bottom=Math.max(bottom,y);}
 if(right<0)return null;
 return {x:left,y:top,png:await sharp(buffer).extract({left,top,width:right-left+1,height:bottom-top+1}).png().toBuffer()};
}
async function main(){
 if(process.argv.includes('--banks')){
  const gate=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12-reference/plane-parity.json'),'utf8'));
  assert(gate.allPass===true,'Candidate decomposition has not passed visual parity; no native packs may be generated');
 }
 fs.mkdirSync(destination,{recursive:true});
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const exported=[];
 try{
  const page=await browser.newPage({viewport:{width:720,height:500},deviceScaleFactor:1});
  await page.goto(pathToFileURL(path.join(root,'obs-playing/designs.html')).href);
  await page.evaluate(()=>{window.requestAnimationFrame=()=>0;});
  for(const preset of reference.catalogue.filter(p=>p.renderer==='legacy-local-DOM')){
   if(process.argv[2]&&process.argv[2]!=='--all'&&process.argv[2]!==preset.id)continue;
   await page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),{preset:preset.id,width:720,original:true,title:'Lipstick',artist:'Alaia & Gallo',label:'NOW PLAYING',visible:true,gap:12,speed:40,pause:2,opacity:1,transition:'fade',duration:800,restart:0});
   await page.evaluate(()=>document.fonts.ready);
   const description=await page.evaluate(()=>{
    const background=document.getElementById('background'),nodes=[background,...background.querySelectorAll('*')];
    nodes.forEach((e,i)=>{e.dataset.exportIndex=i;});
    const animations=background.getAnimations({subtree:true});animations.forEach(a=>{a.pause();a.currentTime=0;});
    const targets=[...new Set(animations.map(a=>a.effect.target))];
    const outer=targets.filter(e=>!targets.some(other=>other!==e&&other.contains(e)));
    const groups=outer.map(e=>({index:Number(e.dataset.exportIndex),nodes:nodes.filter(n=>n===e||e.contains(n)).map(n=>Number(n.dataset.exportIndex)),durations:animations.filter(a=>a.effect.target===e||e.contains(a.effect.target)).map(a=>a.effect.getTiming().duration),lead:Math.max(0,...animations.filter(a=>a.effect.target===e||e.contains(a.effect.target)).map(a=>a.effect.getTiming().delay))})).sort((a,b)=>a.index-b.index);
    const intervals=[];let used=new Set();
    for(const group of groups){
     const statics=nodes.filter(n=>Number(n.dataset.exportIndex)<group.index&&!used.has(n)&&!groups.some(g=>g.nodes.includes(Number(n.dataset.exportIndex))));
     if(statics.length)intervals.push({kind:'static',nodes:statics.map(n=>Number(n.dataset.exportIndex))});statics.forEach(n=>used.add(n));
     intervals.push({kind:'motion',...group});
    }
    const last=nodes.filter(n=>!used.has(n)&&!groups.some(g=>g.nodes.includes(Number(n.dataset.exportIndex))));if(last.length)intervals.push({kind:'static',nodes:last.map(n=>Number(n.dataset.exportIndex))});
    const text=[...document.querySelectorAll('#foreground .run')].map(e=>{
     const runStyle=getComputedStyle(e),motion=e.parentElement;
     motion.style.position='relative';
     const point=(x,y)=>{const p=document.createElement('span');p.style.cssText=`position:absolute;left:${x}px;top:${y}px;width:0;height:0`;motion.append(p);const r=p.getBoundingClientRect();p.remove();return {x:r.x,y:r.y};};
     const origin=point(0,0),x=point(1,0),y=point(0,1);
     const baseline=document.createElement('span');baseline.style.cssText='display:inline-block;width:0;height:0;vertical-align:baseline';e.append(baseline);const r=baseline.getBoundingClientRect();baseline.remove();
     const a=x.x-origin.x,b=x.y-origin.y,c=y.x-origin.x,d=y.y-origin.y,det=a*d-b*c;
     const baselineLocal=(-b*(r.x-origin.x)+a*(r.y-origin.y))/det;
     return {matrix:[a,b,c,d,origin.x,origin.y],baseline:baselineLocal,windowWidth:motion.parentElement.clientWidth,windowHeight:motion.parentElement.clientHeight,fontFamily:runStyle.fontFamily,fontSize:parseFloat(runStyle.fontSize),fontWeight:Number(runStyle.fontWeight),italic:runStyle.fontStyle==='italic',letterSpacing:parseFloat(runStyle.letterSpacing)||0,color:runStyle.color,stroke:runStyle.webkitTextStroke,textShadow:runStyle.textShadow,upper:runStyle.textTransform==='uppercase',center:e.closest('.gum')!==null};
    });
    // A child cannot be rasterized independently when an ancestor applies
    // opacity/filter to the combined subtree. Preserve this authored dependency
    // in the audit instead of treating a failed sum of PNGs as native artwork.
    const paintDependencies=nodes.map(e=>{
     const s=getComputedStyle(e),reasons=[];
     if(s.filter!=='none')reasons.push('filter');
     if(Number(s.opacity)!==1)reasons.push('opacity');
     if(s.mixBlendMode!=='normal')reasons.push('blend');
     if(s.isolation==='isolate')reasons.push('isolation');
     if(s.transform!=='none')reasons.push('transform');
     if(s.clipPath!=='none')reasons.push('clip');
     if(s.overflowX!=='visible'||s.overflowY!=='visible')reasons.push('overflow');
     return {index:Number(e.dataset.exportIndex),selector:e.id?'#'+e.id:'.'+[...e.classList].join('.'),
      reasons,filter:s.filter,opacity:s.opacity,blend:s.mixBlendMode,isolation:s.isolation,
      transform:s.transform,transformOrigin:s.transformOrigin,zIndex:s.zIndex,position:s.position,clip:s.clipPath,
      descendants:nodes.filter(n=>e!==n&&e.contains(n)).map(n=>Number(n.dataset.exportIndex))};
    }).filter(e=>e.reasons.length);
    const authoredAnimations=animations.map(a=>{
     const timing=a.effect.getTiming();timing.iterations=Number.isFinite(timing.iterations)?timing.iterations:'infinite';
     return {name:a.animationName||null,target:Number(a.effect.target.dataset.exportIndex),
      pseudo:a.effect.pseudoElement||null,timing,keyframes:a.effect.getKeyframes(),
      dependencyAncestors:paintDependencies.filter(p=>p.index===Number(a.effect.target.dataset.exportIndex)||p.descendants.includes(Number(a.effect.target.dataset.exportIndex))).map(p=>p.index)};
    });
    return {intervals,text,paintDependencies,authoredAnimations,rootTransform:getComputedStyle(document.getElementById('op-design-lab')).transform};
   });
   const clip={x:0,y:0,width:720,height:preset.height};
   const sampleTimes=[0,1500,3000];
   if(process.argv.includes('--full-motion-audit')){
    for(const animation of description.authoredAnimations){
     const duration=Number(animation.timing.duration),delay=Number(animation.timing.delay);
     assert(Number.isFinite(duration)&&duration>0&&duration<=60000&&Number.isFinite(delay)&&Math.abs(delay)<=60000,'Invalid authored animation timing');
     assert(animation.keyframes.every(k=>Number.isFinite(k.computedOffset)&&k.computedOffset>=0&&k.computedOffset<=1),'Invalid authored keyframe offset');
     const offsets=[...new Set([0,.25,.5,.75,1,...animation.keyframes.map(k=>k.computedOffset)])].sort((a,b)=>a-b);
     for(let i=0;i<offsets.length;i++){
      for(const offset of [offsets[i],...(i?[ (offsets[i-1]+offsets[i])/2 ]:[])]){
       let time=delay+duration*offset;while(time<0)time+=duration;
       sampleTimes.push(Math.round(time));
      }
     }
    }
   }
   const sampleTimesMs=[...new Set(sampleTimes)].sort((a,b)=>a-b);
   assert(sampleTimesMs.length<=256&&sampleTimesMs.every(t=>Number.isFinite(t)&&t>=0&&t<=120000),'Motion audit exceeds time/sample bounds');
   const foregroundOnly=await page.addStyleTag({content:'#foreground{visibility:hidden!important}'});
   const originals=[];
   for(const time of sampleTimesMs){
    await page.evaluate(t=>document.getElementById('background').getAnimations({subtree:true}).forEach(a=>{a.pause();a.currentTime=t;}),time);
    originals.push(await page.screenshot({omitBackground:true,clip}));
   }
   const compositor=process.argv.includes('--force-compositing')?await page.addStyleTag({content:'#background,#background *{will-change:transform!important}'}):
    process.argv.includes('--preserve-root-compositing')?await page.addStyleTag({content:'#background>.overlay{will-change:transform!important}'}):
    process.argv.includes('--preserve-layer-compositing')?await page.addStyleTag({content:'#background{will-change:transform!important}'}):null;
   const visibility=await page.addStyleTag({content:'#background *{visibility:hidden!important}#background .export-visible{visibility:visible!important}#background .run{visibility:hidden!important}'});
   const capture=async(nodes,time)=>{
    await page.evaluate(({nodes,time})=>{const wanted=new Set(nodes);document.querySelectorAll('#background [data-export-index]').forEach(e=>e.classList.toggle('export-visible',wanted.has(Number(e.dataset.exportIndex))));document.getElementById('background').classList.toggle('export-visible',wanted.has(0));document.getElementById('background').getAnimations({subtree:true}).forEach(a=>{a.pause();a.currentTime=time;});},{nodes,time});
    return page.screenshot({omitBackground:true,clip});
   };
   const images=[],imageIds=new Map(),planes=[];
   const register=async(buffer)=>{const result=await crop(buffer);if(!result)return {image:-1,x:0,y:0};const key=hash(result.png);if(!imageIds.has(key)){imageIds.set(key,images.length);images.push(result.png);}return {image:imageIds.get(key),x:result.x,y:result.y};};
   for(const interval of description.intervals){
    const cycle=interval.kind==='motion'?period(interval.durations.map(Math.round)):0;
    const lead=interval.kind==='motion'?Math.ceil(interval.lead*30/1000):0;
    assert(cycle<=60000,'Unexpected merged motion period: '+preset.id);
    const frames=[];const count=interval.kind==='motion'?lead+Math.ceil(cycle*30/1000):1;
    // Initially validate the exact decomposition at authored timestamps. Full
    // banks are generated only with --banks after these checks pass.
    const times=process.argv.includes('--banks')?Array.from({length:count},(_,i)=>i*1000/30):sampleTimesMs;
    for(const time of times)frames.push(await register(await capture(interval.nodes,time)));
    planes.push({kind:interval.kind,periodMs:cycle,leadFrames:lead,frames,nodes:interval.nodes});
   }
   const parity=[];
   if(!process.argv.includes('--banks'))for(let sample=0;sample<sampleTimesMs.length;sample++){
    const time=sampleTimesMs[sample];
    const original=originals[sample];
    const layers=planes.map(p=>p.frames[sample]).filter(p=>p.image>=0).map(p=>({input:images[p.image],left:p.x,top:p.y}));
    const composed=await sharp({create:{width:720,height:preset.height,channels:4,background:'#00000000'}}).composite(layers).png().toBuffer();
    const a=await sharp(original).raw().toBuffer(),b=await sharp(composed).raw().toBuffer();let total=0,bad=0,max=0;
    for(let i=0;i<a.length;i++){const delta=Math.abs(a[i]-b[i]);total+=delta;if(delta>2)bad++;max=Math.max(max,delta);}
    parity.push({time,meanChannelError:total/a.length,channelsOver2:bad,maxChannelError:max});
    fs.writeFileSync(path.join(destination,preset.id+'-composed-'+time+'.png'),composed);
    fs.writeFileSync(path.join(destination,preset.id+'-background-'+time+'.png'),original);
   }
   await visibility.evaluate(e=>e.remove());
   if(compositor)await compositor.evaluate(e=>e.remove());
   await foregroundOnly.evaluate(e=>e.remove());
   if(process.argv.includes('--debug-planes'))for(const [index,png] of images.entries())fs.writeFileSync(path.join(destination,preset.id+'-plane-'+index+'.png'),png);
   const captureMode=process.argv.includes('--force-compositing')?'all':process.argv.includes('--preserve-root-compositing')?'panel':process.argv.includes('--preserve-layer-compositing')?'layer':'unchanged';
   const header={schema:'playing-native-normal-planes-1',captureMode,sampleTimesMs,id:preset.id,name:preset.name,width:720,height:preset.height,fps:30,rootTransform:description.rootTransform,text:description.text,paintDependencies:description.paintDependencies,authoredAnimations:description.authoredAnimations,planes,inputs:reference.inputs,imageCount:images.length,parity};
   fs.writeFileSync(path.join(destination,preset.id+'.json'),JSON.stringify(header,null,2)+'\n');
   if(process.argv.includes('--banks')){
    const json=Buffer.from(JSON.stringify(header)),u32=n=>{const b=Buffer.alloc(4);b.writeUInt32LE(n);return b;};
    const bytes=Buffer.concat([Buffer.from('PNORM12\0'),u32(json.length),json,u32(images.length),...images.flatMap(b=>[u32(b.length),b])]);
    assert(bytes.length<=64*1024*1024,'Compressed pack exceeds resource bound');
    fs.writeFileSync(path.join(destination,preset.id+'.pn12'),bytes);header.sha256=hash(bytes);header.bytes=bytes.length;
   }
   exported.push({id:preset.id,captureMode,planes:planes.length,images:images.length,bytes:header.bytes||0,parity});
   console.log(JSON.stringify(exported[exported.length-1]));
  }
 }finally{await browser.close();}
 const resultName=exported.length===21?'result.json':`result-${exported[0]?.id||'empty'}-experiment.json`;
 fs.writeFileSync(path.join(destination,resultName),JSON.stringify(exported,null,2)+'\n');
}
main().catch(e=>{console.error(e);process.exitCode=1;});
