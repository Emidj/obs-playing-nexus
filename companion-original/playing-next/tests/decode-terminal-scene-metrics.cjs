const fs=require('fs'),path=require('path'),sharp=require('sharp');
async function main(){
 const [directory,report]=process.argv.slice(2);if(!report)throw Error('CEF capture directory and report required');
 const document=JSON.parse(fs.readFileSync(path.join(directory,'glyph-reference-cases.json'))),results=[];
 for(const item of document.cases){
  const pixels=await sharp(path.join(directory,item.id+'-black.png')).extract({left:0,top:210,width:128,height:1}).ensureAlpha().raw().toBuffer();
  const values=[];
  if(item.encodedMetricsVersion===2){
   if(!pixels.subarray(0,4).equals(Buffer.from([80,78,88,255])))throw Error('Missing geometry version marker');
   for(let i=0;i<78;i++){const at=(i+1)*4;if(pixels[at+3]!==255)throw Error('Incomplete geometry');values.push((pixels[at]+pixels[at+1]*256+pixels[at+2]*65536-4194304)/64);}
  }else for(let i=0;i<56;i++){if(pixels[i*4+2]!==137||pixels[i*4+3]!==255)throw Error('Missing CEF geometry marker');values.push((pixels[i*4]+pixels[i*4+1]*256)/64);}
  const bars=Array.from({length:12},(_,i)=>values.slice(i*4,i*4+4));
  if(bars.some(b=>b[2]!==1525/64))throw Error('CEF flex behavior changed: review target contract');
  results.push({id:item.id,bars,onAir:values.slice(48,52),prompt:values.slice(52,56),...(item.encodedMetricsVersion===2?{cursor:values.slice(56,60),rows:Array.from({length:3},(_,i)=>({box:values.slice(60+i*4,64+i*4),full:values[72+i*2],client:values[73+i*2]}))}:{})});
 }
 fs.writeFileSync(report,JSON.stringify({scope:'Geometry measured inside actual OBS CEF127, encoded below visible scene',cases:results.length,results},null,2)+'\n');console.log('PASS actual CEF geometry decoded for '+results.length+' scenes');
}
main().catch(error=>{console.error(error);process.exitCode=1});
