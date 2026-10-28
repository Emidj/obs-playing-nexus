// Test-only reference from preserved DOM. No native parity or migration claim.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const {pathToFileURL}=require('url'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../..');
const destination=path.join(root,'outputs/phase12v-other-normal-text');
async function main(){
 const catalogue=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12-reference/catalogue.json')));
 const presets=catalogue.catalogue.filter(p=>p.renderer==='legacy-local-DOM'&&p.id!=='terminal');
 if(presets.length!==20)throw Error('Expected 20 preserved DOM designs excluding Terminal');
 fs.mkdirSync(destination,{recursive:true});
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const cases=[],errors=[];
 try{
  const page=await browser.newPage({viewport:{width:1800,height:1000}});
  page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>window.requestAnimationFrame=()=>0);
  await page.goto(pathToFileURL(path.join(root,'obs-playing/designs.html')).href);
  for(const preset of presets)for(const [kind,title,artist] of [
   ['short','Lipstick','Alaia & Gallo'],['unicode','Jóga 🎵 日本語','Björk'],
   ['long','Long track '.repeat(40).trimEnd(),'Long artist '.repeat(30).trimEnd()]]){
   await page.evaluate(detail=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail})),
    {preset:preset.id,width:400,gap:12,original:true,title,artist,label:'NOW PLAYING '+kind,visible:true,opacity:1,speed:40,pause:2,transition:'fade',duration:800,restart:0});
   await page.evaluate(()=>document.fonts.ready);
   const rows=await page.evaluate(()=>[...document.querySelectorAll('#foreground .run')].map(run=>{
    const style=getComputedStyle(run),rect=run.getBoundingClientRect();
    return {text:run.textContent,advance:rect.width,height:rect.height,
     family:style.fontFamily,pixelSize:parseFloat(style.fontSize),weight:style.fontWeight,
     fontStyle:style.fontStyle,spacing:style.letterSpacing,lineHeight:style.lineHeight,
     color:style.color,shadow:style.textShadow,stroke:style.webkitTextStroke,
     transform:style.textTransform};
   }));
   if(rows.length!==3||rows.some(r=>!Number.isFinite(r.advance)||r.height<=0))throw Error('Invalid original text: '+preset.id);
   cases.push({preset:preset.id,name:preset.name,kind,width:400,rows});
  }
 }finally{await browser.close()}
 if(errors.length)throw Error(errors.join('\n'));
 const html=fs.readFileSync(path.join(root,'obs-playing/designs.html'),'utf8');
 const inputs=['designs.html','designs.js',...[...html.matchAll(/href="([^"]+\.css)"/g)].map(m=>m[1])]
  .map(file=>({file,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'obs-playing',file))).digest('hex')}));
 fs.writeFileSync(path.join(destination,'reference.json'),JSON.stringify({scope:'20 original DOM text contracts; no native raster/OBS parity',browser:browser.version(),inputs,cases},null,2)+'\n');
 console.log(`${cases.length} original cases / ${cases.length*3} text layers; zero page errors`);
}
main().catch(e=>{console.error(e);process.exitCode=1});
