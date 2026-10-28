// Execute the formulas from the preserved production JavaScript, not a second
// handwritten model. Produces optional golden input for normal-motion-tests.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert');
const root=path.resolve(__dirname,'../..');
const original=fs.readFileSync(path.join(root,'obs-playing/designs.js'),'utf8');
const offset=original.match(/function offset\(seconds,distance,speed,pause\)\{([\s\S]*?)\n\}/);
const clamp=original.match(/const clamp=(.*);/);
const effects=original.match(/const incoming=progress>=\.5,phase=[\s\S]*?if\(cfg.transition==='zoom'\)[^\n]+/);
assert(offset&&clamp&&effects,'Original formulas changed: audit required');
const scroll=new Function('seconds','distance','speed','pause',offset[1]);
const transition=new Function('elapsed','duration','kind',`const clamp=${clamp[1]};const cfg={transition:kind};const progress=clamp(elapsed/clamp(duration,200,2000),0,1);let alpha=1,y=0,zoom=1;${effects[0]}return [alpha,y,zoom,Number(incoming),Number(progress>=1)];`);
const rows=[];
for(const distance of [0,1,24,100,4096])for(const speed of [0,1,25,40])for(const pause of [0,2,5])for(const time of [-1,0,.1,2,4,6,8,12,123.456])
 rows.push(['S',time,distance,speed,pause,scroll(time,distance,speed,pause)].join(' '));
for(const [index,kind] of ['fade','slide','zoom'].entries())for(const duration of [0,200,800,2000,3000])for(const elapsed of [-10,0,50,100,199,200,399,400,600,799,800,1000,2000,4000])
 rows.push(['T',index,elapsed,duration,...transition(elapsed,duration,kind)].join(' '));
const folder=path.join(root,'outputs/phase12-reference');fs.mkdirSync(folder,{recursive:true});
fs.writeFileSync(path.join(folder,'motion-golden.txt'),rows.join('\n')+'\n');
fs.writeFileSync(path.join(folder,'motion-golden.json'),JSON.stringify({source:'obs-playing/designs.js',sha256:crypto.createHash('sha256').update(original).digest('hex'),cases:rows.length,scope:'Formula parity only; renderer, scheduling and metadata transitions remain pending'},null,2)+'\n');
console.log(`${rows.length} cases extracted from original JavaScript`);
