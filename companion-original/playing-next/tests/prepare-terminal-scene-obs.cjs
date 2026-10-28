// Full preserved DOM/CSS, with a test clock; never a product dependency.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'../..'),destination=path.resolve(process.argv[2]||'');
if(!destination.startsWith(path.join(root,'work')+path.sep)||!fs.statSync(destination).isDirectory())throw Error('Owned work directory required');
const originals=path.resolve(destination,'../../obs-playing');
const html=fs.readFileSync(path.join(originals,'designs.html'),'utf8');
if(!html.includes('<script src="designs.js"></script>'))throw Error('Original script entry changed');
const clock=`(()=>{let now=0,id=0;const callbacks=new Map();Object.defineProperty(performance,'now',{value:()=>now});window.requestAnimationFrame=cb=>{callbacks.set(++id,cb);return id};window.cancelAnimationFrame=key=>callbacks.delete(key);window.auditTick=time=>{if(time<now)throw Error('Non-monotonic clock');now=time;const pending=[...callbacks.values()];callbacks.clear();pending.forEach(cb=>cb(now))}})();`;
fs.writeFileSync(path.join(originals,'terminal-reference-clock.js'),clock);
const cases=[];
for(const [name,title,artist] of [['short','Lipstick','Alaia & Gallo'],['unicode','Jóga 🎵 日本語','Björk'],['long','Long track '.repeat(40).trimEnd(),'Long artist '.repeat(30).trimEnd()]]){
 for(const time of [0,2000,8500,10500]){
  const id=`terminal-${name}-${time}`,state={preset:'terminal',width:400,gap:12,original:true,title,artist,label:'NOW PLAYING',visible:true,opacity:1,speed:40,pause:2,transition:'fade',duration:800,restart:0};
  const driver=`document.fonts.ready.then(()=>{window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:${JSON.stringify(state)}}));return document.fonts.ready}).then(()=>{window.auditTick(${time});document.getAnimations().forEach(a=>{a.pause();a.currentTime=${time}});const marker=document.createElement('div');marker.style.cssText='position:absolute;right:0;bottom:0;width:1px;height:1px;background:#ff00ff';document.body.append(marker);window.terminalReferenceState=window.obsPlayingInspect()});`;
  const driverName=id+'-driver.js';fs.writeFileSync(path.join(originals,driverName),driver);
  const file=path.join(originals,id+'.html');
  fs.writeFileSync(file,html.replace('<script src="designs.js"></script>','<script src="terminal-reference-clock.js"></script><script src="designs.js"></script><script src="'+driverName+'"></script>'));
  cases.push({id,kind:'terminal-scene',width:400,height:210,captureHeight:212,localFile:file.replaceAll('\\','/'),state,time});
 }
}
fs.writeFileSync(path.join(destination,'glyph-reference-cases.json'),JSON.stringify({schema:'playing-terminal-scene-reference-1',cases},null,2)+'\n');
const files=['designs.html',...new Set([...html.matchAll(/(?:href|src)="([^"/]+\.(?:css|js))"/g)].map(match=>match[1]))];
fs.writeFileSync(path.join(destination,'terminal-reference-provenance.json'),JSON.stringify({scope:'Full original Terminal, controlled JS and CSS clocks; no native parity claim',cases:12,inputs:files.map(file=>({file,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'obs-playing',file))).digest('hex')}))},null,2)+'\n');
console.log('Prepared 12 full original Terminal scenes');
