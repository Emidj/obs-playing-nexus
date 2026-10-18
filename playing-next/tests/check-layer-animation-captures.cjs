// SPDX-License-Identifier: MIT
const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{
 const root=process.argv[2],out=process.argv[3];const images={};
 for(const label of ['base','float-a','float-b','breathe-a','breathe-b','static-again'])images[label]=await sharp(path.join(root,`motion-capture-${label}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
 const regions={title:[0,0,490,55],artist:[0,60,490,55],artwork:[490,0,150,120]};const cases=[];
 for(const profile of ['float','breathe'])for(const [layer,[x,y,w,h]]of Object.entries(regions)){
  const a=images[profile+'-a'],b=images[profile+'-b'];let changed=0;
  if(a.info.width!==b.info.width||a.info.height!==b.info.height)throw Error('Canvas changed');
  for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++){const p=(j*a.info.width+i)*4;if(a.data.subarray(p,p+4).compare(b.data.subarray(p,p+4)))changed++;}
  cases.push({profile,layer,changed,pass:changed>10});
 }
 cases.push({profile:'default',layer:'all',pass:images.base.data.equals(images['static-again'].data)});
 for(const preset of ['prism','cantilever']){
  const a=await sharp(path.join(root,`motion-capture-${preset}-text-a.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  const b=await sharp(path.join(root,`motion-capture-${preset}-text-b.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});let changed=0;
  if(a.info.width!==b.info.width||a.info.height!==b.info.height)throw Error('NORMAL geometry changed');
  for(let y=0;y<a.info.height;y++)for(let x=0;x<490;x++){const p=(y*a.info.width+x)*4;if(a.data.subarray(p,p+4).compare(b.data.subarray(p,p+4)))changed++;}
  cases.push({profile:preset,layer:'isolated-text',changed,pass:changed>10});
 }
 const pass=cases.every(c=>c.pass);fs.writeFileSync(out,JSON.stringify({pass,cases},null,2));if(!pass)throw Error('Independent layer pixel gate failed');
 console.log('9/9 independent layer/default/NORMAL text pixel gates PASS');
})();
