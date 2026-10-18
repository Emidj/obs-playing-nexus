// Test-only browser: evaluates original CSS rules, including missing properties.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'../..');
async function main(){
 const source=fs.readFileSync(path.join(root,'obs-playing/design-motion.css'),'utf8');
 const browser=await chromium.launch({executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
 try {
  const page=await browser.newPage();
  await page.setContent('<style>#op-design-lab .terminal{position:relative;width:100px;height:10000px}</style><div id="op-design-lab"><div id="background"><div class="terminal"><i class="cursor-block"></i></div></div></div>');
  await page.addStyleTag({content:source});
  const times=[...new Set([...Array.from({length:561},(_,i)=>i*50),...[0,.58,.62,.86,.9,1].flatMap(p=>[Math.max(0,p*14000-.001),p*14000,p*14000+.001])])].sort((a,b)=>a-b);
  const rows=await page.evaluate(times=>{
   const panel=document.querySelector('.terminal'),cursor=document.querySelector('.cursor-block');
   const animations=document.getAnimations();animations.forEach(a=>a.pause());
   if(animations.length!==2)throw Error('Expected both original Terminal animations');
   return times.map(t=>{animations.forEach(a=>a.currentTime=t);const scan=getComputedStyle(panel,'::after');return ['R',t,Number(getComputedStyle(cursor).opacity),parseFloat(scan.top)/100,Number(scan.opacity)].join(' ')});
  },times);
  const destination=path.join(root,'outputs/phase12i-terminal-motion');fs.mkdirSync(destination,{recursive:true});
  fs.writeFileSync(path.join(destination,'browser-reference.txt'),rows.join('\n')+'\n');
  fs.writeFileSync(path.join(destination,'provenance.json'),JSON.stringify({file:'obs-playing/design-motion.css',sha256:crypto.createHash('sha256').update(source).digest('hex'),browser:browser.version(),samples:rows.length,scope:'CSS property timing only; no pixel or integration certification'},null,2)+'\n');
  console.log(rows.length+' original CSS property samples');
 }finally{await browser.close()}
}
main().catch(e=>{console.error(e);process.exitCode=1});
