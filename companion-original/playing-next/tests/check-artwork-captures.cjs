// SPDX-License-Identifier: MIT
const fs=require('fs'),path=require('path');
const sharp=require('C:/NEXUS-Development/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp');
(async()=>{
 const root=process.argv[2],out=process.argv[3];const results=[];
 for(const label of ['base','red','blue','disabled','normal','broken','resumed']){
  const {data,info}=await sharp(path.join(root,`artwork-capture-${label}.png`)).ensureAlpha().raw().toBuffer({resolveWithObject:true});
  const at=(60*info.width+550)*4,rgba=Array.from(data.subarray(at,at+4));
  const image=['red','normal','resumed'].includes(label)?'red':label==='blue'?'blue':'none';
  const pass=image==='red'?rgba[0]>=250&&rgba[1]<=5&&rgba[2]<=5&&rgba[3]===255:image==='blue'?rgba[2]>=250&&rgba[0]<=5&&rgba[1]<=5&&rgba[3]===255:rgba[3]===0;
  results.push({label,width:info.width,height:info.height,rgba,expected:image,pass});
 }
 fs.writeFileSync(out,JSON.stringify({pass:results.every(r=>r.pass),results},null,2));
 if(!results.every(r=>r.pass))throw Error('Artwork rendered pixels mismatch');
 const panels=[];
 for(let i=0;i<results.length;i++)panels.push({input:await sharp(path.join(root,`artwork-capture-${results[i].label}.png`)).resize({width:640}).png().toBuffer(),left:0,top:i*300});
 await sharp({create:{width:640,height:2100,channels:4,background:'#102030'}}).composite(panels).png().toFile(out.replace(/\.json$/,'.png'));
 console.log('7/7 artwork pixel captures PASS');
})();
