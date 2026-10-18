// Readiness, metadata-independent decoration and actual scrolling reference.
const fs=require('fs'),path=require('path'),sharp=require('sharp'),crypto=require('crypto');
async function main(){
 const [directory,report]=process.argv.slice(2);if(!report)throw Error('Capture directory and report required');
 const document=JSON.parse(fs.readFileSync(path.join(directory,'glyph-reference-cases.json')));
 if(document.schema!=='playing-terminal-scene-reference-1'||document.cases.length!==12)throw Error('Incomplete Terminal reference');
 const rows=[],images=new Map();
 for(const item of document.cases){
  if(item.state.preset!=='terminal'||item.width!==400||item.height!==210||item.captureHeight!==212)throw Error('Unexpected fixture');
  for(const matte of ['transparent','black','white']){
   const file=path.join(directory,`${item.id}-${matte}.png`),{data,info}=await sharp(file).ensureAlpha().raw().toBuffer({resolveWithObject:true});
   if(info.width!==400||info.height!==212)throw Error('Wrong capture dimensions');
   const pixel=(x,y)=>data.subarray((y*400+x)*4,(y*400+x+1)*4);
   if(!pixel(399,211).equals(Buffer.from([255,0,255,255])))throw Error(`Missing fonts-ready marker: ${item.id}`);
   // Original opaque border remains fixed while title, cursor and scan move.
   if(!pixel(20,20).equals(Buffer.from([100,186,131,255])))throw Error(`Missing original Terminal border: ${item.id}`);
   const crop=await sharp(file).extract({left:60,top:70,width:180,height:24}).ensureAlpha().raw().toBuffer();
   const titleHash=crypto.createHash('sha256').update(crop).digest('hex');
   rows.push({id:item.id,matte,ready:true,border:true,titleHash});
   if(matte==='black')images.set(item.id,crop);
  }
 }
 if(images.get('terminal-long-0').equals(images.get('terminal-long-10500')))throw Error('Long title did not scroll');
 if(images.get('terminal-short-0').equals(images.get('terminal-unicode-0')))throw Error('Metadata fixtures did not reach original renderer');
 const summary={scope:'Original full Terminal CEF only; native scene parity pending',cases:12,captures:36,ready:36,border:36,longScrollingObserved:true,distinctMetadataObserved:true,rows};
 fs.writeFileSync(report,JSON.stringify(summary,null,2)+'\n');console.log('PASS 36 ready full Terminal captures, fixed border, metadata and scrolling');
}
main().catch(error=>{console.error(error);process.exitCode=1});
