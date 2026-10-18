// Build/test only. Browser timing is the independent reference for the native
// NORMAL keyframe sampler; this does not certify scene geometry or GPU parity.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'../..');
async function main(){
 const folder=path.join(root,'outputs/phase12e-dom-contracts');
 const contracts=fs.readdirSync(folder).filter(f=>f.endsWith('.json')).map(f=>JSON.parse(fs.readFileSync(path.join(folder,f)))).filter(c=>c.schema==='playing-native-normal-planes-1');
 assert(contracts.length===21,'All original DOM contracts required');
 for(const contract of contracts)for(const input of contract.inputs){
  assert(!input.file.includes('/')&&!input.file.includes('\\')&&!input.file.includes('..'),'Invalid provenance path');
  const actual=crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'obs-playing',input.file))).digest('hex').toUpperCase();
  assert(actual===input.sha256,'Original source changed since authored audit: '+input.file);
 }
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const rows=[],inventory=[];const easingIds={'linear':0,'ease-in-out':1,'ease-out':2};
 try{
  const page=await browser.newPage();
  await page.setContent('<div id="probe" style="opacity:0;width:1px;height:1px"></div>');
  for(const contract of contracts)for(const animation of contract.authoredAnimations){
   const timing=animation.timing,frames=animation.keyframes;
   assert(timing.direction==='normal'&&timing.fill==='none'&&timing.easing==='linear'&&timing.iterations==='infinite'&&timing.iterationStart===0&&timing.endDelay===0,'Unsupported authored effect timing');
   assert(frames.length>=2&&frames.length<=64&&frames[0].computedOffset===0&&frames.at(-1).computedOffset===1,'Incomplete authored offsets');
   frames.forEach((f,i)=>assert(f.easing in easingIds&&(!i||f.computedOffset>frames[i-1].computedOffset),'Unsupported authored keyframe sequence'));
   const times=[...new Set([0,...contract.sampleTimesMs,...Array.from({length:33},(_,i)=>Math.max(0,timing.delay+timing.duration*i/16)),Math.max(0,timing.delay-0.001),Math.max(0,timing.delay),Math.max(0,timing.delay+timing.duration)])].sort((a,b)=>a-b);
   const samples=await page.evaluate(({timing,frames,times})=>{
    const node=document.getElementById('probe');
    const effect=new KeyframeEffect(node,frames.map((f,i)=>({offset:f.computedOffset,easing:f.easing,opacity:i/(frames.length-1)})),{...timing,iterations:Infinity});
    const instance=new Animation(effect,document.timeline);instance.pause();
    const values=times.map(time=>{instance.currentTime=time;return {time,active:effect.getComputedTiming().progress!==null,opacity:Number(getComputedStyle(node).opacity)};});
    instance.cancel();return values;
   },{timing,frames,times});
   for(const sample of samples)rows.push(['A',sample.time,timing.duration,timing.delay,frames.length,...frames.flatMap(f=>[f.computedOffset,easingIds[f.easing]]),Number(sample.active),sample.opacity].join(' '));
   inventory.push({preset:contract.id,name:animation.name,target:animation.target,pseudo:animation.pseudo,samples:samples.length});
  }
 }finally{await browser.close();}
 const destination=path.join(root,'outputs/phase12f-authored-motion');fs.mkdirSync(destination,{recursive:true});
 fs.writeFileSync(path.join(destination,'browser-reference.txt'),rows.join('\n')+'\n');
 fs.writeFileSync(path.join(destination,'inventory.json'),JSON.stringify({scope:'Timing and easing only; not scene/render parity',presets:21,animations:inventory.length,samples:rows.length,browserVersion:browser.version(),inputs:contracts[0].inputs,inventory},null,2)+'\n');
 console.log(JSON.stringify({presets:21,animations:inventory.length,samples:rows.length}));
}
main().catch(error=>{console.error(error);process.exitCode=1;});
