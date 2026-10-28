// Build/test only: dynamic geometry of the preserved Terminal renderer.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const {pathToFileURL}=require('url'),{chromium}=require('playwright');
const root=path.resolve(__dirname,'../..');
async function main(){
 const destination=path.join(root,'outputs/phase12j-terminal-layout');fs.mkdirSync(destination,{recursive:true});
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 const cases=[];
 try{
  const page=await browser.newPage({viewport:{width:1800,height:1000}});
  await page.addInitScript(()=>{window.requestAnimationFrame=()=>0});
  await page.goto(pathToFileURL(path.join(root,'obs-playing/designs.html')).href);
  for(const width of [400,720,1600])for(const gap of [0,12,80])for(const [id,title,artist] of [
   ['short','Lipstick','Alaia & Gallo'],['unicode','Jóga 🎵 日本語','Björk'],['long','Long track '.repeat(40),'Long artist '.repeat(30)],['empty','','']]){
   // A new label forces an original rebuild rather than a track transition.
   const label='NOW PLAYING '+cases.length;
   await page.evaluate(p=>window.dispatchEvent(new CustomEvent('obsPlayingState',{detail:p})),{preset:'terminal',width,gap,original:true,title,artist,label,visible:true,speed:40,pause:2,opacity:1,transition:'fade',duration:800,restart:0});
   await page.evaluate(()=>document.fonts.ready);
   const original=await page.evaluate(()=>{
    const scale=document.getElementById('op-design-lab').getBoundingClientRect().width/400;
    const rect=e=>{const r=e.getBoundingClientRect();return [r.x/scale,r.y/scale,r.width/scale,r.height/scale]};
    const panel=document.querySelector('#foreground .terminal');
    const rows=[...panel.querySelectorAll('.run')].map(run=>({text:run.textContent,box:rect(run.parentElement.parentElement),full:run.getBoundingClientRect().width/scale,client:run.parentElement.parentElement.clientWidth}));
    return {scale,panel:rect(panel),top:rect(panel.querySelector('.terminal-top')),main:rect(panel.querySelector('.terminal-main')),prompt:rect(panel.querySelector('.terminal-prompt')),cursor:rect(panel.querySelector('.cursor-block')),bottom:rect(panel.querySelector('.terminal-bottom')),bars:[...panel.querySelectorAll('.terminal-bottom i')].map(rect),rows};
   });
   cases.push({width,gap,id,title,artist,label,...original});
  }
 }finally{await browser.close()}
 fs.writeFileSync(path.join(destination,'reference.json'),JSON.stringify(cases,null,2)+'\n');
 const files=JSON.parse(fs.readFileSync(path.join(root,'outputs/phase12e-dom-contracts/terminal.json'),'utf8')).inputs.map(input=>input.file);
 fs.writeFileSync(path.join(destination,'provenance.json'),JSON.stringify({browser:browser.version(),cases:cases.length,inputs:files.map(file=>({file,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'obs-playing',file))).digest('hex')})),scope:'Original dynamic layout; not native pixel parity'},null,2)+'\n');
 console.log(cases.length+' original Terminal layouts');
}
main().catch(e=>{console.error(e);process.exitCode=1});
